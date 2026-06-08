#include <algorithm>
#include <cmath>
#include <ctime>
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include <direct.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "app_config.hpp"
#include "annotation_loader.hpp"
#include "frame_source.hpp"
#include "kcftracker.hpp"

namespace {

const std::string DEFAULT_CONFIG_PATH =
    "D:\\code\\cpp\\vs\\myKCF\\joaofaro\\joao\\configs\\baseline_hog31.yaml";

class TeeStreamBuffer : public std::streambuf {
public:
    TeeStreamBuffer(std::streambuf* first, std::streambuf* second)
        : first_(first), second_(second)
    {
    }

protected:
    int overflow(int ch)
    {
        if (ch == traits_type::eof()) {
            return traits_type::not_eof(ch);
        }

        const int firstResult = first_->sputc(static_cast<char>(ch));
        const int secondResult = second_->sputc(static_cast<char>(ch));
        if (firstResult == traits_type::eof() ||
            secondResult == traits_type::eof()) {
            return traits_type::eof();
        }
        return ch;
    }

    int sync()
    {
        const int firstResult = first_->pubsync();
        const int secondResult = second_->pubsync();
        return firstResult == 0 && secondResult == 0 ? 0 : -1;
    }

private:
    std::streambuf* first_;
    std::streambuf* second_;
};

class ScopedOutputTee {
public:
    explicit ScopedOutputTee(std::ofstream& logFile)
        : originalCout_(std::cout.rdbuf()),
          originalCerr_(std::cerr.rdbuf()),
          coutTee_(originalCout_, logFile.rdbuf()),
          cerrTee_(originalCerr_, logFile.rdbuf())
    {
        std::cout.rdbuf(&coutTee_);
        std::cerr.rdbuf(&cerrTee_);
    }

    ~ScopedOutputTee()
    {
        std::cout.flush();
        std::cerr.flush();
        std::cout.rdbuf(originalCout_);
        std::cerr.rdbuf(originalCerr_);
    }

private:
    std::streambuf* originalCout_;
    std::streambuf* originalCerr_;
    TeeStreamBuffer coutTee_;
    TeeStreamBuffer cerrTee_;
};

double centerLocationError(const cv::Rect2f& predicted, const cv::Rect2f& truth)
{
    const cv::Point2f predictedCenter(predicted.x + predicted.width * 0.5f,
                                      predicted.y + predicted.height * 0.5f);
    const cv::Point2f truthCenter(truth.x + truth.width * 0.5f,
                                  truth.y + truth.height * 0.5f);
    const double dx = static_cast<double>(predictedCenter.x - truthCenter.x);
    const double dy = static_cast<double>(predictedCenter.y - truthCenter.y);
    return std::sqrt(dx * dx + dy * dy);
}

double intersectionOverUnion(const cv::Rect2f& predicted, const cv::Rect2f& truth)
{
    const float left = std::max(predicted.x, truth.x);
    const float top = std::max(predicted.y, truth.y);
    const float right = std::min(predicted.x + predicted.width,
                                 truth.x + truth.width);
    const float bottom = std::min(predicted.y + predicted.height,
                                  truth.y + truth.height);

    const double intersectionWidth = std::max(0.0f, right - left);
    const double intersectionHeight = std::max(0.0f, bottom - top);
    const double intersectionArea = intersectionWidth * intersectionHeight;
    const double predictedArea =
        static_cast<double>(predicted.width) * static_cast<double>(predicted.height);
    const double truthArea =
        static_cast<double>(truth.width) * static_cast<double>(truth.height);
    const double unionArea = predictedArea + truthArea - intersectionArea;

    if (unionArea <= 0.0) {
        return 0.0;
    }
    return intersectionArea / unionArea;
}

cv::Rect clampRectToFrame(const cv::Rect2f& box, const cv::Size& frameSize)
{
    const cv::Rect2f frameBounds(0.0f, 0.0f,
                                 static_cast<float>(frameSize.width),
                                 static_cast<float>(frameSize.height));
    const cv::Rect2f clipped = box & frameBounds;
    if (clipped.width <= 0.0f || clipped.height <= 0.0f) {
        return cv::Rect();
    }

    return cv::Rect(cvRound(clipped.x), cvRound(clipped.y),
                    cvRound(clipped.width), cvRound(clipped.height));
}

cv::Rect rect2fToRect(const cv::Rect2f& box)
{
    return cv::Rect(cvRound(box.x), cvRound(box.y),
                    cvRound(box.width), cvRound(box.height));
}

void printFrameMetrics(int frameIndex,
                       double loadTimeMs,
                       double kcfTimeMs,
                       bool hasTruth,
                       double cle,
                       double iou)
{
    std::cout << "frame=" << frameIndex
              << ", load_ms=" << loadTimeMs
              << ", kcf_ms=" << kcfTimeMs
              << ", target=" << (hasTruth ? "valid" : "missing");
    if (hasTruth) {
        std::cout << ", CLE=" << cle
                  << ", IoU=" << iou;
    }
    std::cout << std::endl;
}

int effectiveFeatureChannels(const FeatureConfig& features)
{
    int channels = 0;
    if (features.hog_enabled) {
        channels += features.hog_channels;
    }
    if (features.cn_enabled) {
        channels += features.cn_channels;
    }
    return channels;
}

std::string normalizeSlashes(const std::string& path)
{
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

std::string dirname(const std::string& path)
{
    const std::string normalized = normalizeSlashes(path);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return normalized.substr(0, 1);
    }
    return normalized.substr(0, slash);
}

std::string basename(const std::string& path)
{
    const std::string normalized = normalizeSlashes(path);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) {
        return normalized;
    }
    return normalized.substr(slash + 1);
}

std::string joinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return right;
    }
    const char last = left[left.size() - 1];
    if (last == '/' || last == '\\') {
        return left + right;
    }
    return left + "/" + right;
}

std::string projectRootFromConfigPath(const std::string& configPath)
{
    const std::string configDir = dirname(configPath);
    if (basename(configDir) == "configs") {
        return dirname(configDir);
    }
    return ".";
}

bool createDirectoryIfMissing(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    if (_mkdir(path.c_str()) == 0) {
        return true;
    }
    return errno == EEXIST;
}

bool ensureDirectory(const std::string& path)
{
    const std::string normalized = normalizeSlashes(path);
    std::string current;
    size_t start = 0;

    if (normalized.size() >= 2 && normalized[1] == ':') {
        current = normalized.substr(0, 2);
        start = 2;
        if (normalized.size() > 2 && normalized[2] == '/') {
            current += "/";
            start = 3;
        }
    }

    while (start < normalized.size()) {
        const size_t slash = normalized.find('/', start);
        const std::string part = normalized.substr(
            start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty()) {
            current = current.empty() ? part : joinPath(current, part);
            if (!createDirectoryIfMissing(current)) {
                return false;
            }
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return true;
}

bool fileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.is_open();
}

std::string makeRunTimestamp()
{
    const std::time_t now = std::time(NULL);
    std::tm localTime;
    localtime_s(&localTime, &now);

    std::ostringstream text;
    text << std::put_time(&localTime, "%Y%m%d_%H%M");
    return text.str();
}

std::string makeUniqueRunId(const std::string& outputDataDir,
                            const std::string& outputVideoDir,
                            const std::string& timestamp)
{
    for (int suffix = 0; suffix < 1000; ++suffix) {
        std::ostringstream runId;
        runId << timestamp;
        if (suffix > 0) {
            runId << "_" << std::setw(2) << std::setfill('0') << suffix;
        }

        const std::string id = runId.str();
        if (!fileExists(joinPath(outputDataDir, id + ".txt")) &&
            !fileExists(joinPath(outputVideoDir, id + ".mp4"))) {
            return id;
        }
    }
    return timestamp;
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc > 2) {
        std::cerr << "Usage: RunKCF.exe [config_path]" << std::endl;
        return 1;
    }

    const std::string configPath = argc == 2 ? argv[1] : DEFAULT_CONFIG_PATH;
    const std::string runTimestamp = makeRunTimestamp();
    const std::string projectRoot = projectRootFromConfigPath(configPath);
    const std::string outputDataDir = joinPath(joinPath(projectRoot, "run"), "output_data");
    const std::string outputVideoDir = joinPath(joinPath(projectRoot, "run"), "output_video");

    if (!ensureDirectory(outputDataDir)) {
        std::cerr << "Failed to create output data directory: "
                  << outputDataDir << std::endl;
        return 1;
    }

    const std::string runId = makeUniqueRunId(outputDataDir, outputVideoDir, runTimestamp);
    const std::string outputTextPath = joinPath(outputDataDir, runId + ".txt");
    const std::string outputVideoPath = joinPath(outputVideoDir, runId + ".mp4");

    std::ofstream logFile(outputTextPath.c_str());
    if (!logFile.is_open()) {
        std::cerr << "Failed to open output text file: "
                  << outputTextPath << std::endl;
        return 1;
    }

    ScopedOutputTee outputTee(logFile);

    std::cout << "output_text=" << outputTextPath << std::endl;

    AppConfig config;
    std::string configError;
    if (!loadAppConfig(configPath, config, configError)) {
        std::cerr << "Failed to load config: " << configError << std::endl;
        return 1;
    }

    if (config.output.save_video) {
        if (!ensureDirectory(outputVideoDir)) {
            std::cerr << "Failed to create output video directory: "
                      << outputVideoDir << std::endl;
            return 1;
        }
        config.output.video_path = outputVideoPath;
        std::cout << "output_video=" << outputVideoPath << std::endl;
    }

    std::cout << "effective_feature_channels="
              << effectiveFeatureChannels(config.features)
              << std::endl;

    std::map<int, cv::Rect2f> groundTruth;
    std::string annotationError;
    if (!loadGroundTruthAnnotations(config.input.annotation_path,
                                    groundTruth,
                                    annotationError)) {
        std::cerr << annotationError << std::endl;
        return 1;
    }
    if (groundTruth.empty()) {
        std::cerr << "No valid annotations loaded." << std::endl;
        return 1;
    }
    const std::map<int, cv::Rect2f>::const_iterator firstTruth = groundTruth.begin();
    const int firstFrameNumber = firstTruth->first;

    std::string frameSourceError;
    std::unique_ptr<FrameSource> frameSource =
        createFrameSource(config.input, frameSourceError);
    if (!frameSource) {
        std::cerr << frameSourceError << std::endl;
        return 1;
    }

    int loadedFrames = 0;
    double totalFrameLoadTimeMs = 0.0;
    FrameData frameData;
    bool foundInitialFrame = false;
    while (frameSource->readNext(frameData, frameSourceError)) {
        ++loadedFrames;
        totalFrameLoadTimeMs += frameData.load_time_ms;
        if (frameData.frame_number < firstFrameNumber) {
            continue;
        }
        if (frameData.frame_number == firstFrameNumber) {
            foundInitialFrame = true;
        }
        break;
    }
    if (!frameSourceError.empty()) {
        std::cerr << frameSourceError << std::endl;
        return 1;
    }
    if (!foundInitialFrame || frameData.image.empty()) {
        std::cerr << "Failed to read initial frame " << firstFrameNumber << "." << std::endl;
        return 1;
    }

    cv::Mat frame = frameData.image;

    cv::VideoWriter writer;
    if (config.output.save_video) {
        writer.open(config.output.video_path,
                    cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                    frameSource->fps(),
                    frame.size());
        if (!writer.isOpened()) {
            std::cerr << "Failed to open output video: " << config.output.video_path
                      << std::endl;
            return 1;
        }
    }

    KCFTracker tracker(config.tracker, config.features);
    const cv::Rect2f initBox = firstTruth->second;
    try {
        tracker.init(rect2fToRect(initBox), frame);
    } catch (const std::exception& ex) {
        std::cerr << "Tracker initialization failed: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Running KCF evaluation" << std::endl;
    printAppConfig(configPath, config);

    int evaluatedFrames = 0;
    int updateFrames = 0;
    int cleBelowTwoFrames = 0;
    double totalKcfTimeMs = 0.0;
    double totalCle = 0.0;
    double totalIou = 0.0;

    cv::Rect2f predictedBox = initBox;
    double cle = centerLocationError(predictedBox, firstTruth->second);
    double iou = intersectionOverUnion(predictedBox, firstTruth->second);
    ++evaluatedFrames;
    totalCle += cle;
    totalIou += iou;
    if (cle < 2.0) {
        ++cleBelowTwoFrames;
    }
    printFrameMetrics(firstFrameNumber, frameData.load_time_ms, 0.0, true, cle, iou);

    if (config.output.save_video) {
        const cv::Rect predictedDrawBox = clampRectToFrame(predictedBox, frame.size());
        const cv::Rect truthDrawBox = clampRectToFrame(firstTruth->second, frame.size());
        if (predictedDrawBox.area() > 0) {
            cv::rectangle(frame, predictedDrawBox, cv::Scalar(0, 255, 0), 2);
        }
        if (truthDrawBox.area() > 0) {
            cv::rectangle(frame, truthDrawBox, cv::Scalar(0, 0, 255), 2);
        }
        writer.write(frame);
    }

    int processedFrames = 1;
    while (frameSource->readNext(frameData, frameSourceError)) {
        ++loadedFrames;
        totalFrameLoadTimeMs += frameData.load_time_ms;
        ++processedFrames;

        frame = frameData.image;
        const int frameIndex = frameData.frame_number;
        const std::map<int, cv::Rect2f>::const_iterator truthIt =
            groundTruth.find(frameIndex);

        cv::TickMeter timer;
        timer.start();
        cv::Rect result;
        try {
            result = tracker.update(frame);
            timer.stop();
        } catch (const std::exception& ex) {
            timer.stop();
            std::cerr << "Tracker update failed at frame " << frameIndex
                      << ": " << ex.what() << std::endl;
            return 1;
        }

        const double kcfTimeMs = timer.getTimeMilli();
        ++updateFrames;
        totalKcfTimeMs += kcfTimeMs;

        predictedBox = cv::Rect2f(static_cast<float>(result.x),
                                  static_cast<float>(result.y),
                                  static_cast<float>(result.width),
                                  static_cast<float>(result.height));

        bool hasTruth = truthIt != groundTruth.end();
        cv::Rect2f truthBox;
        if (hasTruth) {
            truthBox = truthIt->second;
            cle = centerLocationError(predictedBox, truthBox);
            iou = intersectionOverUnion(predictedBox, truthBox);

            ++evaluatedFrames;
            totalCle += cle;
            totalIou += iou;
            if (cle < 2.0) {
                ++cleBelowTwoFrames;
            }
        }

        printFrameMetrics(frameIndex, frameData.load_time_ms, kcfTimeMs, hasTruth, cle, iou);

        if (config.output.save_video) {
            const cv::Rect predictedDrawBox = clampRectToFrame(predictedBox, frame.size());
            if (predictedDrawBox.area() > 0) {
                cv::rectangle(frame, predictedDrawBox, cv::Scalar(0, 255, 0), 2);
            }
            if (hasTruth) {
                const cv::Rect truthDrawBox = clampRectToFrame(truthBox, frame.size());
                if (truthDrawBox.area() > 0) {
                    cv::rectangle(frame, truthDrawBox, cv::Scalar(0, 0, 255), 2);
                }
            }
            writer.write(frame);
        }
    }
    if (!frameSourceError.empty()) {
        std::cerr << frameSourceError << std::endl;
        return 1;
    }

    const double averageKcfTimeMs =
        updateFrames > 0 ? totalKcfTimeMs / static_cast<double>(updateFrames) : 0.0;
    const double averageFrameLoadTimeMs =
        loadedFrames > 0 ? totalFrameLoadTimeMs / static_cast<double>(loadedFrames) : 0.0;
    const double averageKcfFps =
        averageKcfTimeMs > 0.0 ? 1000.0 / averageKcfTimeMs : 0.0;
    const double averageCle =
        evaluatedFrames > 0 ? totalCle / static_cast<double>(evaluatedFrames) : 0.0;
    const double averageIou =
        evaluatedFrames > 0 ? totalIou / static_cast<double>(evaluatedFrames) : 0.0;
    const double cleBelowTwoRatio =
        evaluatedFrames > 0
            ? static_cast<double>(cleBelowTwoFrames) / static_cast<double>(evaluatedFrames)
            : 0.0;

    std::cout << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "experiment=" << config.experiment.name << std::endl;
    std::cout << "config=" << configPath << std::endl;
    std::cout << "hog_channels="
              << (config.features.hog_enabled ? config.features.hog_channels : 0)
              << std::endl;
    std::cout << "cn_enabled="
              << (config.features.cn_enabled ? "true" : "false") << std::endl;
    std::cout << "cn_channels="
              << (config.features.cn_enabled ? config.features.cn_channels : 0)
              << std::endl;
    std::cout << "effective_feature_channels="
              << effectiveFeatureChannels(config.features) << std::endl;
    std::cout << "lab_enabled="
              << (config.features.lab_enabled ? "true" : "false") << std::endl;
    std::cout << "fusion_mode=" << config.features.fusion_mode << std::endl;
    std::cout << "processed_frames=" << processedFrames << std::endl;
    std::cout << "loaded_frames=" << loadedFrames << std::endl;
    std::cout << "evaluated_frames=" << evaluatedFrames << std::endl;
    std::cout << "update_frames=" << updateFrames << std::endl;
    std::cout << "avg_frame_load_ms=" << averageFrameLoadTimeMs << std::endl;
    std::cout << "avg_kcf_ms=" << averageKcfTimeMs << std::endl;
    std::cout << "avg_kcf_fps=" << averageKcfFps << std::endl;
    std::cout << "avg_CLE=" << averageCle << std::endl;
    std::cout << "avg_IoU=" << averageIou << std::endl;
    std::cout << "CLE_lt_2_ratio=" << cleBelowTwoRatio << std::endl;

    return 0;
}

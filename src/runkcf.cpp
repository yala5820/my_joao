#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "app_config.hpp"
#include "kcftracker.hpp"

namespace {

const std::string DEFAULT_CONFIG_PATH =
    "D:\\code\\cpp\\vs\\myKCF\\joaofaro\\joao\\configs\\baseline_hog31.yaml";

struct FrameAnnotation {
    int frameNumber;
    cv::Rect2f box;
};

bool parseAnnotationLine(const std::string& line, FrameAnnotation& annotation)
{
    std::string normalized = line;
    for (size_t i = 0; i < normalized.size(); ++i) {
        char& ch = normalized[i];
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == ',') {
            ch = ' ';
        }
    }

    std::istringstream stream(normalized);
    int frameNumber = 0;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!(stream >> frameNumber >> x >> y >> width >> height)) {
        return false;
    }

    annotation.frameNumber = frameNumber;
    annotation.box = cv::Rect2f(x, y, width, height);
    return frameNumber >= 0 && width > 0.0f && height > 0.0f;
}

std::map<int, cv::Rect2f> loadGroundTruth(const std::string& path)
{
    std::map<int, cv::Rect2f> boxesByFrame;
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "Failed to open annotation file: " << path << std::endl;
        return boxesByFrame;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        FrameAnnotation annotation;
        if (!parseAnnotationLine(line, annotation)) {
            std::cerr << "Invalid annotation at line " << lineNumber
                      << ": " << line << std::endl;
            boxesByFrame.clear();
            return boxesByFrame;
        }
        if (boxesByFrame.find(annotation.frameNumber) != boxesByFrame.end()) {
            std::cerr << "Duplicate annotation for frame "
                      << annotation.frameNumber << " at line "
                      << lineNumber << std::endl;
            boxesByFrame.clear();
            return boxesByFrame;
        }
        boxesByFrame[annotation.frameNumber] = annotation.box;
    }

    return boxesByFrame;
}

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

void printFrameMetrics(int frameIndex, double kcfTimeMs, double cle, double iou)
{
    std::cout << "frame=" << frameIndex
              << ", kcf_ms=" << kcfTimeMs
              << ", CLE=" << cle
              << ", IoU=" << iou << std::endl;
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

}  // namespace

int main(int argc, char* argv[])
{
    if (argc > 2) {
        std::cerr << "Usage: RunKCF.exe [config_path]" << std::endl;
        return 1;
    }

    const std::string configPath = argc == 2 ? argv[1] : DEFAULT_CONFIG_PATH;
    AppConfig config;
    std::string configError;
    if (!loadAppConfig(configPath, config, configError)) {
        std::cerr << "Failed to load config: " << configError << std::endl;
        return 1;
    }

    std::cout << "effective_feature_channels="
              << effectiveFeatureChannels(config.features)
              << std::endl;

    const std::map<int, cv::Rect2f> groundTruth =
        loadGroundTruth(config.input.annotation_path);
    if (groundTruth.empty()) {
        std::cerr << "No valid annotations loaded." << std::endl;
        return 1;
    }
    const std::map<int, cv::Rect2f>::const_iterator firstTruth = groundTruth.begin();
    const int firstFrameNumber = firstTruth->first;

    cv::VideoCapture capture(config.input.video_path);
    if (!capture.isOpened()) {
        std::cerr << "Failed to open video: " << config.input.video_path << std::endl;
        return 1;
    }

    cv::Mat frame;
    if (!capture.read(frame) || frame.empty()) {
        std::cerr << "Failed to read the first frame." << std::endl;
        return 1;
    }

    cv::VideoWriter writer;
    if (config.output.save_video) {
        double inputFps = capture.get(cv::CAP_PROP_FPS);
        if (inputFps <= 0.0) {
            inputFps = 30.0;
        }

        writer.open(config.output.video_path,
                    cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                    inputFps,
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
    printFrameMetrics(firstFrameNumber, 0.0, cle, iou);

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

    int frameOffset = 0;
    while (capture.read(frame)) {
        ++frameOffset;
        if (frame.empty()) {
            break;
        }

        const int frameIndex = firstFrameNumber + frameOffset;
        const std::map<int, cv::Rect2f>::const_iterator truthIt =
            groundTruth.find(frameIndex);
        if (truthIt == groundTruth.end()) {
            std::cerr << "No annotation for frame " << frameIndex
                      << ". Stopping evaluation." << std::endl;
            break;
        }

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
        predictedBox = cv::Rect2f(static_cast<float>(result.x),
                                  static_cast<float>(result.y),
                                  static_cast<float>(result.width),
                                  static_cast<float>(result.height));
        const cv::Rect2f truthBox = truthIt->second;
        cle = centerLocationError(predictedBox, truthBox);
        iou = intersectionOverUnion(predictedBox, truthBox);

        ++evaluatedFrames;
        ++updateFrames;
        totalKcfTimeMs += kcfTimeMs;
        totalCle += cle;
        totalIou += iou;
        if (cle < 2.0) {
            ++cleBelowTwoFrames;
        }

        printFrameMetrics(frameIndex, kcfTimeMs, cle, iou);

        if (config.output.save_video) {
            const cv::Rect predictedDrawBox = clampRectToFrame(predictedBox, frame.size());
            const cv::Rect truthDrawBox = clampRectToFrame(truthBox, frame.size());
            if (predictedDrawBox.area() > 0) {
                cv::rectangle(frame, predictedDrawBox, cv::Scalar(0, 255, 0), 2);
            }
            if (truthDrawBox.area() > 0) {
                cv::rectangle(frame, truthDrawBox, cv::Scalar(0, 0, 255), 2);
            }
            writer.write(frame);
        }
    }

    const double averageKcfTimeMs =
        updateFrames > 0 ? totalKcfTimeMs / static_cast<double>(updateFrames) : 0.0;
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
    std::cout << "evaluated_frames=" << evaluatedFrames << std::endl;
    std::cout << "update_frames=" << updateFrames << std::endl;
    std::cout << "avg_kcf_ms=" << averageKcfTimeMs << std::endl;
    std::cout << "avg_kcf_fps=" << averageKcfFps << std::endl;
    std::cout << "avg_CLE=" << averageCle << std::endl;
    std::cout << "avg_IoU=" << averageIou << std::endl;
    std::cout << "CLE_lt_2_ratio=" << cleBelowTwoRatio << std::endl;

    return 0;
}

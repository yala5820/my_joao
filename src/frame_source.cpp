#include "frame_source.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

namespace {

bool fileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.is_open();
}

class VideoFrameSource : public FrameSource {
public:
    explicit VideoFrameSource(const std::string& path)
        : next_frame_number_(0)
    {
        capture_.open(path);
    }

    bool isOpened() const
    {
        return capture_.isOpened();
    }

    double fps() const
    {
        const double value = capture_.get(cv::CAP_PROP_FPS);
        return value > 0.0 ? value : 30.0;
    }

    bool readNext(FrameData& frame, std::string& error)
    {
        cv::TickMeter timer;
        timer.start();
        cv::Mat image;
        const bool ok = capture_.read(image);
        timer.stop();

        if (!ok || image.empty()) {
            error.clear();
            return false;
        }

        frame.frame_number = next_frame_number_;
        frame.image = image;
        frame.load_time_ms = timer.getTimeMilli();
        ++next_frame_number_;
        return true;
    }

private:
    cv::VideoCapture capture_;
    int next_frame_number_;
};

class ImageSequenceFrameSource : public FrameSource {
public:
    ImageSequenceFrameSource(const std::string& image_dir,
                             const std::string& image_extension,
                             int start_index,
                             int index_digits)
        : image_dir_(image_dir),
          image_extension_(image_extension),
          next_frame_number_(start_index),
          index_digits_(index_digits)
    {
    }

    bool readNext(FrameData& frame, std::string& error)
    {
        const std::string path = makeImageSequencePath(
            image_dir_, next_frame_number_, index_digits_, image_extension_);

        if (!fileExists(path)) {
            error.clear();
            return false;
        }

        cv::TickMeter timer;
        timer.start();
        cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
        timer.stop();

        if (image.empty()) {
            error.clear();
            return false;
        }

        frame.frame_number = next_frame_number_;
        frame.image = image;
        frame.load_time_ms = timer.getTimeMilli();
        ++next_frame_number_;
        return true;
    }

    double fps() const
    {
        return 30.0;
    }

private:
    std::string image_dir_;
    std::string image_extension_;
    int next_frame_number_;
    int index_digits_;
};

}  // namespace

std::string makeImageSequencePath(const std::string& image_dir,
                                  int frame_number,
                                  int index_digits,
                                  const std::string& image_extension)
{
    std::ostringstream path;
    path << image_dir;
    if (!image_dir.empty()) {
        const char last = image_dir[image_dir.size() - 1];
        if (last != '/' && last != '\\') {
            path << "/";
        }
    }
    path << std::setw(index_digits) << std::setfill('0') << frame_number
         << image_extension;
    return path.str();
}

std::unique_ptr<FrameSource> createFrameSource(const InputConfig& input,
                                               std::string& error)
{
    if (input.type == "video") {
        std::unique_ptr<VideoFrameSource> source(new VideoFrameSource(input.video_path));
        if (!source->isOpened()) {
            error = "Failed to open video: " + input.video_path;
            return std::unique_ptr<FrameSource>();
        }
        return std::unique_ptr<FrameSource>(source.release());
    }

    if (input.type == "image_sequence") {
        return std::unique_ptr<FrameSource>(
            new ImageSequenceFrameSource(input.image_dir,
                                         input.image_extension,
                                         input.image_start_index,
                                         input.image_index_digits));
    }

    error = "Unsupported input.type: " + input.type;
    return std::unique_ptr<FrameSource>();
}

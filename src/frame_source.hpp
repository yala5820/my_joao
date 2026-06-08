#pragma once

#include <memory>
#include <string>

#include <opencv2/core.hpp>

#include "app_config.hpp"

struct FrameData {
    int frame_number;
    cv::Mat image;
    double load_time_ms;
};

class FrameSource {
public:
    virtual ~FrameSource() {}
    virtual bool readNext(FrameData& frame, std::string& error) = 0;
    virtual double fps() const = 0;
};

std::string makeImageSequencePath(const std::string& image_dir,
                                  int frame_number,
                                  int index_digits,
                                  const std::string& image_extension);

std::unique_ptr<FrameSource> createFrameSource(const InputConfig& input,
                                               std::string& error);

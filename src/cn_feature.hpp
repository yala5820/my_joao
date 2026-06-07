#pragma once

#include <opencv2/core.hpp>

cv::Mat extractCnFeatureMap(const cv::Mat& bgr_patch,
                            int cell_size,
                            int output_channels,
                            float weight = 1.0f);

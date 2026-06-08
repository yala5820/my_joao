#pragma once

#include <map>
#include <string>

#include <opencv2/core.hpp>

bool parseAnnotationText(const std::string& text,
                         std::map<int, cv::Rect2f>& boxes_by_frame,
                         std::string& error);

bool loadGroundTruthAnnotations(const std::string& path,
                                std::map<int, cv::Rect2f>& boxes_by_frame,
                                std::string& error);

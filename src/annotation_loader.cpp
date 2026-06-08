#include "annotation_loader.hpp"

#include <fstream>
#include <sstream>
#include <vector>

namespace {

bool isMissingBox(float x, float y, float width, float height)
{
    return x == -100.0f && y == -100.0f && width == -100.0f && height == -100.0f;
}

std::vector<float> parseNumbers(const std::string& line)
{
    std::string normalized = line;
    for (size_t i = 0; i < normalized.size(); ++i) {
        char& ch = normalized[i];
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == ',') {
            ch = ' ';
        }
    }

    std::vector<float> values;
    std::istringstream stream(normalized);
    float value = 0.0f;
    while (stream >> value) {
        values.push_back(value);
    }
    return values;
}

}  // namespace

bool parseAnnotationText(const std::string& text,
                         std::map<int, cv::Rect2f>& boxes_by_frame,
                         std::string& error)
{
    boxes_by_frame.clear();

    std::istringstream input(text);
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        const std::vector<float> values = parseNumbers(line);
        int frame_number = line_number;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        if (values.size() == 4) {
            x = values[0];
            y = values[1];
            width = values[2];
            height = values[3];
        } else if (values.size() == 5) {
            frame_number = static_cast<int>(values[0]);
            x = values[1];
            y = values[2];
            width = values[3];
            height = values[4];
        } else {
            std::ostringstream message;
            message << "Invalid annotation at line " << line_number
                    << ": expected 4 or 5 numbers.";
            error = message.str();
            boxes_by_frame.clear();
            return false;
        }

        if (isMissingBox(x, y, width, height)) {
            continue;
        }
        if (frame_number < 0 || width <= 0.0f || height <= 0.0f) {
            std::ostringstream message;
            message << "Invalid annotation box at line " << line_number << ".";
            error = message.str();
            boxes_by_frame.clear();
            return false;
        }
        if (boxes_by_frame.find(frame_number) != boxes_by_frame.end()) {
            std::ostringstream message;
            message << "Duplicate annotation for frame " << frame_number
                    << " at line " << line_number << ".";
            error = message.str();
            boxes_by_frame.clear();
            return false;
        }

        boxes_by_frame[frame_number] = cv::Rect2f(x, y, width, height);
    }

    if (boxes_by_frame.empty()) {
        error = "No valid annotations loaded.";
        return false;
    }
    return true;
}

bool loadGroundTruthAnnotations(const std::string& path,
                                std::map<int, cv::Rect2f>& boxes_by_frame,
                                std::string& error)
{
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        error = "Failed to open annotation file: " + path;
        return false;
    }

    std::ostringstream text;
    text << file.rdbuf();
    return parseAnnotationText(text.str(), boxes_by_frame, error);
}

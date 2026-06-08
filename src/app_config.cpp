#include "app_config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace {

std::string trim(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }

    size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
        --last;
    }

    return text.substr(first, last - first);
}

std::string stripComment(const std::string& line)
{
    bool inQuote = false;
    char quote = '\0';
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if ((ch == '"' || ch == '\'') && (i == 0 || line[i - 1] != '\\')) {
            if (!inQuote) {
                inQuote = true;
                quote = ch;
            } else if (quote == ch) {
                inQuote = false;
                quote = '\0';
            }
        } else if (ch == '#' && !inQuote) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::string unquote(const std::string& value)
{
    if (value.size() >= 2) {
        const char first = value[0];
        const char last = value[value.size() - 1];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::string joinPath(const std::vector<std::string>& path, size_t level,
                     const std::string& key)
{
    std::ostringstream fullPath;
    for (size_t i = 0; i < level; ++i) {
        if (i > 0) {
            fullPath << '.';
        }
        fullPath << path[i];
    }
    if (level > 0) {
        fullPath << '.';
    }
    fullPath << key;
    return fullPath.str();
}

bool readTextFile(const std::string& path, std::string& text, std::string& error)
{
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        error = "Failed to open config file: " + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

bool parseConfigText(const std::string& text,
                     std::map<std::string, std::string>& values,
                     std::string& error)
{
    values.clear();

    std::istringstream input(text);
    std::string line;
    std::vector<std::string> path;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.resize(line.size() - 1);
        }

        std::string cleaned = stripComment(line);
        if (trim(cleaned).empty()) {
            continue;
        }

        const std::string trimmedLine = trim(cleaned);
        if (trimmedLine == "---" || trimmedLine.find("%YAML:") == 0) {
            continue;
        }

        size_t indent = 0;
        while (indent < cleaned.size() && cleaned[indent] == ' ') {
            ++indent;
        }
        if (indent % 2 != 0) {
            std::ostringstream message;
            message << "Invalid indentation at config line " << lineNumber << ".";
            error = message.str();
            return false;
        }

        const size_t level = indent / 2;
        if (level > path.size()) {
            std::ostringstream message;
            message << "Invalid nesting at config line " << lineNumber << ".";
            error = message.str();
            return false;
        }

        const size_t colon = trimmedLine.find(':');
        if (colon == std::string::npos) {
            std::ostringstream message;
            message << "Invalid config line " << lineNumber << ": expected key/value.";
            error = message.str();
            return false;
        }

        const std::string key = trim(trimmedLine.substr(0, colon));
        const std::string rawValue = trim(trimmedLine.substr(colon + 1));
        if (key.empty()) {
            std::ostringstream message;
            message << "Invalid empty key at config line " << lineNumber << ".";
            error = message.str();
            return false;
        }

        if (rawValue.empty()) {
            path.resize(level);
            path.push_back(key);
            continue;
        }

        const std::string fullPath = joinPath(path, level, key);
        if (values.find(fullPath) != values.end()) {
            error = "Duplicate config field: " + fullPath;
            return false;
        }
        values[fullPath] = unquote(rawValue);
    }

    return true;
}

bool readRequiredString(const std::map<std::string, std::string>& values,
                        const std::string& key,
                        std::string& value,
                        std::string& error)
{
    std::map<std::string, std::string>::const_iterator it = values.find(key);
    if (it == values.end()) {
        error = "Missing required config field: " + key;
        return false;
    }
    value = it->second;
    return true;
}

bool readOptionalString(const std::map<std::string, std::string>& values,
                        const std::string& key,
                        const std::string& defaultValue,
                        std::string& value)
{
    std::map<std::string, std::string>::const_iterator it = values.find(key);
    if (it == values.end()) {
        value = defaultValue;
        return true;
    }
    value = it->second;
    return true;
}

bool readRequiredInt(const std::map<std::string, std::string>& values,
                     const std::string& key,
                     int& value,
                     std::string& error)
{
    std::map<std::string, std::string>::const_iterator it = values.find(key);
    if (it == values.end()) {
        error = "Missing required config field: " + key;
        return false;
    }

    char* end = NULL;
    const long parsed = std::strtol(it->second.c_str(), &end, 10);
    if (end == it->second.c_str() || *end != '\0') {
        error = "Invalid integer value for config field: " + key;
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool readOptionalInt(const std::map<std::string, std::string>& values,
                     const std::string& key,
                     int defaultValue,
                     int& value,
                     std::string& error)
{
    if (values.find(key) == values.end()) {
        value = defaultValue;
        return true;
    }
    return readRequiredInt(values, key, value, error);
}

bool readRequiredBool(const std::map<std::string, std::string>& values,
                      const std::string& key,
                      bool& value,
                      std::string& error)
{
    std::string text;
    if (!readRequiredString(values, key, text, error)) {
        return false;
    }

    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (text == "true" || text == "1") {
        value = true;
        return true;
    }
    if (text == "false" || text == "0") {
        value = false;
        return true;
    }

    error = "Invalid boolean value for config field: " + key;
    return false;
}

bool validateAppConfig(const AppConfig& config, std::string& error)
{
    if (config.input.video_path.empty()) {
        if (config.input.type == "video") {
            error = "input.video_path is empty.";
            return false;
        }
    }
    if (config.input.type != "video" && config.input.type != "image_sequence") {
        error = "input.type must be video or image_sequence.";
        return false;
    }
    if (config.input.type == "image_sequence") {
        if (config.input.image_dir.empty()) {
            error = "input.image_dir is empty.";
            return false;
        }
        if (config.input.image_extension.empty()) {
            error = "input.image_extension is empty.";
            return false;
        }
        if (config.input.image_start_index <= 0) {
            error = "input.image_start_index must be positive.";
            return false;
        }
        if (config.input.image_index_digits <= 0) {
            error = "input.image_index_digits must be positive.";
            return false;
        }
    }
    if (config.input.annotation_path.empty()) {
        error = "input.annotation_path is empty.";
        return false;
    }
    if (!config.features.hog_enabled && !config.features.cn_enabled) {
        error = "At least one feature must be enabled: HOG or CN.";
        return false;
    }
    if (config.features.cn_enabled && !config.features.hog_enabled) {
        error = "Phase 2.3 requires HOG to be enabled when CN is enabled.";
        return false;
    }
    if (config.features.hog_enabled &&
        config.features.hog_channels != 31 &&
        config.features.hog_channels != 18) {
        error = "features.hog.channels must be 31 or 18.";
        return false;
    }
    if (!config.features.hog_enabled && config.features.hog_channels != 0) {
        error = "features.hog.channels must be 0 when HOG is disabled.";
        return false;
    }
    if (config.features.cn_enabled && config.features.cn_channels != 4) {
        error = "features.cn.channels must be 4 when CN is enabled.";
        return false;
    }
    if (!config.features.cn_enabled && config.features.cn_channels != 0) {
        error = "features.cn.channels must be 0 when CN is disabled.";
        return false;
    }
    if (config.features.fusion_mode != "concat") {
        error = "features.fusion.mode must be concat.";
        return false;
    }
    return true;
}

}  // namespace

bool loadAppConfig(const std::string& path, AppConfig& config, std::string& error)
{
    std::string text;
    std::map<std::string, std::string> values;
    if (!readTextFile(path, text, error) ||
        !parseConfigText(text, values, error)) {
        return false;
    }

    if (!readRequiredString(values, "experiment.name", config.experiment.name, error) ||
        !readRequiredString(values, "experiment.description", config.experiment.description, error) ||
        !readRequiredString(values, "input.annotation_path", config.input.annotation_path, error) ||
        !readRequiredBool(values, "output.save_video", config.output.save_video, error) ||
        !readRequiredString(values, "output.video_path", config.output.video_path, error) ||
        !readRequiredBool(values, "tracker.fixed_window", config.tracker.fixed_window, error) ||
        !readRequiredBool(values, "tracker.multiscale", config.tracker.multiscale, error) ||
        !readRequiredBool(values, "features.hog.enabled", config.features.hog_enabled, error) ||
        !readRequiredInt(values, "features.hog.channels", config.features.hog_channels, error) ||
        !readRequiredBool(values, "features.cn.enabled", config.features.cn_enabled, error) ||
        !readRequiredInt(values, "features.cn.channels", config.features.cn_channels, error) ||
        !readRequiredBool(values, "features.lab.enabled", config.features.lab_enabled, error) ||
        !readRequiredString(values, "features.fusion.mode", config.features.fusion_mode, error)) {
        return false;
    }

    if (!readOptionalString(values, "input.type", "video", config.input.type) ||
        !readOptionalString(values, "input.video_path", "", config.input.video_path) ||
        !readOptionalString(values, "input.image_dir", "", config.input.image_dir) ||
        !readOptionalString(values, "input.image_extension", ".jpg", config.input.image_extension) ||
        !readOptionalInt(values, "input.image_start_index", 1, config.input.image_start_index, error) ||
        !readOptionalInt(values, "input.image_index_digits", 5, config.input.image_index_digits, error)) {
        return false;
    }

    return validateAppConfig(config, error);
}

void printAppConfig(const std::string& path, const AppConfig& config)
{
    std::cout << "config=" << path << std::endl;
    std::cout << "experiment.name=" << config.experiment.name << std::endl;
    std::cout << "experiment.description=" << config.experiment.description << std::endl;
    std::cout << "input.type=" << config.input.type << std::endl;
    std::cout << "video=" << config.input.video_path << std::endl;
    std::cout << "image_dir=" << config.input.image_dir << std::endl;
    std::cout << "image_extension=" << config.input.image_extension << std::endl;
    std::cout << "image_start_index=" << config.input.image_start_index << std::endl;
    std::cout << "image_index_digits=" << config.input.image_index_digits << std::endl;
    std::cout << "annotation=" << config.input.annotation_path << std::endl;
    std::cout << "save_video=" << (config.output.save_video ? "true" : "false") << std::endl;
    std::cout << "output_video=" << config.output.video_path << std::endl;
    std::cout << "fixed_window=" << (config.tracker.fixed_window ? "true" : "false") << std::endl;
    std::cout << "multiscale=" << (config.tracker.multiscale ? "true" : "false") << std::endl;
    std::cout << "hog.enabled=" << (config.features.hog_enabled ? "true" : "false") << std::endl;
    std::cout << "hog.channels=" << config.features.hog_channels << std::endl;
    std::cout << "cn.enabled=" << (config.features.cn_enabled ? "true" : "false") << std::endl;
    std::cout << "cn.channels=" << config.features.cn_channels << std::endl;
    std::cout << "lab.enabled=" << (config.features.lab_enabled ? "true" : "false") << std::endl;
    std::cout << "fusion.mode=" << config.features.fusion_mode << std::endl;
}

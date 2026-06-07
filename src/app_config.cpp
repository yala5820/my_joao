#include "app_config.hpp"

#include <iostream>
#include <sstream>

#include <opencv2/core.hpp>

namespace {

bool readRequiredString(const cv::FileNode& parent,
                        const std::string& key,
                        std::string& value,
                        std::string& error)
{
    const cv::FileNode node = parent[key];
    if (node.empty()) {
        error = "Missing required config field: " + key;
        return false;
    }
    node >> value;
    return true;
}

bool readRequiredInt(const cv::FileNode& parent,
                     const std::string& key,
                     int& value,
                     std::string& error)
{
    const cv::FileNode node = parent[key];
    if (node.empty()) {
        error = "Missing required config field: " + key;
        return false;
    }
    value = static_cast<int>(node);
    return true;
}

bool readRequiredBool(const cv::FileNode& parent,
                      const std::string& key,
                      bool& value,
                      std::string& error)
{
    const cv::FileNode node = parent[key];
    if (node.empty()) {
        error = "Missing required config field: " + key;
        return false;
    }

    if (node.type() == cv::FileNode::INT || node.type() == cv::FileNode::REAL) {
        value = static_cast<int>(node) != 0;
        return true;
    }

    std::string text;
    node >> text;
    if (text == "true" || text == "True" || text == "TRUE" || text == "1") {
        value = true;
        return true;
    }
    if (text == "false" || text == "False" || text == "FALSE" || text == "0") {
        value = false;
        return true;
    }

    error = "Invalid boolean value for config field: " + key;
    return false;
}

bool readRequiredMap(const cv::FileStorage& storage,
                     const std::string& key,
                     cv::FileNode& value,
                     std::string& error)
{
    value = storage[key];
    if (value.empty() || !value.isMap()) {
        error = "Missing required config section: " + key;
        return false;
    }
    return true;
}

bool readRequiredMap(const cv::FileNode& parent,
                     const std::string& key,
                     cv::FileNode& value,
                     std::string& error)
{
    value = parent[key];
    if (value.empty() || !value.isMap()) {
        error = "Missing required config section: " + key;
        return false;
    }
    return true;
}

bool validateAppConfig(const AppConfig& config, std::string& error)
{
    if (config.input.video_path.empty()) {
        error = "input.video_path is empty.";
        return false;
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
    cv::FileStorage storage(path, cv::FileStorage::READ);
    if (!storage.isOpened()) {
        error = "Failed to open config file: " + path;
        return false;
    }

    cv::FileNode experimentNode;
    cv::FileNode inputNode;
    cv::FileNode outputNode;
    cv::FileNode trackerNode;
    cv::FileNode featuresNode;
    cv::FileNode hogNode;
    cv::FileNode cnNode;
    cv::FileNode labNode;
    cv::FileNode fusionNode;

    if (!readRequiredMap(storage, "experiment", experimentNode, error) ||
        !readRequiredMap(storage, "input", inputNode, error) ||
        !readRequiredMap(storage, "output", outputNode, error) ||
        !readRequiredMap(storage, "tracker", trackerNode, error) ||
        !readRequiredMap(storage, "features", featuresNode, error) ||
        !readRequiredMap(featuresNode, "hog", hogNode, error) ||
        !readRequiredMap(featuresNode, "cn", cnNode, error) ||
        !readRequiredMap(featuresNode, "lab", labNode, error) ||
        !readRequiredMap(featuresNode, "fusion", fusionNode, error)) {
        return false;
    }

    if (!readRequiredString(experimentNode, "name", config.experiment.name, error) ||
        !readRequiredString(experimentNode, "description", config.experiment.description, error) ||
        !readRequiredString(inputNode, "video_path", config.input.video_path, error) ||
        !readRequiredString(inputNode, "annotation_path", config.input.annotation_path, error) ||
        !readRequiredBool(outputNode, "save_video", config.output.save_video, error) ||
        !readRequiredString(outputNode, "video_path", config.output.video_path, error) ||
        !readRequiredBool(trackerNode, "fixed_window", config.tracker.fixed_window, error) ||
        !readRequiredBool(trackerNode, "multiscale", config.tracker.multiscale, error) ||
        !readRequiredBool(hogNode, "enabled", config.features.hog_enabled, error) ||
        !readRequiredInt(hogNode, "channels", config.features.hog_channels, error) ||
        !readRequiredBool(cnNode, "enabled", config.features.cn_enabled, error) ||
        !readRequiredInt(cnNode, "channels", config.features.cn_channels, error) ||
        !readRequiredBool(labNode, "enabled", config.features.lab_enabled, error) ||
        !readRequiredString(fusionNode, "mode", config.features.fusion_mode, error)) {
        return false;
    }

    return validateAppConfig(config, error);
}

void printAppConfig(const std::string& path, const AppConfig& config)
{
    std::cout << "config=" << path << std::endl;
    std::cout << "experiment.name=" << config.experiment.name << std::endl;
    std::cout << "experiment.description=" << config.experiment.description << std::endl;
    std::cout << "video=" << config.input.video_path << std::endl;
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

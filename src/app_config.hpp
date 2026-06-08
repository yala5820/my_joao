#pragma once

#include <string>

struct ExperimentConfig {
    std::string name;
    std::string description;
};

struct InputConfig {
    std::string type;
    std::string video_path;
    std::string image_dir;
    std::string image_extension;
    int image_start_index;
    int image_index_digits;
    std::string annotation_path;
};

struct OutputConfig {
    bool save_video;
    std::string video_path;
};

struct TrackerConfig {
    bool fixed_window;
    bool multiscale;
};

struct FeatureConfig {
    bool hog_enabled;
    int hog_channels;
    bool cn_enabled;
    int cn_channels;
    bool lab_enabled;
    std::string fusion_mode;
};

struct AppConfig {
    ExperimentConfig experiment;
    InputConfig input;
    OutputConfig output;
    TrackerConfig tracker;
    FeatureConfig features;
};

bool loadAppConfig(const std::string& path, AppConfig& config, std::string& error);
void printAppConfig(const std::string& path, const AppConfig& config);

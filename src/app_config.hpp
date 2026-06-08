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

struct ConfidenceConfig {
    bool enabled = false;
    int psr_exclusion_radius = 5;
    float eps = 0.000001f;
    int warmup_frames = 5;
    float ema_alpha = 0.05f;
    float medium_ema_alpha = 0.0f;
    float low_ema_alpha = 0.0f;
    float high_ratio = 0.95f;
    float low_ratio = 0.60f;
    float hard_low_ratio = 0.35f;
    float medium_lr_factor = 0.30f;
    float soft_low_position_damping = 1.00f;
    float medium_scale_smoothing = 0.50f;
    float hard_low_displacement_threshold = 0.50f;
    float hard_low_position_damping = 0.50f;
};

struct AppConfig {
    ExperimentConfig experiment;
    InputConfig input;
    OutputConfig output;
    TrackerConfig tracker;
    FeatureConfig features;
    ConfidenceConfig confidence;
};

bool loadAppConfig(const std::string& path, AppConfig& config, std::string& error);
void printAppConfig(const std::string& path, const AppConfig& config);

#include <cerrno>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "app_config.hpp"
#include "annotation_loader.hpp"
#include "frame_source.hpp"

namespace {

bool nearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 1.0e-7f;
}

bool ensureDirectory(const char* path)
{
#ifdef _WIN32
    const int result = _mkdir(path);
#else
    const int result = mkdir(path, 0755);
#endif
    return result == 0 || errno == EEXIST;
}

bool testFourValueAnnotationsUseLineNumber()
{
    std::map<int, cv::Rect2f> boxes;
    std::string error;
    const bool ok = parseAnnotationText(
        "1190 832 65 35\n"
        "-100 -100 -100 -100\n"
        "1187,833,66,34\n",
        boxes,
        error);

    if (!ok || boxes.size() != 2) {
        std::cerr << "Unexpected annotation parse result: " << error << std::endl;
        return false;
    }
    return boxes.count(1) == 1 &&
           boxes.count(2) == 0 &&
           boxes.count(3) == 1 &&
           boxes[1].x == 1190.0f &&
           boxes[3].width == 66.0f;
}

bool testFiveValueAnnotationsKeepExplicitFrameNumber()
{
    std::map<int, cv::Rect2f> boxes;
    std::string error;
    const bool ok = parseAnnotationText(
        "[0, 100, 200, 30, 40]\n"
        "[11, -100, -100, -100, -100]\n",
        boxes,
        error);

    if (!ok || boxes.size() != 1) {
        std::cerr << "Unexpected explicit-frame parse result: " << error << std::endl;
        return false;
    }
    return boxes.count(0) == 1 && boxes.count(11) == 0;
}

bool testImageSequencePathFormatting()
{
    const std::string path = makeImageSequencePath("C:/seq/img", 12, 5, ".jpg");
    return path == "C:/seq/img/00012.jpg";
}

bool testHeaderlessYamlConfigLoads()
{
    if (!ensureDirectory("run")) {
        std::cerr << "Failed to create test output directory: run" << std::endl;
        return false;
    }

    const std::string path = "run/headerless_config_test.yaml";
    std::ofstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "Failed to create test config: " << path << std::endl;
        return false;
    }

    file << "experiment:\n"
         << "  name: headerless_config_test\n"
         << "  description: Headerless OpenCV FileStorage config test\n"
         << "\n"
         << "input:\n"
         << "  type: image_sequence\n"
         << "  image_dir: \"C:/seq/img\"\n"
         << "  image_extension: \".jpg\"\n"
         << "  image_start_index: 1\n"
         << "  image_index_digits: 5\n"
         << "  annotation_path: \"C:/seq/gt.txt\"\n"
         << "\n"
         << "output:\n"
         << "  save_video: 0\n"
         << "  video_path: \"run/out.mp4\"\n"
         << "\n"
         << "tracker:\n"
         << "  fixed_window: 0\n"
         << "  multiscale: 1\n"
         << "\n"
         << "features:\n"
         << "  hog:\n"
         << "    enabled: 1\n"
         << "    channels: 31\n"
         << "  cn:\n"
         << "    enabled: 0\n"
         << "    channels: 0\n"
         << "  lab:\n"
         << "    enabled: 0\n"
         << "  fusion:\n"
         << "    mode: concat\n";
    file.close();

    AppConfig config;
    std::string error;
    const bool ok = loadAppConfig(path, config, error);
    std::remove(path.c_str());

    if (!ok) {
        std::cerr << "Failed to load headerless config: " << error << std::endl;
        return false;
    }
    return config.experiment.name == "headerless_config_test" &&
           config.input.type == "image_sequence" &&
           config.input.image_start_index == 1 &&
           config.features.hog_channels == 31 &&
           !config.confidence.enabled;
}

bool testConfidenceYamlConfigLoads()
{
    if (!ensureDirectory("run")) {
        std::cerr << "Failed to create test output directory: run" << std::endl;
        return false;
    }

    const std::string path = "run/confidence_config_test.yaml";
    std::ofstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "Failed to create test config: " << path << std::endl;
        return false;
    }

    file << "experiment:\n"
         << "  name: confidence_config_test\n"
         << "  description: Confidence config test\n"
         << "\n"
         << "input:\n"
         << "  video_path: \"C:/seq/video.mp4\"\n"
         << "  annotation_path: \"C:/seq/gt.txt\"\n"
         << "\n"
         << "output:\n"
         << "  save_video: 0\n"
         << "  video_path: \"run/out.mp4\"\n"
         << "\n"
         << "tracker:\n"
         << "  fixed_window: 0\n"
         << "  multiscale: 1\n"
         << "\n"
         << "features:\n"
         << "  hog:\n"
         << "    enabled: 1\n"
         << "    channels: 18\n"
         << "  cn:\n"
         << "    enabled: 0\n"
         << "    channels: 0\n"
         << "  lab:\n"
         << "    enabled: 0\n"
         << "  fusion:\n"
         << "    mode: concat\n"
         << "\n"
         << "confidence:\n"
         << "  enabled: 1\n"
         << "  psr_exclusion_radius: 4\n"
         << "  eps: 0.000002\n"
         << "  warmup_frames: 6\n"
         << "  ema_alpha: 0.06\n"
         << "  medium_ema_alpha: 0.01\n"
         << "  low_ema_alpha: 0.02\n"
         << "  high_ratio: 0.96\n"
         << "  low_ratio: 0.61\n"
         << "  hard_low_ratio: 0.32\n"
         << "  medium_lr_factor: 0.31\n"
         << "  soft_low_position_damping: 0.93\n"
         << "  medium_scale_smoothing: 0.44\n"
         << "  hard_low_displacement_threshold: 0.51\n"
         << "  hard_low_position_damping: 0.52\n";
    file.close();

    AppConfig config;
    std::string error;
    const bool ok = loadAppConfig(path, config, error);
    std::remove(path.c_str());

    if (!ok) {
        std::cerr << "Failed to load confidence config: " << error << std::endl;
        return false;
    }

    return config.confidence.enabled &&
           config.confidence.psr_exclusion_radius == 4 &&
           config.confidence.warmup_frames == 6 &&
           nearlyEqual(config.confidence.eps, 0.000002f) &&
           nearlyEqual(config.confidence.ema_alpha, 0.06f) &&
           nearlyEqual(config.confidence.medium_ema_alpha, 0.01f) &&
           nearlyEqual(config.confidence.low_ema_alpha, 0.02f) &&
           nearlyEqual(config.confidence.high_ratio, 0.96f) &&
           nearlyEqual(config.confidence.low_ratio, 0.61f) &&
           nearlyEqual(config.confidence.hard_low_ratio, 0.32f) &&
           nearlyEqual(config.confidence.medium_lr_factor, 0.31f) &&
           nearlyEqual(config.confidence.soft_low_position_damping, 0.93f) &&
           nearlyEqual(config.confidence.medium_scale_smoothing, 0.44f) &&
           nearlyEqual(config.confidence.hard_low_displacement_threshold, 0.51f) &&
           nearlyEqual(config.confidence.hard_low_position_damping, 0.52f);
}

bool testLegacyLowDisplacementFieldsMapToHardLowFields()
{
    if (!ensureDirectory("run")) {
        std::cerr << "Failed to create test output directory: run" << std::endl;
        return false;
    }

    const std::string path = "run/legacy_confidence_config_test.yaml";
    std::ofstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "Failed to create test config: " << path << std::endl;
        return false;
    }

    file << "experiment:\n"
         << "  name: legacy_confidence_config_test\n"
         << "  description: Legacy confidence config test\n"
         << "\n"
         << "input:\n"
         << "  video_path: \"C:/seq/video.mp4\"\n"
         << "  annotation_path: \"C:/seq/gt.txt\"\n"
         << "\n"
         << "output:\n"
         << "  save_video: 0\n"
         << "  video_path: \"run/out.mp4\"\n"
         << "\n"
         << "tracker:\n"
         << "  fixed_window: 0\n"
         << "  multiscale: 1\n"
         << "\n"
         << "features:\n"
         << "  hog:\n"
         << "    enabled: 1\n"
         << "    channels: 18\n"
         << "  cn:\n"
         << "    enabled: 0\n"
         << "    channels: 0\n"
         << "  lab:\n"
         << "    enabled: 0\n"
         << "  fusion:\n"
         << "    mode: concat\n"
         << "\n"
         << "confidence:\n"
         << "  enabled: 1\n"
         << "  low_displacement_threshold: 0.43\n"
         << "  low_displacement_damping: 0.27\n";
    file.close();

    AppConfig config;
    std::string error;
    const bool ok = loadAppConfig(path, config, error);
    std::remove(path.c_str());

    if (!ok) {
        std::cerr << "Failed to load legacy confidence config: " << error << std::endl;
        return false;
    }

    return nearlyEqual(config.confidence.hard_low_displacement_threshold, 0.43f) &&
           nearlyEqual(config.confidence.hard_low_position_damping, 0.27f);
}

}  // namespace

int main()
{
    if (!testFourValueAnnotationsUseLineNumber() ||
        !testFiveValueAnnotationsKeepExplicitFrameNumber() ||
        !testImageSequencePathFormatting() ||
        !testHeaderlessYamlConfigLoads() ||
        !testConfidenceYamlConfigLoads() ||
        !testLegacyLowDisplacementFieldsMapToHardLowFields()) {
        return 1;
    }

    std::cout << "Dataset IO tests passed." << std::endl;
    return 0;
}

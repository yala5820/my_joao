#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "app_config.hpp"
#include "annotation_loader.hpp"
#include "frame_source.hpp"

namespace {

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
           config.features.hog_channels == 31;
}

}  // namespace

int main()
{
    if (!testFourValueAnnotationsUseLineNumber() ||
        !testFiveValueAnnotationsKeepExplicitFrameNumber() ||
        !testImageSequencePathFormatting() ||
        !testHeaderlessYamlConfigLoads()) {
        return 1;
    }

    std::cout << "Dataset IO tests passed." << std::endl;
    return 0;
}

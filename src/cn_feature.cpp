#include "cn_feature.hpp"

#include "cn_data.hpp"

#include <stdexcept>

namespace {

void validateCnArguments(const cv::Mat& bgr_patch,
                         int cell_size,
                         int output_channels)
{
    if (bgr_patch.empty() || bgr_patch.type() != CV_8UC3) {
        throw std::invalid_argument("CN feature extraction requires a non-empty CV_8UC3 BGR image.");
    }
    if (cell_size <= 0) {
        throw std::invalid_argument("CN feature extraction requires a positive cell size.");
    }
    if (output_channels != cn::kReducedChannels) {
        throw std::invalid_argument("CN feature extraction only supports 4 output channels.");
    }
    if (bgr_patch.cols < cell_size * 3 || bgr_patch.rows < cell_size * 3) {
        throw std::invalid_argument("CN feature extraction patch is too small for FHOG-aligned cells.");
    }
}

void projectCn11ToCn4(const float* cn11, float* cn4)
{
    const float* mean = cn::pcaMean();
    for (int channel = 0; channel < cn::kReducedChannels; ++channel) {
        const float* projection = cn::pcaProjectionRow(channel);
        float value = 0.0f;
        for (int source = 0; source < cn::kColorNameChannels; ++source) {
            value += (cn11[source] - mean[source]) * projection[source];
        }
        cn4[channel] = value;
    }
}

}  // namespace

cv::Mat extractCnFeatureMap(const cv::Mat& bgr_patch,
                            int cell_size,
                            int output_channels,
                            float weight)
{
    validateCnArguments(bgr_patch, cell_size, output_channels);

    const int cells_x = bgr_patch.cols / cell_size - 2;
    const int cells_y = bgr_patch.rows / cell_size - 2;
    if (cells_x <= 0 || cells_y <= 0) {
        throw std::invalid_argument("CN feature extraction produced no FHOG-aligned cells.");
    }

    cv::Mat output(output_channels, cells_x * cells_y, CV_32F, cv::Scalar(0.0f));
    int cell_index = 0;
    for (int c_y = cell_size; c_y < bgr_patch.rows - cell_size; c_y += cell_size) {
        if (cell_index >= cells_x * cells_y) {
            break;
        }
        for (int c_x = cell_size; c_x < bgr_patch.cols - cell_size; c_x += cell_size) {
            if (cell_index >= cells_x * cells_y) {
                break;
            }

            float cn11[cn::kColorNameChannels] = {0.0f};
            for (int y = c_y; y < c_y + cell_size; ++y) {
                for (int x = c_x; x < c_x + cell_size; ++x) {
                    const cv::Vec3b pixel = bgr_patch.at<cv::Vec3b>(y, x);
                    const int b_bin = pixel[0] >> 3;
                    const int g_bin = pixel[1] >> 3;
                    const int r_bin = pixel[2] >> 3;
                    const cn::ColorNameVector& cn_value =
                        cn::lookupColorName(r_bin, g_bin, b_bin);
                    for (int channel = 0; channel < cn::kColorNameChannels; ++channel) {
                        cn11[channel] += cn_value[channel];
                    }
                }
            }

            const float normalizer = 1.0f / static_cast<float>(cell_size * cell_size);
            for (int channel = 0; channel < cn::kColorNameChannels; ++channel) {
                cn11[channel] *= normalizer;
            }

            float cn4[cn::kReducedChannels] = {0.0f};
            projectCn11ToCn4(cn11, cn4);
            for (int channel = 0; channel < output_channels; ++channel) {
                output.at<float>(channel, cell_index) = cn4[channel] * weight;
            }
            ++cell_index;
        }
    }

    if (cell_index != cells_x * cells_y) {
        throw std::invalid_argument("CN feature extraction cell count does not match FHOG geometry.");
    }
    return output;
}

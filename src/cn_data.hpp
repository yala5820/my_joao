#pragma once

#include <array>

namespace cn {

const int kColorNameChannels = 11;
const int kReducedChannels = 4;
const int kQuantizationBins = 32;

typedef std::array<float, kColorNameChannels> ColorNameVector;

const ColorNameVector& lookupColorName(int r_bin, int g_bin, int b_bin);
const float* pcaMean();
const float* pcaProjectionRow(int row);

}  // namespace cn

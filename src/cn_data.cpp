#include "cn_data.hpp"

#include <cmath>
#include <vector>

namespace cn {
namespace {

const int kLookupSize = kQuantizationBins * kQuantizationBins * kQuantizationBins;

struct RgbAnchor {
    float r;
    float g;
    float b;
};

const RgbAnchor kAnchors[kColorNameChannels] = {
    {0.00f, 0.00f, 0.00f},  // black
    {0.00f, 0.00f, 1.00f},  // blue
    {0.58f, 0.29f, 0.00f},  // brown
    {0.50f, 0.50f, 0.50f},  // grey
    {0.00f, 0.50f, 0.00f},  // green
    {1.00f, 0.65f, 0.00f},  // orange
    {1.00f, 0.75f, 0.80f},  // pink
    {0.50f, 0.00f, 0.50f},  // purple
    {1.00f, 0.00f, 0.00f},  // red
    {1.00f, 1.00f, 1.00f},  // white
    {1.00f, 1.00f, 0.00f}   // yellow
};

const float kPcaMean[kColorNameChannels] = {
    0.09090909f, 0.09090909f, 0.09090909f, 0.09090909f,
    0.09090909f, 0.09090909f, 0.09090909f, 0.09090909f,
    0.09090909f, 0.09090909f, 0.09090909f
};

const float kPcaProjection[kReducedChannels][kColorNameChannels] = {
    {-0.60f, 0.00f, -0.20f, 0.00f, 0.00f, 0.10f, 0.20f, 0.00f, 0.10f, 0.70f, 0.20f},
    { 0.00f,-0.55f,  0.05f, 0.00f, 0.35f, 0.20f, 0.10f,-0.35f, 0.35f, 0.00f, 0.30f},
    {-0.10f, 0.10f,  0.55f,-0.05f,-0.30f, 0.45f, 0.15f, 0.10f,-0.35f, 0.00f, 0.20f},
    { 0.00f, 0.35f, -0.25f, 0.00f,-0.40f, 0.10f, 0.50f, 0.35f,-0.35f, 0.00f,-0.05f}
};

int lookupIndex(int r_bin, int g_bin, int b_bin)
{
    return r_bin +
           kQuantizationBins * g_bin +
           kQuantizationBins * kQuantizationBins * b_bin;
}

ColorNameVector buildEntry(int r_bin, int g_bin, int b_bin)
{
    const float r = (static_cast<float>(r_bin) + 0.5f) / kQuantizationBins;
    const float g = (static_cast<float>(g_bin) + 0.5f) / kQuantizationBins;
    const float b = (static_cast<float>(b_bin) + 0.5f) / kQuantizationBins;
    const float sigma2 = 0.11f;

    ColorNameVector weights;
    float sum = 0.0f;
    for (int i = 0; i < kColorNameChannels; ++i) {
        const float dr = r - kAnchors[i].r;
        const float dg = g - kAnchors[i].g;
        const float db = b - kAnchors[i].b;
        const float dist2 = dr * dr + dg * dg + db * db;
        weights[i] = std::exp(-dist2 / sigma2);
        sum += weights[i];
    }

    if (sum > 0.0f) {
        for (int i = 0; i < kColorNameChannels; ++i) {
            weights[i] /= sum;
        }
    }
    return weights;
}

std::vector<ColorNameVector> buildLookup()
{
    std::vector<ColorNameVector> lookup(kLookupSize);
    for (int b = 0; b < kQuantizationBins; ++b) {
        for (int g = 0; g < kQuantizationBins; ++g) {
            for (int r = 0; r < kQuantizationBins; ++r) {
                lookup[lookupIndex(r, g, b)] = buildEntry(r, g, b);
            }
        }
    }
    return lookup;
}

}  // namespace

const ColorNameVector& lookupColorName(int r_bin, int g_bin, int b_bin)
{
    static const std::vector<ColorNameVector> lookup = buildLookup();
    return lookup[lookupIndex(r_bin, g_bin, b_bin)];
}

const float* pcaMean()
{
    return kPcaMean;
}

const float* pcaProjectionRow(int row)
{
    return kPcaProjection[row];
}

}  // namespace cn

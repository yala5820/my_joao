#include <cmath>
#include <iostream>

#include "fhog.hpp"

namespace {

bool nearlyEqual(float a, float b)
{
    return std::fabs(a - b) < 1e-6f;
}

CvLSVMFeatureMapCaskade* createSequentialMap(int sizeX, int sizeY, int features)
{
    CvLSVMFeatureMapCaskade* map = 0;
    allocFeatureMapObject(&map, sizeX, sizeY, features);
    for (int i = 0; i < sizeX * sizeY * features; ++i) {
        map->map[i] = static_cast<float>(i + 1);
    }
    return map;
}

bool testSelectFirst18Channels()
{
    CvLSVMFeatureMapCaskade* map = createSequentialMap(2, 2, 31);

    const int status = selectFeatureMapChannels(map, 18);
    if (status != LATENT_SVM_OK) {
        std::cerr << "selectFeatureMapChannels returned " << status << std::endl;
        freeFeatureMapObject(&map);
        return false;
    }
    if (map->numFeatures != 18 || map->sizeX != 2 || map->sizeY != 2) {
        std::cerr << "Unexpected feature map shape after selection." << std::endl;
        freeFeatureMapObject(&map);
        return false;
    }

    bool ok = true;
    for (int cell = 0; cell < 4; ++cell) {
        for (int channel = 0; channel < 18; ++channel) {
            const int newIndex = cell * 18 + channel;
            const int oldIndex = cell * 31 + channel;
            ok = ok && nearlyEqual(map->map[newIndex], static_cast<float>(oldIndex + 1));
        }
    }

    freeFeatureMapObject(&map);
    return ok;
}

bool testSelectAllChannelsIsNoOp()
{
    CvLSVMFeatureMapCaskade* map = createSequentialMap(2, 1, 31);
    float* originalData = map->map;

    const int status = selectFeatureMapChannels(map, 31);
    const bool ok = status == LATENT_SVM_OK &&
                    map->numFeatures == 31 &&
                    map->sizeX == 2 &&
                    map->sizeY == 1 &&
                    map->map == originalData &&
                    nearlyEqual(map->map[0], 1.0f) &&
                    nearlyEqual(map->map[61], 62.0f);

    freeFeatureMapObject(&map);
    return ok;
}

bool testInvalidChannelCountsAreRejected()
{
    bool ok = true;

    CvLSVMFeatureMapCaskade* zeroMap = createSequentialMap(1, 1, 31);
    ok = ok && selectFeatureMapChannels(zeroMap, 0) == FILTER_OUT_OF_BOUNDARIES;
    ok = ok && zeroMap->numFeatures == 31;
    freeFeatureMapObject(&zeroMap);

    CvLSVMFeatureMapCaskade* negativeMap = createSequentialMap(1, 1, 31);
    ok = ok && selectFeatureMapChannels(negativeMap, -1) == FILTER_OUT_OF_BOUNDARIES;
    ok = ok && negativeMap->numFeatures == 31;
    freeFeatureMapObject(&negativeMap);

    CvLSVMFeatureMapCaskade* tooLargeMap = createSequentialMap(1, 1, 31);
    ok = ok && selectFeatureMapChannels(tooLargeMap, 32) == FILTER_OUT_OF_BOUNDARIES;
    ok = ok && tooLargeMap->numFeatures == 31;
    freeFeatureMapObject(&tooLargeMap);

    return ok;
}

bool testNullMapsAreRejected()
{
    bool ok = selectFeatureMapChannels(0, 18) == LATENT_SVM_MEM_NULL;

    CvLSVMFeatureMapCaskade mapWithoutData;
    mapWithoutData.sizeX = 1;
    mapWithoutData.sizeY = 1;
    mapWithoutData.numFeatures = 31;
    mapWithoutData.map = 0;
    ok = ok && selectFeatureMapChannels(&mapWithoutData, 18) == LATENT_SVM_MEM_NULL;

    return ok;
}

}  // namespace

int main()
{
    if (!testSelectFirst18Channels() ||
        !testSelectAllChannelsIsNoOp() ||
        !testInvalidChannelCountsAreRejected() ||
        !testNullMapsAreRejected()) {
        return 1;
    }

    std::cout << "HOG feature tests passed." << std::endl;
    return 0;
}

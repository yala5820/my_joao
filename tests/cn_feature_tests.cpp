#include <cmath>
#include <iostream>
#include <stdexcept>

#include <opencv2/core.hpp>

#include "cn_feature.hpp"

namespace {

bool nearlyEqual(float a, float b)
{
    return std::fabs(a - b) < 1e-6f;
}

bool matricesEqual(const cv::Mat& lhs, const cv::Mat& rhs)
{
    if (lhs.rows != rhs.rows || lhs.cols != rhs.cols || lhs.type() != rhs.type()) {
        return false;
    }

    for (int row = 0; row < lhs.rows; ++row) {
        for (int col = 0; col < lhs.cols; ++col) {
            if (!nearlyEqual(lhs.at<float>(row, col), rhs.at<float>(row, col))) {
                return false;
            }
        }
    }
    return true;
}

bool testCn4ShapeMatchesFhogCells()
{
    const cv::Mat patch(16, 20, CV_8UC3, cv::Scalar(10, 80, 200));
    const cv::Mat cn = extractCnFeatureMap(patch, 4, 4);

    if (cn.rows != 4 || cn.cols != 6 || cn.type() != CV_32F) {
        std::cerr << "Unexpected CN feature shape: "
                  << cn.rows << "x" << cn.cols << std::endl;
        return false;
    }
    return true;
}

bool testCn4ExtractionIsDeterministic()
{
    cv::Mat patch(16, 16, CV_8UC3);
    for (int y = 0; y < patch.rows; ++y) {
        for (int x = 0; x < patch.cols; ++x) {
            patch.at<cv::Vec3b>(y, x) =
                cv::Vec3b(static_cast<unsigned char>(x * 11),
                          static_cast<unsigned char>(y * 13),
                          static_cast<unsigned char>((x + y) * 7));
        }
    }

    const cv::Mat first = extractCnFeatureMap(patch, 4, 4);
    const cv::Mat second = extractCnFeatureMap(patch, 4, 4);
    if (!matricesEqual(first, second)) {
        std::cerr << "CN extraction is not deterministic." << std::endl;
        return false;
    }
    return true;
}

bool testPureColorCellsAreEqual()
{
    const cv::Mat patch(16, 16, CV_8UC3, cv::Scalar(0, 0, 255));
    const cv::Mat cn = extractCnFeatureMap(patch, 4, 4);

    for (int col = 1; col < cn.cols; ++col) {
        for (int row = 0; row < cn.rows; ++row) {
            if (!nearlyEqual(cn.at<float>(row, 0), cn.at<float>(row, col))) {
                std::cerr << "Pure color cells differ." << std::endl;
                return false;
            }
        }
    }
    return true;
}

bool testInvalidArgumentsAreRejected()
{
    bool ok = true;

    try {
        (void)extractCnFeatureMap(cv::Mat(16, 16, CV_8UC3), 4, 3);
        ok = false;
    } catch (const std::invalid_argument&) {
    }

    try {
        (void)extractCnFeatureMap(cv::Mat(16, 16, CV_8UC3), 0, 4);
        ok = false;
    } catch (const std::invalid_argument&) {
    }

    try {
        (void)extractCnFeatureMap(cv::Mat(4, 4, CV_8UC1), 4, 4);
        ok = false;
    } catch (const std::invalid_argument&) {
    }

    return ok;
}

}  // namespace

int main()
{
    if (!testCn4ShapeMatchesFhogCells() ||
        !testCn4ExtractionIsDeterministic() ||
        !testPureColorCellsAreEqual() ||
        !testInvalidArgumentsAreRejected()) {
        return 1;
    }

    std::cout << "CN feature tests passed." << std::endl;
    return 0;
}

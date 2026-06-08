#include <cmath>
#include <iostream>
#include <string>

#include "confidence.hpp"

namespace {

bool almostEqual(float a, float b, float tolerance)
{
    return std::fabs(a - b) <= tolerance;
}

bool testSinglePeakHasHigherConfidenceThanFlatResponse()
{
    cv::Mat peaked(15, 15, CV_32F, cv::Scalar(0.1f));
    peaked.at<float>(7, 7) = 10.0f;

    cv::Mat flat(15, 15, CV_32F, cv::Scalar(0.1f));

    const ResponseStats peakedStats = computeResponseStats(peaked, 2, 1.0e-6f);
    const ResponseStats flatStats = computeResponseStats(flat, 2, 1.0e-6f);
    return peakedStats.peak > flatStats.peak &&
           peakedStats.psr > flatStats.psr &&
           peakedStats.apce > flatStats.apce;
}

bool testExclusionRadiusAffectsPsr()
{
    cv::Mat response(15, 15, CV_32F, cv::Scalar(0.1f));
    response.at<float>(7, 7) = 10.0f;
    response.at<float>(7, 8) = 8.0f;

    const ResponseStats radiusZero = computeResponseStats(response, 0, 1.0e-6f);
    const ResponseStats radiusTwo = computeResponseStats(response, 2, 1.0e-6f);
    return radiusTwo.psr > radiusZero.psr;
}

bool testWarmupInitializesEma()
{
    ConfidenceConfig config;
    config.enabled = true;
    config.warmup_frames = 2;

    ConfidenceEstimator estimator(config);
    ResponseStats first = {1.0f, 10.0f, 20.0f};
    ResponseStats second = {1.0f, 14.0f, 28.0f};

    ConfidenceDiagnostics d1 = estimator.evaluate(first);
    ConfidenceDiagnostics d2 = estimator.evaluate(second);

    return d1.confidence_level == ConfidenceLevel::Warmup &&
           d2.confidence_level == ConfidenceLevel::Warmup &&
           d2.ema_updated &&
           almostEqual(d2.psr_ema, 12.0f, 1.0e-5f) &&
           almostEqual(d2.apce_ema, 24.0f, 1.0e-5f);
}

bool testHighMediumSoftLowHardLowEmaRules()
{
    ConfidenceConfig config;
    config.enabled = true;
    config.warmup_frames = 1;
    config.ema_alpha = 0.1f;
    config.medium_ema_alpha = 0.0f;
    config.high_ratio = 0.95f;
    config.low_ratio = 0.60f;
    config.hard_low_ratio = 0.35f;

    ConfidenceEstimator estimator(config);
    estimator.evaluate({1.0f, 10.0f, 20.0f});

    ConfidenceDiagnostics high = estimator.evaluate({1.0f, 11.0f, 22.0f});
    ConfidenceDiagnostics medium = estimator.evaluate({1.0f, 8.0f, 16.0f});
    ConfidenceDiagnostics softLow = estimator.evaluate({1.0f, 5.0f, 12.0f});
    ConfidenceDiagnostics hardLow = estimator.evaluate({1.0f, 2.0f, 4.0f});

    return high.confidence_level == ConfidenceLevel::High &&
           high.ema_updated &&
           medium.confidence_level == ConfidenceLevel::Medium &&
           !medium.ema_updated &&
           softLow.confidence_level == ConfidenceLevel::SoftLow &&
           !softLow.ema_updated &&
           hardLow.confidence_level == ConfidenceLevel::HardLow &&
           !hardLow.ema_updated;
}

bool testSoftLowCanUpdateEmaWithLowAlpha()
{
    ConfidenceConfig config;
    config.enabled = true;
    config.warmup_frames = 1;
    config.high_ratio = 0.95f;
    config.low_ratio = 0.60f;
    config.hard_low_ratio = 0.35f;
    config.low_ema_alpha = 0.1f;

    ConfidenceEstimator estimator(config);
    estimator.evaluate({1.0f, 10.0f, 20.0f});

    ConfidenceDiagnostics softLow = estimator.evaluate({1.0f, 5.0f, 12.0f});

    return softLow.confidence_level == ConfidenceLevel::SoftLow &&
           softLow.ema_updated &&
           almostEqual(softLow.psr_ema, 9.5f, 1.0e-5f) &&
           almostEqual(softLow.apce_ema, 19.2f, 1.0e-5f);
}

bool testConfidenceLevelNames()
{
    return std::string(confidenceLevelName(ConfidenceLevel::SoftLow)) == "soft_low" &&
           std::string(confidenceLevelName(ConfidenceLevel::HardLow)) == "hard_low";
}

}  // namespace

int main()
{
    if (!testSinglePeakHasHigherConfidenceThanFlatResponse() ||
        !testExclusionRadiusAffectsPsr() ||
        !testWarmupInitializesEma() ||
        !testHighMediumSoftLowHardLowEmaRules() ||
        !testSoftLowCanUpdateEmaWithLowAlpha() ||
        !testConfidenceLevelNames()) {
        return 1;
    }

    std::cout << "Confidence tests passed." << std::endl;
    return 0;
}

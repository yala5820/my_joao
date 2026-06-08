#include <cmath>
#include <iostream>

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

bool testHighMediumLowEmaRules()
{
    ConfidenceConfig config;
    config.enabled = true;
    config.warmup_frames = 1;
    config.ema_alpha = 0.1f;
    config.medium_ema_alpha = 0.0f;
    config.high_ratio = 0.95f;
    config.low_ratio = 0.60f;

    ConfidenceEstimator estimator(config);
    estimator.evaluate({1.0f, 10.0f, 20.0f});

    ConfidenceDiagnostics high = estimator.evaluate({1.0f, 11.0f, 22.0f});
    ConfidenceDiagnostics medium = estimator.evaluate({1.0f, 8.0f, 16.0f});
    ConfidenceDiagnostics low = estimator.evaluate({1.0f, 3.0f, 6.0f});

    return high.confidence_level == ConfidenceLevel::High &&
           high.ema_updated &&
           medium.confidence_level == ConfidenceLevel::Medium &&
           !medium.ema_updated &&
           low.confidence_level == ConfidenceLevel::Low &&
           !low.ema_updated;
}

}  // namespace

int main()
{
    if (!testSinglePeakHasHigherConfidenceThanFlatResponse() ||
        !testExclusionRadiusAffectsPsr() ||
        !testWarmupInitializesEma() ||
        !testHighMediumLowEmaRules()) {
        return 1;
    }

    std::cout << "Confidence tests passed." << std::endl;
    return 0;
}

#include "confidence.hpp"

#include <algorithm>
#include <cmath>

namespace {

float safeEps(float eps)
{
    return eps > 0.0f ? eps : 1.0e-6f;
}

}  // namespace

ConfidenceEstimator::ConfidenceEstimator(const ConfidenceConfig& config)
    : config_(config),
      initialized_(false),
      warmup_count_(0),
      psr_sum_(0.0),
      apce_sum_(0.0),
      psr_ema_(0.0f),
      apce_ema_(0.0f)
{
}

void ConfidenceEstimator::reset()
{
    initialized_ = false;
    warmup_count_ = 0;
    psr_sum_ = 0.0;
    apce_sum_ = 0.0;
    psr_ema_ = 0.0f;
    apce_ema_ = 0.0f;
}

ConfidenceDiagnostics ConfidenceEstimator::evaluate(const ResponseStats& stats)
{
    ConfidenceDiagnostics diagnostics = makeDefaultConfidenceDiagnostics(config_.enabled);
    diagnostics.peak = stats.peak;
    diagnostics.psr = stats.psr;
    diagnostics.apce = stats.apce;

    if (!config_.enabled) {
        return diagnostics;
    }

    const int warmup_frames = std::max(1, config_.warmup_frames);
    if (!initialized_) {
        ++warmup_count_;
        psr_sum_ += stats.psr;
        apce_sum_ += stats.apce;
        diagnostics.confidence_level = ConfidenceLevel::Warmup;

        if (warmup_count_ >= warmup_frames) {
            psr_ema_ = static_cast<float>(psr_sum_ / warmup_count_);
            apce_ema_ = static_cast<float>(apce_sum_ / warmup_count_);
            initialized_ = true;
            diagnostics.ema_updated = true;
        }

        diagnostics.psr_ema = psr_ema_;
        diagnostics.apce_ema = apce_ema_;
        return diagnostics;
    }

    const bool high =
        stats.psr >= config_.high_ratio * psr_ema_ &&
        stats.apce >= config_.high_ratio * apce_ema_;
    const bool low =
        stats.psr < config_.low_ratio * psr_ema_ ||
        stats.apce < config_.low_ratio * apce_ema_;

    float alpha = 0.0f;
    if (high) {
        diagnostics.confidence_level = ConfidenceLevel::High;
        alpha = config_.ema_alpha;
    } else if (low) {
        diagnostics.confidence_level = ConfidenceLevel::Low;
    } else {
        diagnostics.confidence_level = ConfidenceLevel::Medium;
        alpha = config_.medium_ema_alpha;
    }

    if (alpha > 0.0f) {
        psr_ema_ = (1.0f - alpha) * psr_ema_ + alpha * stats.psr;
        apce_ema_ = (1.0f - alpha) * apce_ema_ + alpha * stats.apce;
        diagnostics.ema_updated = true;
    }

    diagnostics.psr_ema = psr_ema_;
    diagnostics.apce_ema = apce_ema_;
    return diagnostics;
}

const ConfidenceConfig& ConfidenceEstimator::config() const
{
    return config_;
}

ResponseStats computeResponseStats(const cv::Mat& response,
                                   int psr_exclusion_radius,
                                   float eps)
{
    CV_Assert(response.type() == CV_32F);
    CV_Assert(response.rows > 0 && response.cols > 0);

    double minValue = 0.0;
    double maxValue = 0.0;
    cv::Point maxLocation;
    cv::minMaxLoc(response, &minValue, &maxValue, 0, &maxLocation);

    const float peak = static_cast<float>(maxValue);
    const float minResponse = static_cast<float>(minValue);
    const float denominatorEps = safeEps(eps);
    const int radius = std::max(0, psr_exclusion_radius);

    double sidelobeSum = 0.0;
    double sidelobeSquaredSum = 0.0;
    int sidelobeCount = 0;

    double apceSquaredSum = 0.0;
    for (int y = 0; y < response.rows; ++y) {
        for (int x = 0; x < response.cols; ++x) {
            const float value = response.at<float>(y, x);
            const float apceDelta = value - minResponse;
            apceSquaredSum += static_cast<double>(apceDelta) * apceDelta;

            const bool inPeakWindow =
                std::abs(x - maxLocation.x) <= radius &&
                std::abs(y - maxLocation.y) <= radius;
            if (!inPeakWindow) {
                sidelobeSum += value;
                sidelobeSquaredSum += static_cast<double>(value) * value;
                ++sidelobeCount;
            }
        }
    }

    float psr = 0.0f;
    if (sidelobeCount > 0) {
        const double mean = sidelobeSum / sidelobeCount;
        const double variance = std::max(
            0.0, sidelobeSquaredSum / sidelobeCount - mean * mean);
        const double stddev = std::sqrt(variance);
        psr = static_cast<float>((peak - mean) / (stddev + denominatorEps));
    }

    const float peakDelta = peak - minResponse;
    const float apce = static_cast<float>(
        (static_cast<double>(peakDelta) * peakDelta) /
        (apceSquaredSum / (response.rows * response.cols) + denominatorEps));

    ResponseStats stats;
    stats.peak = peak;
    stats.psr = psr;
    stats.apce = apce;
    return stats;
}

const char* confidenceLevelName(ConfidenceLevel level)
{
    switch (level) {
    case ConfidenceLevel::Disabled:
        return "disabled";
    case ConfidenceLevel::Warmup:
        return "warmup";
    case ConfidenceLevel::High:
        return "high";
    case ConfidenceLevel::Medium:
        return "medium";
    case ConfidenceLevel::Low:
        return "low";
    }
    return "unknown";
}

const char* positionActionName(PositionAction action)
{
    switch (action) {
    case PositionAction::Disabled:
        return "disabled";
    case PositionAction::Accept:
        return "accept";
    case PositionAction::Damped:
        return "damped";
    case PositionAction::Reject:
        return "reject";
    }
    return "unknown";
}

ConfidenceDiagnostics makeDefaultConfidenceDiagnostics(bool enabled)
{
    ConfidenceDiagnostics diagnostics;
    diagnostics.enabled = enabled;
    diagnostics.peak = 0.0f;
    diagnostics.psr = 0.0f;
    diagnostics.apce = 0.0f;
    diagnostics.psr_ema = 0.0f;
    diagnostics.apce_ema = 0.0f;
    diagnostics.confidence_level =
        enabled ? ConfidenceLevel::Warmup : ConfidenceLevel::Disabled;
    diagnostics.displacement_ratio = 0.0f;
    diagnostics.effective_learning_rate = 0.0f;
    diagnostics.position_action =
        enabled ? PositionAction::Accept : PositionAction::Disabled;
    diagnostics.template_updated = false;
    diagnostics.scale_updated = false;
    diagnostics.ema_updated = false;
    return diagnostics;
}

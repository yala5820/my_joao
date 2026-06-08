#pragma once

#include <string>

#include <opencv2/core.hpp>

#include "app_config.hpp"

enum class ConfidenceLevel {
    Disabled,
    Warmup,
    High,
    Medium,
    Low
};

enum class PositionAction {
    Disabled,
    Accept,
    Damped,
    Reject
};

struct ResponseStats {
    float peak;
    float psr;
    float apce;
};

struct ConfidenceDiagnostics {
    bool enabled;
    float peak;
    float psr;
    float apce;
    float psr_ema;
    float apce_ema;
    ConfidenceLevel confidence_level;
    float displacement_ratio;
    float effective_learning_rate;
    PositionAction position_action;
    bool template_updated;
    bool scale_updated;
    bool ema_updated;
};

class ConfidenceEstimator {
public:
    explicit ConfidenceEstimator(const ConfidenceConfig& config = ConfidenceConfig());

    void reset();
    ConfidenceDiagnostics evaluate(const ResponseStats& stats);
    const ConfidenceConfig& config() const;

private:
    ConfidenceConfig config_;
    bool initialized_;
    int warmup_count_;
    double psr_sum_;
    double apce_sum_;
    float psr_ema_;
    float apce_ema_;
};

ResponseStats computeResponseStats(const cv::Mat& response,
                                   int psr_exclusion_radius,
                                   float eps);

const char* confidenceLevelName(ConfidenceLevel level);
const char* positionActionName(PositionAction action);

ConfidenceDiagnostics makeDefaultConfidenceDiagnostics(bool enabled);

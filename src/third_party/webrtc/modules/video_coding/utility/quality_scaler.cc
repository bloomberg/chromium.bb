/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/quality_scaler.h"

#include <memory>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/experiments/quality_scaler_settings.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/task_queue.h"

// TODO(kthelgason): Some versions of Android have issues with log2.
// See https://code.google.com/p/android/issues/detail?id=212634 for details
#if defined(WEBRTC_ANDROID)
#define log2(x) (log(x) / log(2))
#endif

namespace webrtc {

namespace {
// TODO(nisse): Delete, delegate to encoders.
// Threshold constant used until first downscale (to permit fast rampup).
static const int kMeasureMs = 2000;
static const float kSamplePeriodScaleFactor = 2.5;
static const int kFramedropPercentThreshold = 60;
static const size_t kMinFramesNeededToScale = 2 * 30;

}  // namespace

class QualityScaler::QpSmoother {
 public:
  explicit QpSmoother(float alpha)
      : alpha_(alpha),
        // The initial value of last_sample_ms doesn't matter since the smoother
        // will ignore the time delta for the first update.
        last_sample_ms_(0),
        smoother_(alpha) {}

  absl::optional<int> GetAvg() const {
    float value = smoother_.filtered();
    if (value == rtc::ExpFilter::kValueUndefined) {
      return absl::nullopt;
    }
    return static_cast<int>(value);
  }

  void Add(float sample, int64_t time_sent_us) {
    int64_t now_ms = time_sent_us / 1000;
    smoother_.Apply(static_cast<float>(now_ms - last_sample_ms_), sample);
    last_sample_ms_ = now_ms;
  }

  void Reset() { smoother_.Reset(alpha_); }

 private:
  const float alpha_;
  int64_t last_sample_ms_;
  rtc::ExpFilter smoother_;
};

QualityScaler::QualityScaler(rtc::TaskQueue* task_queue,
                             AdaptationObserverInterface* observer,
                             VideoEncoder::QpThresholds thresholds)
    : QualityScaler(task_queue, observer, thresholds, kMeasureMs) {}

// Protected ctor, should not be called directly.
QualityScaler::QualityScaler(rtc::TaskQueue* task_queue,
                             AdaptationObserverInterface* observer,
                             VideoEncoder::QpThresholds thresholds,
                             int64_t sampling_period_ms)
    : observer_(observer),
      thresholds_(thresholds),
      sampling_period_ms_(sampling_period_ms),
      fast_rampup_(true),
      // Arbitrarily choose size based on 30 fps for 5 seconds.
      average_qp_(5 * 30),
      framedrop_percent_media_opt_(5 * 30),
      framedrop_percent_all_(5 * 30),
      experiment_enabled_(QualityScalingExperiment::Enabled()),
      observed_enough_frames_(false),
      min_frames_needed_(
          QualityScalerSettings::ParseFromFieldTrials().MinFrames().value_or(
              kMinFramesNeededToScale)),
      initial_scale_factor_(QualityScalerSettings::ParseFromFieldTrials()
                                .InitialScaleFactor()
                                .value_or(kSamplePeriodScaleFactor)),
      scale_factor_(
          QualityScalerSettings::ParseFromFieldTrials().ScaleFactor()),
      adapt_called_(false),
      adapt_failed_(false) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (experiment_enabled_) {
    config_ = QualityScalingExperiment::GetConfig();
    qp_smoother_high_.reset(new QpSmoother(config_.alpha_high));
    qp_smoother_low_.reset(new QpSmoother(config_.alpha_low));
  }
  RTC_DCHECK(observer_ != nullptr);
  check_qp_task_ = RepeatingTaskHandle::DelayedStart(
      task_queue->Get(), TimeDelta::ms(GetSamplingPeriodMs()), [this]() {
        CheckQp();
        return TimeDelta::ms(GetSamplingPeriodMs());
      });
  RTC_LOG(LS_INFO) << "QP thresholds: low: " << thresholds_.low
                   << ", high: " << thresholds_.high;
}

QualityScaler::~QualityScaler() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  check_qp_task_.Stop();
}

int64_t QualityScaler::GetSamplingPeriodMs() const {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (fast_rampup_) {
    return sampling_period_ms_;
  }
  if (experiment_enabled_ && !observed_enough_frames_) {
    // Use half the interval while waiting for enough frames.
    return sampling_period_ms_ / 2;
  }
  if (adapt_failed_) {
    // Check shortly again.
    return sampling_period_ms_ / 8;
  }
  if (scale_factor_ && !adapt_called_) {
    // Last CheckQp did not call AdaptDown/Up, possibly reduce interval.
    return sampling_period_ms_ * scale_factor_.value();
  }
  return sampling_period_ms_ * initial_scale_factor_;
}

void QualityScaler::SetQpThresholds(VideoEncoder::QpThresholds thresholds) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  thresholds_ = thresholds;
}

void QualityScaler::ReportDroppedFrameByMediaOpt() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  framedrop_percent_media_opt_.AddSample(100);
  framedrop_percent_all_.AddSample(100);
}

void QualityScaler::ReportDroppedFrameByEncoder() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  framedrop_percent_all_.AddSample(100);
}

void QualityScaler::ReportQp(int qp, int64_t time_sent_us) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  framedrop_percent_media_opt_.AddSample(0);
  framedrop_percent_all_.AddSample(0);
  average_qp_.AddSample(qp);
  if (qp_smoother_high_)
    qp_smoother_high_->Add(qp, time_sent_us);
  if (qp_smoother_low_)
    qp_smoother_low_->Add(qp, time_sent_us);
}

void QualityScaler::CheckQp() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  // Should be set through InitEncode -> Should be set by now.
  RTC_DCHECK_GE(thresholds_.low, 0);
  adapt_failed_ = false;
  adapt_called_ = false;

  // If we have not observed at least this many frames we can't make a good
  // scaling decision.
  const size_t frames = config_.use_all_drop_reasons
                            ? framedrop_percent_all_.Size()
                            : framedrop_percent_media_opt_.Size();
  if (frames < min_frames_needed_) {
    observed_enough_frames_ = false;
    return;
  }
  observed_enough_frames_ = true;

  // Check if we should scale down due to high frame drop.
  const absl::optional<int> drop_rate =
      config_.use_all_drop_reasons
          ? framedrop_percent_all_.GetAverageRoundedDown()
          : framedrop_percent_media_opt_.GetAverageRoundedDown();
  if (drop_rate && *drop_rate >= kFramedropPercentThreshold) {
    RTC_LOG(LS_INFO) << "Reporting high QP, framedrop percent " << *drop_rate;
    ReportQpHigh();
    return;
  }

  // Check if we should scale up or down based on QP.
  const absl::optional<int> avg_qp_high =
      qp_smoother_high_ ? qp_smoother_high_->GetAvg()
                        : average_qp_.GetAverageRoundedDown();
  const absl::optional<int> avg_qp_low =
      qp_smoother_low_ ? qp_smoother_low_->GetAvg()
                       : average_qp_.GetAverageRoundedDown();
  if (avg_qp_high && avg_qp_low) {
    RTC_LOG(LS_INFO) << "Checking average QP " << *avg_qp_high << " ("
                     << *avg_qp_low << ").";
    if (*avg_qp_high > thresholds_.high) {
      ReportQpHigh();
      return;
    }
    if (*avg_qp_low <= thresholds_.low) {
      // QP has been low. We want to try a higher resolution.
      ReportQpLow();
      return;
    }
  }
}

void QualityScaler::ReportQpLow() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  ClearSamples();
  observer_->AdaptUp(AdaptationObserverInterface::AdaptReason::kQuality);
  adapt_called_ = true;
}

void QualityScaler::ReportQpHigh() {
  RTC_DCHECK_RUN_ON(&task_checker_);

  if (observer_->AdaptDown(
          AdaptationObserverInterface::AdaptReason::kQuality)) {
    ClearSamples();
  } else {
    adapt_failed_ = true;
  }

  // If we've scaled down, wait longer before scaling up again.
  if (fast_rampup_) {
    fast_rampup_ = false;
  }
  adapt_called_ = true;
}

void QualityScaler::ClearSamples() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  framedrop_percent_media_opt_.Reset();
  framedrop_percent_all_.Reset();
  average_qp_.Reset();
  if (qp_smoother_high_)
    qp_smoother_high_->Reset();
  if (qp_smoother_low_)
    qp_smoother_low_->Reset();
}
}  // namespace webrtc

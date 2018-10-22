// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/echo_information.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

namespace {

// Used to log echo quality based on delay estimates.
enum DelayBasedEchoQuality {
  DELAY_BASED_ECHO_QUALITY_GOOD = 0,
  DELAY_BASED_ECHO_QUALITY_SPURIOUS,
  DELAY_BASED_ECHO_QUALITY_BAD,
  DELAY_BASED_ECHO_QUALITY_INVALID,
  DELAY_BASED_ECHO_QUALITY_MAX = DELAY_BASED_ECHO_QUALITY_INVALID
};

DelayBasedEchoQuality EchoDelayFrequencyToQuality(float delay_frequency) {
  const float kEchoDelayFrequencyLowerLimit = 0.1f;
  const float kEchoDelayFrequencyUpperLimit = 0.8f;
  // DELAY_BASED_ECHO_QUALITY_GOOD
  //   delay is out of bounds during at most 10 % of the time.
  // DELAY_BASED_ECHO_QUALITY_SPURIOUS
  //   delay is out of bounds 10-80 % of the time.
  // DELAY_BASED_ECHO_QUALITY_BAD
  //   delay is mostly out of bounds >= 80 % of the time.
  // DELAY_BASED_ECHO_QUALITY_INVALID
  //   delay_frequency is negative which happens if we have insufficient data.
  if (delay_frequency < 0)
    return DELAY_BASED_ECHO_QUALITY_INVALID;
  else if (delay_frequency <= kEchoDelayFrequencyLowerLimit)
    return DELAY_BASED_ECHO_QUALITY_GOOD;
  else if (delay_frequency < kEchoDelayFrequencyUpperLimit)
    return DELAY_BASED_ECHO_QUALITY_SPURIOUS;
  else
    return DELAY_BASED_ECHO_QUALITY_BAD;
}

}  // namespace

EchoInformation::EchoInformation()
    : delay_stats_time_ms_(0),
      echo_frames_received_(false),
      divergent_filter_stats_time_ms_(0),
      num_divergent_filter_fraction_(0),
      num_non_zero_divergent_filter_fraction_(0) {}

EchoInformation::~EchoInformation() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReportAndResetAecDivergentFilterStats();
}

void EchoInformation::UpdateAecStats(
    webrtc::EchoCancellation* echo_cancellation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!echo_cancellation->is_enabled())
    return;

  UpdateAecDelayStats(echo_cancellation);
  UpdateAecDivergentFilterStats(echo_cancellation);
}

void EchoInformation::UpdateAecDelayStats(
    webrtc::EchoCancellation* echo_cancellation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Only start collecting stats if we know echo cancellation has measured an
  // echo. Otherwise we clutter the stats with for example cases where only the
  // microphone is used.
  if (!echo_frames_received_ & !echo_cancellation->stream_has_echo())
    return;

  echo_frames_received_ = true;

  // In WebRTC, three echo delay metrics are calculated and updated every
  // five seconds. We use one of them, |fraction_poor_delays| to log in a UMA
  // histogram an Echo Cancellation quality metric. The stat in WebRTC has a
  // fixed aggregation window of five seconds, so we use the same query
  // frequency to avoid logging old values.
  if (!echo_cancellation->is_delay_logging_enabled())
    return;

  delay_stats_time_ms_ += webrtc::AudioProcessing::kChunkSizeMs;
  if (delay_stats_time_ms_ <
      500 * webrtc::AudioProcessing::kChunkSizeMs) {  // 5 seconds
    return;
  }

  int dummy_median = 0, dummy_std = 0;
  float fraction_poor_delays = 0;
  if (echo_cancellation->GetDelayMetrics(&dummy_median, &dummy_std,
                                         &fraction_poor_delays) ==
      webrtc::AudioProcessing::kNoError) {
    delay_stats_time_ms_ = 0;
    // Map |fraction_poor_delays| to an Echo Cancellation quality and log in UMA
    // histogram. See DelayBasedEchoQuality for information on histogram
    // buckets.
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AecDelayBasedQuality",
                              EchoDelayFrequencyToQuality(fraction_poor_delays),
                              DELAY_BASED_ECHO_QUALITY_MAX + 1);
  }
}

void EchoInformation::UpdateAecDivergentFilterStats(
    webrtc::EchoCancellation* echo_cancellation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!echo_cancellation->are_metrics_enabled())
    return;

  divergent_filter_stats_time_ms_ += webrtc::AudioProcessing::kChunkSizeMs;
  if (divergent_filter_stats_time_ms_ <
      100 * webrtc::AudioProcessing::kChunkSizeMs) {  // 1 second
    return;
  }

  webrtc::EchoCancellation::Metrics metrics;
  if (echo_cancellation->GetMetrics(&metrics) ==
      webrtc::AudioProcessing::kNoError) {
    // If not yet calculated, |metrics.divergent_filter_fraction| is -1.0. After
    // being calculated the first time, it is updated periodically.
    if (metrics.divergent_filter_fraction < 0.0f) {
      DCHECK_EQ(num_divergent_filter_fraction_, 0);
      return;
    }
    if (metrics.divergent_filter_fraction > 0.0f) {
      ++num_non_zero_divergent_filter_fraction_;
    }
  } else {
    DLOG(WARNING) << "Get echo cancellation metrics failed.";
  }
  ++num_divergent_filter_fraction_;
  divergent_filter_stats_time_ms_ = 0;
}

void EchoInformation::ReportAndResetAecDivergentFilterStats() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (num_divergent_filter_fraction_ == 0)
    return;

  int non_zero_percent = 100 * num_non_zero_divergent_filter_fraction_ /
                         num_divergent_filter_fraction_;
  UMA_HISTOGRAM_PERCENTAGE("WebRTC.AecFilterHasDivergence", non_zero_percent);

  divergent_filter_stats_time_ms_ = 0;
  num_non_zero_divergent_filter_fraction_ = 0;
  num_divergent_filter_fraction_ = 0;
}

}  // namespace media

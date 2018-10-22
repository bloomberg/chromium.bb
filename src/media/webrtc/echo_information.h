// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_ECHO_INFORMATION_H_
#define MEDIA_WEBRTC_ECHO_INFORMATION_H_

#include "base/component_export.h"
#include "base/threading/thread_checker.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

// A helper class to log echo information in general and Echo Cancellation
// quality in particular.
class COMPONENT_EXPORT(MEDIA_WEBRTC) EchoInformation {
 public:
  EchoInformation();
  virtual ~EchoInformation();

  // Updates stats, and reports delay metrics as UMA stats every 5 seconds.
  // Must be called every time AudioProcessing::ProcessStream() is called.
  void UpdateAecStats(webrtc::EchoCancellation* echo_cancellation);

  // Reports AEC divergent filter metrics as UMA and resets the associated data.
  void ReportAndResetAecDivergentFilterStats();

 private:
  void UpdateAecDelayStats(webrtc::EchoCancellation* echo_cancellation);
  void UpdateAecDivergentFilterStats(
      webrtc::EchoCancellation* echo_cancellation);

  // Counter to track 5 seconds of data in order to query a new metric from
  // webrtc::EchoCancellation::GetEchoDelayMetrics().
  int delay_stats_time_ms_;
  bool echo_frames_received_;

  // Counter to track 1 second of data in order to query a new divergent filter
  // fraction metric from webrtc::EchoCancellation::GetMetrics().
  int divergent_filter_stats_time_ms_;

  // Total number of times we queried for the divergent filter fraction metric.
  int num_divergent_filter_fraction_;

  // Number of non-zero divergent filter fraction metrics.
  int num_non_zero_divergent_filter_fraction_;

  // Ensures that this class is accessed on the same thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(EchoInformation);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_ECHO_INFORMATION_H_

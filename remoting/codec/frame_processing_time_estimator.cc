// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/frame_processing_time_estimator.h"

#include "base/logging.h"
#include "remoting/base/constants.h"

namespace remoting {

namespace {

// We tracks the frame information in last 6 seconds.
static constexpr int kWindowSizeInSeconds = 6;

// A key-frame is assumed to be generated roughly every 3 seconds, though the
// accurate frequency is dependent on host/client software versions, the encoder
// being used, and the quality of the network.
static constexpr int kKeyFrameWindowSize = kWindowSizeInSeconds / 3;

// The count of delta frames we are tracking.
static constexpr int kDeltaFrameWindowSize =
    kTargetFrameRate * kWindowSizeInSeconds - kKeyFrameWindowSize;

}  // namespace

FrameProcessingTimeEstimator::FrameProcessingTimeEstimator()
    : delta_frame_processing_us_(kDeltaFrameWindowSize),
      delta_frame_size_(kDeltaFrameWindowSize),
      key_frame_processing_us_(kKeyFrameWindowSize),
      key_frame_size_(kKeyFrameWindowSize),
      bandwidth_kbps_(kDeltaFrameWindowSize + kKeyFrameWindowSize) {}

FrameProcessingTimeEstimator::~FrameProcessingTimeEstimator() = default;

void FrameProcessingTimeEstimator::StartFrame() {
  start_time_ = Now();
}

void FrameProcessingTimeEstimator::FinishFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame) {
  DCHECK(!start_time_.is_null());
  base::TimeTicks now = Now();
  if (frame.key_frame) {
    key_frame_processing_us_.Record((now - start_time_).InMicroseconds());
    key_frame_size_.Record(frame.data.length());
  } else {
    delta_frame_processing_us_.Record((now - start_time_).InMicroseconds());
    delta_frame_size_.Record(frame.data.length());
  }
  start_time_ = base::TimeTicks();
}

void FrameProcessingTimeEstimator::SetBandwidthKbps(int bandwidth_kbps) {
  if (bandwidth_kbps >= 0) {
    bandwidth_kbps_.Record(bandwidth_kbps);
  }
}

base::TimeDelta FrameProcessingTimeEstimator::EstimatedProcessingTime(
    bool key_frame) const {
  // Avoid returning 0 if there are no records for delta-frames.
  if ((key_frame && !key_frame_processing_us_.IsEmpty()) ||
      delta_frame_processing_us_.IsEmpty()) {
    return base::TimeDelta::FromMicroseconds(
        key_frame_processing_us_.Average());
  }
  return base::TimeDelta::FromMicroseconds(
      delta_frame_processing_us_.Average());
}

base::TimeDelta FrameProcessingTimeEstimator::EstimatedTransitTime(
    bool key_frame) const {
  if (bandwidth_kbps_.Average() == 0) {
    // To avoid unnecessary complexity in WebrtcFrameSchedulerSimple, we return
    // a fairly large value (1 minute) here. So WebrtcFrameSchedulerSimple does
    // not need to handle the overflow issue caused by returning
    // TimeDelta::Max().
    return base::TimeDelta::FromMinutes(1);
  }
  // Avoid returning 0 if there are no records for delta-frames.
  if ((key_frame && !key_frame_size_.IsEmpty()) ||
      delta_frame_size_.IsEmpty()) {
    return base::TimeDelta::FromMilliseconds(
        key_frame_size_.Average() * 8 / bandwidth_kbps_.Average());
  }
  return base::TimeDelta::FromMilliseconds(
      delta_frame_size_.Average() * 8 / bandwidth_kbps_.Average());
}

base::TimeTicks FrameProcessingTimeEstimator::Now() const {
  return base::TimeTicks::Now();
}

}  // namespace remoting

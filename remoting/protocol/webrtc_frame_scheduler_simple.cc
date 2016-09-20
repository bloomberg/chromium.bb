// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_simple.h"

#include <algorithm>

#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

namespace {

const int kTargetFrameRate = 30;
constexpr base::TimeDelta kTargetFrameInterval =
    base::TimeDelta::FromMilliseconds(1000 / kTargetFrameRate);

// Target quantizer at which stop the encoding top-off.
const int kTargetQuantizerForVp8TopOff = 30;

}  // namespace

WebrtcFrameSchedulerSimple::WebrtcFrameSchedulerSimple() {}
WebrtcFrameSchedulerSimple::~WebrtcFrameSchedulerSimple() {}

void WebrtcFrameSchedulerSimple::Start(const base::Closure& capture_callback) {
  capture_callback_ = capture_callback;
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::Pause(bool pause) {
  paused_ = pause;
  if (paused_) {
    capture_timer_.Stop();
  } else if (!capture_callback_.is_null()) {
    ScheduleNextFrame();
  }
}

void WebrtcFrameSchedulerSimple::SetKeyFrameRequest() {
  key_frame_request_ = true;
}

void WebrtcFrameSchedulerSimple::SetTargetBitrate(int bitrate_kbps) {
  target_bitrate_kbps_ = bitrate_kbps;
}

bool WebrtcFrameSchedulerSimple::GetEncoderFrameParams(
    const webrtc::DesktopFrame& frame,
    WebrtcVideoEncoder::FrameParams* params_out) {
  if (frame.updated_region().is_empty() && !top_off_is_active_ &&
      !key_frame_request_) {
    ScheduleNextFrame();
    return false;
  }

  params_out->bitrate_kbps = target_bitrate_kbps_;

  // TODO(sergeyu): Currently duration is always set to 1/15 of a second.
  // Experiment with different values, and try changing it dynamically.
  params_out->duration = base::TimeDelta::FromSeconds(1) / 15;

  params_out->key_frame = key_frame_request_;
  key_frame_request_ = false;

  params_out->vpx_min_quantizer = 10;
  params_out->vpx_max_quantizer = 63;

  params_out->clear_active_map = !top_off_is_active_;

  return true;
}

void WebrtcFrameSchedulerSimple::OnFrameEncoded(
    const WebrtcVideoEncoder::EncodedFrame& encoded_frame,
    const webrtc::EncodedImageCallback::Result& send_result) {
  if (encoded_frame.data.empty()) {
    top_off_is_active_ = false;
    ScheduleNextFrame();
    return;
  }

  // Top-off until the target quantizer value is reached.
  top_off_is_active_ = encoded_frame.quantizer > kTargetQuantizerForVp8TopOff;

  // Capture next frame after we finish sending the current one.
  const double kKiloBitsPerByte = 8.0 / 1000.0;
  base::TimeDelta expected_send_delay = base::TimeDelta::FromSecondsD(
      encoded_frame.data.size() * kKiloBitsPerByte / target_bitrate_kbps_);
  last_frame_send_finish_time_ = base::TimeTicks::Now() + expected_send_delay;

  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::ScheduleNextFrame() {
  if (paused_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay;

  // If this is not the first frame then capture next frame after the previous
  // one has finished sending.
  if (!last_frame_send_finish_time_.is_null()) {
    delay = std::max(base::TimeDelta(), last_frame_send_finish_time_ - now);
  }

  // Cap interval between frames to kTargetFrameInterval.
  if (!last_capture_started_time_.is_null()) {
    delay = std::max(delay,
                     last_capture_started_time_ + kTargetFrameInterval - now);
  }

  capture_timer_.Start(FROM_HERE, delay,
                       base::Bind(&WebrtcFrameSchedulerSimple::CaptureNextFrame,
                                  base::Unretained(this)));
}

void WebrtcFrameSchedulerSimple::CaptureNextFrame() {
  last_capture_started_time_ = base::TimeTicks::Now();
  capture_callback_.Run();
}

}  // namespace protocol
}  // namespace remoting

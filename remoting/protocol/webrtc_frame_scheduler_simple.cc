// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_simple.h"

#include <algorithm>

#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

namespace {

// Number of samples used to estimate processing time for the next frame.
const int kStatsWindow = 5;

const int kTargetFrameRate = 30;
constexpr base::TimeDelta kTargetFrameInterval =
    base::TimeDelta::FromMilliseconds(1000 / kTargetFrameRate);

// Target quantizer at which stop the encoding top-off.
const int kTargetQuantizerForVp8TopOff = 30;

const int64_t kPixelsPerMegapixel = 1000000;

// Minimum target bitrate per megapixel. The value is chosen experimentally such
// that when screen is not changing the codec converges to the target quantizer
// above in less than 10 frames.
const int kVp8MinimumTargetBitrateKbpsPerMegapixel = 2500;

// Threshold in number of updated pixels used to detect "big" frames. These
// frames update significant portion of the screen compared to the preceding
// frames. For these frames min quantizer may need to be adjusted in order to
// ensure that they get delivered to the client as soon as possible, in exchange
// for lower-quality image.
const int kBigFrameThresholdPixels = 300000;

// Estimated size (in bytes per megapixel) of encoded frame at target quantizer
// value (see kTargetQuantizerForVp8TopOff). Compression ratio varies depending
// on the image, so this is just a rough estimate. It's used to predict when
// encoded "big" frame may be too large to be delivered to the client quickly.
const int kEstimatedBytesPerMegapixel = 100000;

int64_t GetRegionArea(const webrtc::DesktopRegion& region) {
  int64_t result = 0;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    result += r.rect().width() * r.rect().height();
  }
  return result;
}

}  // namespace

WebrtcFrameSchedulerSimple::WebrtcFrameSchedulerSimple()
    : pacing_bucket_(LeakyBucket::kUnlimitedDepth, 0),
      frame_processing_delay_us_(kStatsWindow),
      updated_region_area_(kStatsWindow) {}
WebrtcFrameSchedulerSimple::~WebrtcFrameSchedulerSimple() {}

void WebrtcFrameSchedulerSimple::Start(const base::Closure& capture_callback) {
  capture_callback_ = capture_callback;
  ScheduleNextFrame(base::TimeTicks::Now());
}

void WebrtcFrameSchedulerSimple::Pause(bool pause) {
  paused_ = pause;
  if (paused_) {
    capture_timer_.Stop();
  } else {
    ScheduleNextFrame(base::TimeTicks::Now());
  }
}

void WebrtcFrameSchedulerSimple::SetKeyFrameRequest() {
  key_frame_request_ = true;
}

void WebrtcFrameSchedulerSimple::SetTargetBitrate(int bitrate_kbps) {
  base::TimeTicks now = base::TimeTicks::Now();
  pacing_bucket_.UpdateRate(bitrate_kbps * 1000 / 8, now);
  ScheduleNextFrame(now);
}

bool WebrtcFrameSchedulerSimple::GetEncoderFrameParams(
    const webrtc::DesktopFrame& frame,
    WebrtcVideoEncoder::FrameParams* params_out) {
  base::TimeTicks now = base::TimeTicks::Now();

  if (frame.updated_region().is_empty() && !top_off_is_active_ &&
      !key_frame_request_) {
    ScheduleNextFrame(now);
    return false;
  }

  // TODO(sergeyu): This logic is applicable only to VP8. Reconsider it for VP9.
  int minimum_bitrate =
      static_cast<int64_t>(kVp8MinimumTargetBitrateKbpsPerMegapixel) *
      frame.size().width() * frame.size().height() / 1000000LL;
  params_out->bitrate_kbps =
      std::max(minimum_bitrate, pacing_bucket_.rate() * 8 / 1000);

  params_out->duration = kTargetFrameInterval;
  params_out->key_frame = key_frame_request_;
  key_frame_request_ = false;

  params_out->vpx_min_quantizer = 10;

  int64_t updated_area = params_out->key_frame
                             ? frame.size().width() * frame.size().height()
                             : GetRegionArea(frame.updated_region());

  // If bandwidth is being underutilized then libvpx is likely to choose the
  // minimum allowed quantizer value, which means that encoded frame size may be
  // significantly bigger than the bandwidth allows. Detect this case and set
  // vpx_min_quantizer to 60. The quality will be topped off later.
  if (updated_area - updated_region_area_.Max() > kBigFrameThresholdPixels) {
    int expected_frame_size = updated_area *
                              kEstimatedBytesPerMegapixel / kPixelsPerMegapixel;
    base::TimeDelta expected_send_delay = base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * expected_frame_size /
        pacing_bucket_.rate());
    if (expected_send_delay > kTargetFrameInterval)
      params_out->vpx_min_quantizer = 60;
  }

  updated_region_area_.Record(updated_area);

  params_out->vpx_max_quantizer = 63;

  params_out->clear_active_map = !top_off_is_active_;

  return true;
}

void WebrtcFrameSchedulerSimple::OnFrameEncoded(
    const WebrtcVideoEncoder::EncodedFrame& encoded_frame,
    const webrtc::EncodedImageCallback::Result& send_result) {
  base::TimeTicks now = base::TimeTicks::Now();
  pacing_bucket_.RefillOrSpill(encoded_frame.data.size(), now);

  if (encoded_frame.data.empty()) {
    top_off_is_active_ = false;
  } else {
    frame_processing_delay_us_.Record(
        (now - last_capture_started_time_).InMicroseconds());

    // Top-off until the target quantizer value is reached.
    top_off_is_active_ = encoded_frame.quantizer > kTargetQuantizerForVp8TopOff;
  }

  ScheduleNextFrame(now);
}

void WebrtcFrameSchedulerSimple::ScheduleNextFrame(base::TimeTicks now) {
  // Don't capture frames when paused or target bitrate is 0 or there is
  // no capture callback set.
  if (paused_ || pacing_bucket_.rate() == 0 || capture_callback_.is_null())
    return;

  // If this is not the first frame then capture next frame after the previous
  // one has finished sending.
  base::TimeDelta expected_processing_time =
      base::TimeDelta::FromMicroseconds(frame_processing_delay_us_.Max());
  base::TimeTicks target_capture_time =
      pacing_bucket_.GetEmptyTime() - expected_processing_time;

  // Cap interval between frames to kTargetFrameInterval.
  if (!last_capture_started_time_.is_null()) {
    target_capture_time = std::max(
        target_capture_time, last_capture_started_time_ + kTargetFrameInterval);
  }

  if (target_capture_time < now)
    target_capture_time = now;

  capture_timer_.Start(FROM_HERE, target_capture_time - now,
                       base::Bind(&WebrtcFrameSchedulerSimple::CaptureNextFrame,
                                  base::Unretained(this)));
}

void WebrtcFrameSchedulerSimple::CaptureNextFrame() {
  last_capture_started_time_ = base::TimeTicks::Now();
  capture_callback_.Run();
}

}  // namespace protocol
}  // namespace remoting

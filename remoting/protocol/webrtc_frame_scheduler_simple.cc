// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_simple.h"

#include <algorithm>

#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
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

// Interval over which the bandwidth estimates is averaged to set target encoder
// bitrate.
constexpr base::TimeDelta kBandwidthAveragingInterval =
    base::TimeDelta::FromSeconds(1);

// Only update encoder bitrate when bandwidth changes by more than 33%. This
// value is chosen such that the codec is notified about significant changes in
// bandwidth, while ignoring bandwidth estimate noise. This is necessary because
// the encoder drops quality every time it's being reconfigured. When using VP8
// encoder in realtime mode encoded frame size correlates very poorly with the
// target bitrate, so it's not necessary to set target bitrate to match
// bandwidth exactly. Send bitrate is controlled more precisely by adjusting
// time intervals between frames (i.e. FPS).
const int kEncoderBitrateChangePercentage = 33;

// Minimum interval between frames needed to keep the connection alive.
constexpr base::TimeDelta kKeepAliveInterval =
    base::TimeDelta::FromMilliseconds(200);

int64_t GetRegionArea(const webrtc::DesktopRegion& region) {
  int64_t result = 0;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    result += r.rect().width() * r.rect().height();
  }
  return result;
}

}  // namespace

WebrtcFrameSchedulerSimple::EncoderBitrateFilter::EncoderBitrateFilter() {}
WebrtcFrameSchedulerSimple::EncoderBitrateFilter::~EncoderBitrateFilter() {}

void WebrtcFrameSchedulerSimple::EncoderBitrateFilter::SetBandwidthEstimate(
    int bandwidth_kbps,
    base::TimeTicks now) {
  while (!bandwidth_samples_.empty() &&
         now - bandwidth_samples_.front().first > kBandwidthAveragingInterval) {
    bandwidth_samples_sum_ -= bandwidth_samples_.front().second;
    bandwidth_samples_.pop();
  }

  bandwidth_samples_.push(std::make_pair(now, bandwidth_kbps));
  bandwidth_samples_sum_ += bandwidth_kbps;

  UpdateTargetBitrate();
}

void WebrtcFrameSchedulerSimple::EncoderBitrateFilter::SetFrameSize(
    webrtc::DesktopSize size) {
  // TODO(sergeyu): This logic is applicable only to VP8. Reconsider it for VP9.
  minimum_bitrate_ =
      static_cast<int64_t>(kVp8MinimumTargetBitrateKbpsPerMegapixel) *
      size.width() * size.height() / 1000000LL;

  UpdateTargetBitrate();
}

int WebrtcFrameSchedulerSimple::EncoderBitrateFilter::GetTargetBitrateKbps()
    const {
  DCHECK_GT(current_target_bitrate_, 0);
  return current_target_bitrate_;
}

void WebrtcFrameSchedulerSimple::EncoderBitrateFilter::UpdateTargetBitrate() {
  if (bandwidth_samples_.empty()) {
    return;
  }

  int bandwidth_estimate = bandwidth_samples_sum_ / bandwidth_samples_.size();
  int target_bitrate = std::max(minimum_bitrate_, bandwidth_estimate);

  // Update encoder bitrate only when it changes by more than 30%. This is
  // necessary because the encoder resets internal state when it's reconfigured
  // and this causes visible drop in quality.
  if (current_target_bitrate_ == 0 ||
      std::abs(target_bitrate - current_target_bitrate_) >
          current_target_bitrate_ * kEncoderBitrateChangePercentage / 100) {
    current_target_bitrate_ = target_bitrate;
  }
}

WebrtcFrameSchedulerSimple::WebrtcFrameSchedulerSimple()
    : pacing_bucket_(LeakyBucket::kUnlimitedDepth, 0),
      frame_processing_delay_us_(kStatsWindow),
      updated_region_area_(kStatsWindow),
      weak_factory_(this) {}

WebrtcFrameSchedulerSimple::~WebrtcFrameSchedulerSimple() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcFrameSchedulerSimple::OnKeyFrameRequested() {
  DCHECK(thread_checker_.CalledOnValidThread());
  encoder_ready_ = true;
  key_frame_request_ = true;
  ScheduleNextFrame(base::TimeTicks::Now());
}

void WebrtcFrameSchedulerSimple::OnChannelParameters(int packet_loss,
                                                     base::TimeDelta rtt) {
  DCHECK(thread_checker_.CalledOnValidThread());

  rtt_estimate_ = rtt;
}

void WebrtcFrameSchedulerSimple::OnTargetBitrateChanged(int bandwidth_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::TimeTicks now = base::TimeTicks::Now();
  pacing_bucket_.UpdateRate(bandwidth_kbps * 1000 / 8, now);
  encoder_bitrate_.SetBandwidthEstimate(bandwidth_kbps, now);
  ScheduleNextFrame(now);
}

void WebrtcFrameSchedulerSimple::Start(
    WebrtcDummyVideoEncoderFactory* video_encoder_factory,
    const base::Closure& capture_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_callback_ = capture_callback;
  video_encoder_factory->SetVideoChannelStateObserver(
      weak_factory_.GetWeakPtr());
}

void WebrtcFrameSchedulerSimple::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  paused_ = pause;
  if (paused_) {
    capture_timer_.Stop();
  } else {
    ScheduleNextFrame(base::TimeTicks::Now());
  }
}

bool WebrtcFrameSchedulerSimple::OnFrameCaptured(
    const webrtc::DesktopFrame* frame,
    WebrtcVideoEncoder::FrameParams* params_out) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeTicks now = base::TimeTicks::Now();

  // Null |frame| indicates a capturer error.
  if (!frame) {
    frame_pending_ = false;
    ScheduleNextFrame(now);
    return false;
  }

  if (frame->updated_region().is_empty()) {
    // If we've captured an empty frame we still need to encode and send the
    // previous frame when top-off is active or a key-frame was requested. But
    // it makes sense only when we have a frame to send, i.e. there is nothing
    // to send if first capture request failed.
    // Also send previous frame if there haven't been any frame updates for a
    // while, to keep the video stream alive. Otherwise, the client will
    // think the video stream is frozen and will attempt to recover it by
    // requesting a key-frame every few seconds, wasting network resources.
    bool send_frame =
        top_off_is_active_ || key_frame_request_ ||
        (now - latest_frame_encode_start_time_ > kKeepAliveInterval);
    if (!send_frame) {
      frame_pending_ = false;
      ScheduleNextFrame(now);
      return false;
    }
  }

  latest_frame_encode_start_time_ = now;

  encoder_bitrate_.SetFrameSize(frame->size());

  params_out->bitrate_kbps = encoder_bitrate_.GetTargetBitrateKbps();
  params_out->duration = kTargetFrameInterval;
  params_out->key_frame = key_frame_request_;
  key_frame_request_ = false;

  params_out->vpx_min_quantizer = 10;

  int64_t updated_area = params_out->key_frame
                             ? frame->size().width() * frame->size().height()
                             : GetRegionArea(frame->updated_region());

  // If bandwidth is being underutilized then libvpx is likely to choose the
  // minimum allowed quantizer value, which means that encoded frame size may
  // be significantly bigger than the bandwidth allows. Detect this case and set
  // vpx_min_quantizer to 60. The quality will be topped off later.
  if (updated_area - updated_region_area_.Max() > kBigFrameThresholdPixels) {
    int expected_frame_size =
        updated_area * kEstimatedBytesPerMegapixel / kPixelsPerMegapixel;
    base::TimeDelta expected_send_delay = base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * expected_frame_size /
        pacing_bucket_.rate());
    if (expected_send_delay > kTargetFrameInterval) {
      params_out->vpx_min_quantizer = 60;
    }
  }

  updated_region_area_.Record(updated_area);

  params_out->vpx_max_quantizer = 63;

  params_out->clear_active_map = !top_off_is_active_;

  return true;
}

void WebrtcFrameSchedulerSimple::OnFrameEncoded(
    const WebrtcVideoEncoder::EncodedFrame* encoded_frame,
    HostFrameStats* frame_stats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame_pending_);
  frame_pending_ = false;

  base::TimeTicks now = base::TimeTicks::Now();

  if (frame_stats) {
    // Calculate |send_pending_delay| before refilling |pacing_bucket_|.
    frame_stats->send_pending_delay =
        std::max(base::TimeDelta(), pacing_bucket_.GetEmptyTime() - now);
  }

  if (!encoded_frame || encoded_frame->data.empty()) {
    top_off_is_active_ = false;
  } else {
    pacing_bucket_.RefillOrSpill(encoded_frame->data.size(), now);

    frame_processing_delay_us_.Record(
        (now - last_capture_started_time_).InMicroseconds());

    // Top-off until the target quantizer value is reached.
    top_off_is_active_ =
        encoded_frame->quantizer > kTargetQuantizerForVp8TopOff;
  }

  ScheduleNextFrame(now);

  if (frame_stats) {
    frame_stats->rtt_estimate = rtt_estimate_;
    frame_stats->bandwidth_estimate_kbps = pacing_bucket_.rate() * 8 / 1000;
  }
}

void WebrtcFrameSchedulerSimple::ScheduleNextFrame(base::TimeTicks now) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!encoder_ready_ || paused_ || pacing_bucket_.rate() == 0 ||
      capture_callback_.is_null() || frame_pending_) {
    return;
  }

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

  if (target_capture_time < now) {
    target_capture_time = now;
  }

  capture_timer_.Start(FROM_HERE, target_capture_time - now,
                       base::Bind(&WebrtcFrameSchedulerSimple::CaptureNextFrame,
                                  base::Unretained(this)));
}

void WebrtcFrameSchedulerSimple::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!frame_pending_);
  last_capture_started_time_ = base::TimeTicks::Now();
  frame_pending_ = true;
  capture_callback_.Run();
}

}  // namespace protocol
}  // namespace remoting

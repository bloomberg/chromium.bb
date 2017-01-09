// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include <queue>

#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/base/leaky_bucket.h"
#include "remoting/base/running_samples.h"
#include "remoting/protocol/video_channel_state_observer.h"

namespace remoting {
namespace protocol {

// WebrtcFrameSchedulerSimple is a simple implementation of WebrtcFrameScheduler
// that always keeps only one frame in the pipeline. It schedules each frame
// such that it is encoded and ready to be sent by the time previous one is
// expected to finish sending.
class WebrtcFrameSchedulerSimple : public VideoChannelStateObserver,
                                   public WebrtcFrameScheduler {
 public:
  WebrtcFrameSchedulerSimple();
  ~WebrtcFrameSchedulerSimple() override;

  // VideoChannelStateObserver implementation.
  void OnKeyFrameRequested() override;
  void OnChannelParameters(int packet_loss, base::TimeDelta rtt) override;
  void OnTargetBitrateChanged(int bitrate_kbps) override;

  // WebrtcFrameScheduler implementation.
  void Start(WebrtcDummyVideoEncoderFactory* video_encoder_factory,
             const base::Closure& capture_callback) override;
  void Pause(bool pause) override;
  bool OnFrameCaptured(const webrtc::DesktopFrame* frame,
                       WebrtcVideoEncoder::FrameParams* params_out) override;
  void OnFrameEncoded(const WebrtcVideoEncoder::EncodedFrame* encoded_frame,
                      HostFrameStats* frame_stats) override;

 private:
  // Helper class used to calculate target encoder bitrate.
  class EncoderBitrateFilter {
   public:
    EncoderBitrateFilter();
    ~EncoderBitrateFilter();

    void SetBandwidthEstimate(int bandwidth_kbps, base::TimeTicks now);
    void SetFrameSize(webrtc::DesktopSize size);
    int GetTargetBitrateKbps() const;

   private:
    void UpdateTargetBitrate();

    std::queue<std::pair<base::TimeTicks, int>> bandwidth_samples_;
    int bandwidth_samples_sum_ = 0;

    int minimum_bitrate_ = 0;
    int current_target_bitrate_ = 0;
  };

  void ScheduleNextFrame(base::TimeTicks now);
  void CaptureNextFrame();

  base::Closure capture_callback_;
  bool paused_ = false;
  bool key_frame_request_ = false;

  base::TimeTicks last_capture_started_time_;

  LeakyBucket pacing_bucket_;

  EncoderBitrateFilter encoder_bitrate_;

  // Set to true when a frame is being captured or encoded.
  bool frame_pending_ = false;

  base::TimeDelta rtt_estimate_;

  // Set to true when encoding unchanged frames for top-off.
  bool top_off_is_active_ = false;

  // Accumulator for capture and encoder delay history.
  RunningSamples frame_processing_delay_us_;

  // Accumulator for updated region area in the previously encoded frames.
  RunningSamples updated_region_area_;

  base::OneShotTimer capture_timer_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<WebrtcFrameSchedulerSimple> weak_factory_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

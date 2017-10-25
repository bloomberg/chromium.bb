// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_FRAME_PROCESSING_TIME_ESTIMATOR_H_
#define REMOTING_CODEC_FRAME_PROCESSING_TIME_ESTIMATOR_H_

#include "base/time/time.h"
#include "remoting/base/running_samples.h"
#include "remoting/codec/webrtc_video_encoder.h"

namespace remoting {

// Estimator of frame processing time. It considers both frame size and
// bandwidth, and provides an estimation of the time required from capturing to
// reaching the client.
class FrameProcessingTimeEstimator {
 public:
  FrameProcessingTimeEstimator();
  virtual ~FrameProcessingTimeEstimator();

  // Marks the start of capturing a frame. StartFrame() and FinishFrame() should
  // be called in pairs.
  void StartFrame();

  // Marks the finish of encoding a frame.
  void FinishFrame(const WebrtcVideoEncoder::EncodedFrame& frame);

  // Sets the estimated network bandwidth. Negative |bandwidth_kbps| will be
  // ignored.
  void SetBandwidthKbps(int bandwidth_kbps);

  // Returns the estimated processing time of a frame. The TimeDelta includes
  // both capturing and encoding time.
  base::TimeDelta EstimatedProcessingTime(bool key_frame) const;

  // Returns the estimated transit time of a frame.
  base::TimeDelta EstimatedTransitTime(bool key_frame) const;

 private:
  // A virtual function to replace base::TimeTicks::Now() for testing.
  virtual base::TimeTicks Now() const;

  RunningSamples delta_frame_processing_us_;
  RunningSamples delta_frame_size_;
  RunningSamples key_frame_processing_us_;
  RunningSamples key_frame_size_;

  RunningSamples bandwidth_kbps_;

  // The time when last StartFrame() is called.
  base::TimeTicks start_time_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_FRAME_PROCESSING_TIME_ESTIMATOR_H_

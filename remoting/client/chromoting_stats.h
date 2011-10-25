// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingStats defines a bundle of performance counters and statistics
// for chromoting.

#ifndef REMOTING_CLIENT_CHROMOTING_STATS_H_
#define REMOTING_CLIENT_CHROMOTING_STATS_H_

#include "remoting/base/rate_counter.h"
#include "remoting/base/running_average.h"

namespace remoting {

class ChromotingStats {
 public:
  ChromotingStats();
  virtual ~ChromotingStats();

  RateCounter* video_bandwidth() { return &video_bandwidth_; }
  RateCounter* video_frame_rate() { return &video_frame_rate_; }
  RunningAverage* video_capture_ms() { return &video_capture_ms_; }
  RunningAverage* video_encode_ms() { return &video_encode_ms_; }
  RunningAverage* video_decode_ms() { return &video_decode_ms_; }
  RunningAverage* video_paint_ms() { return &video_paint_ms_; }
  RunningAverage* round_trip_ms() { return &round_trip_ms_; }

 private:
  RateCounter video_bandwidth_;
  RateCounter video_frame_rate_;
  RunningAverage video_capture_ms_;
  RunningAverage video_encode_ms_;
  RunningAverage video_decode_ms_;
  RunningAverage video_paint_ms_;
  RunningAverage round_trip_ms_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingStats);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_STATS_H_

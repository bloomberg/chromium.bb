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

  // Constant used to calculate the average for rate metrics and used by the
  // plugin for the frequency at which stats should be updated.
  static const int kStatsUpdateFrequencyInSeconds = 1;

  // The video and packet rate metrics below are updated per video packet
  // received and then, for reporting, averaged over a 1s time-window.
  // Bytes per second for non-empty video-packets.
  RateCounter* video_bandwidth() { return &video_bandwidth_; }

  // Frames per second for non-empty video-packets.
  RateCounter* video_frame_rate() { return &video_frame_rate_; }

  // Video packets per second, including empty video-packets.
  // This will be greater than the frame rate, as individual frames are
  // contained in packets, some of which might be empty (e.g. when there are no
  // screen changes).
  RateCounter* video_packet_rate() { return &video_packet_rate_; }

  // The latency metrics below are recorded per video packet received and, for
  // reporting, averaged over the N most recent samples.
  // N is defined by kLatencySampleSize.
  RunningAverage* video_capture_ms() { return &video_capture_ms_; }
  RunningAverage* video_encode_ms() { return &video_encode_ms_; }
  RunningAverage* video_decode_ms() { return &video_decode_ms_; }
  RunningAverage* video_paint_ms() { return &video_paint_ms_; }
  RunningAverage* round_trip_ms() { return &round_trip_ms_; }

 private:
  RateCounter video_bandwidth_;
  RateCounter video_frame_rate_;
  RateCounter video_packet_rate_;
  RunningAverage video_capture_ms_;
  RunningAverage video_encode_ms_;
  RunningAverage video_decode_ms_;
  RunningAverage video_paint_ms_;
  RunningAverage round_trip_ms_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingStats);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_STATS_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_stats.h"

namespace {

// We take the last 10 latency numbers and report the average.
const int kLatencySampleSize = 10;

}  // namespace

namespace remoting {

ChromotingStats::ChromotingStats()
    : video_bandwidth_(
          base::TimeDelta::FromSeconds(kStatsUpdateFrequencyInSeconds)),
      video_frame_rate_(
          base::TimeDelta::FromSeconds(kStatsUpdateFrequencyInSeconds)),
      video_packet_rate_(
          base::TimeDelta::FromSeconds(kStatsUpdateFrequencyInSeconds)),
      video_capture_ms_(kLatencySampleSize),
      video_encode_ms_(kLatencySampleSize),
      video_decode_ms_(kLatencySampleSize),
      video_paint_ms_(kLatencySampleSize),
      round_trip_ms_(kLatencySampleSize) {
}

ChromotingStats::~ChromotingStats() {
}

}  // namespace remoting

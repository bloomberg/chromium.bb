// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingStats defines a bundle of performance counters and statistics
// for chromoting.

#ifndef REMOTING_CLIENT_CHROMOTING_STATS_H_
#define REMOTING_CLIENT_CHROMOTING_STATS_H_

#include "remoting/base/rate_counter.h"

namespace remoting {

class ChromotingStats {
 public:
  ChromotingStats();

  RateCounter* video_bandwidth() { return &video_bandwidth_; }

 private:
  RateCounter video_bandwidth_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingStats);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_STATS_H_

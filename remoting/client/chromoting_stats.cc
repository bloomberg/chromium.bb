// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_stats.h"

namespace {

// The default window of bandwidth in seconds.
static const int kBandwidthWindow = 3;

}  // namespace

namespace remoting {

ChromotingStats::ChromotingStats()
    : video_bandwidth_(base::TimeDelta::FromSeconds(kBandwidthWindow)) {
}

}  // namespace remoting

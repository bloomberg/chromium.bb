// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PROCESS_STATS_UTIL_H_
#define REMOTING_HOST_PROCESS_STATS_UTIL_H_

#include <vector>

#include "remoting/proto/process_stats.pb.h"

namespace remoting {

// Whether the |usage| is empty, i.e. all fields hold initial values.
bool IsEmptyProcessResourceUsage(const protocol::ProcessResourceUsage& usage);

// Merges several ProcessResourceUsage instances into one
// AggregatedProcessResourceUsage.
protocol::AggregatedProcessResourceUsage AggregateProcessResourceUsage(
    const std::vector<protocol::ProcessResourceUsage>& usages);

}  // namespace remoting

#endif  // REMOTING_HOST_PROCESS_STATS_UTIL_H_

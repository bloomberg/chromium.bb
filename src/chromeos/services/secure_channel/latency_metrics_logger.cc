// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/latency_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {

namespace secure_channel {

namespace {

constexpr const base::TimeDelta kMinLatencyDuration =
    base::TimeDelta::FromMilliseconds(1);
constexpr const base::TimeDelta kMaxLatencyDuration =
    base::TimeDelta::FromSeconds(30);

// Provide enough granularity so that durations <10s are assigned to buckets
// in the hundreds of milliseconds.
const int kNumMetricsBuckets = 100;

}  // namespace

void LogLatencyMetric(const std::string& metric_name,
                      const base::TimeDelta& duration) {
  base::UmaHistogramCustomTimes(metric_name, duration, kMinLatencyDuration,
                                kMaxLatencyDuration, kNumMetricsBuckets);
}

}  // namespace secure_channel

}  // namespace chromeos

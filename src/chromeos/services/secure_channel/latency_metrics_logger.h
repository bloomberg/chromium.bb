// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_LATENCY_METRICS_LOGGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_LATENCY_METRICS_LOGGER_H_

#include <string>

#include "base/time/time.h"

namespace chromeos {

namespace secure_channel {

// Logs metrics related to connection latencies. This function should be
// utilized instead of the default UMA_HISTOGRAM_TIMES() macro because it
// provides custom bucket sizes (e.g., UMA_HISTOGRAM_TIMES() only allows
// durations up to 10 seconds, but some connection attempts take longer than
// that).
void LogLatencyMetric(const std::string& metric_name,
                      const base::TimeDelta& duration);

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_LATENCY_METRICS_LOGGER_H_

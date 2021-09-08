// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_UKM_SMOOTHNESS_DATA_H_
#define CC_METRICS_UKM_SMOOTHNESS_DATA_H_

#include "cc/metrics/shared_metrics_buffer.h"

namespace cc {

// The smoothness metrics, containing the score measured using various
// normalization strategies. The normalization strategies are detailed in
// https://docs.google.com/document/d/1ENJXn2bPqvxycnVS9X35qDu1642DQyz42upj5ETOhSs/preview
struct UkmSmoothnessData {
  double avg_smoothness = 0.0;
  double worst_smoothness = 0.0;

  // Values are set to -1 to help with recognizing when these metrics are not
  // calculated.
  double worst_smoothness_after1sec = -1.0;
  double worst_smoothness_after2sec = -1.0;
  double worst_smoothness_after5sec = -1.0;

  double above_threshold = 0.0;
  double percentile_95 = 0.0;
  base::TimeDelta time_max_delta = base::TimeDelta::FromMilliseconds(1);
};

using UkmSmoothnessDataShared = SharedMetricsBuffer<UkmSmoothnessData>;

}  // namespace cc

#endif  // CC_METRICS_UKM_SMOOTHNESS_DATA_H_

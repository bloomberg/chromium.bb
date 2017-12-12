// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_
#define SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_

#include <stdint.h>

#include "services/metrics/public/cpp/metrics_export.h"

namespace ukm {

// Calculates the exponential bucket |sample| falls in and returns the lower
// threshold of that bucket. |bucket_spacing| is the exponential spacing factor
// from one bucket to the next. Only returns a non-negative value.
int64_t METRICS_EXPORT GetExponentialBucketMin(int64_t sample,
                                               double bucket_spacing);

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/metrics_utils.h"

#include <math.h>

#include <cmath>

namespace ukm {

int64_t GetExponentialBucketMin(int64_t sample, double bucket_spacing) {
  if (sample <= 0) {
    return 0;
  }
  // This is similar to the bucketing methodology used in histograms, but
  // instead of iteratively calculating each bucket, this calculates the lower
  // end of the specific bucket for network and cached bytes.
  return std::ceil(std::pow(
      bucket_spacing, std::floor(std::log(sample) / std::log(bucket_spacing))));
}

}  // namespace ukm

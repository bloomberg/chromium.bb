// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_tab_count_metrics/live_tab_count_metrics.h"

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"

namespace live_tab_count_metrics {

// These values represent the lower bound for each bucket, and define the live
// tab count buckets. The final bucket has no upper bound, and each other
// bucket, i, is bounded above by the lower bound of bucket i + 1.
//
// The buckets were determined from the Tabs.MaxTabsInADay histogram,
// approximating the 25th, 50th, 75th, 95th, and 99th percentiles, but with the
// single and zero tab cases separated.
//
// If adding or removing a bucket, update |kNumLiveTabCountBuckets|,
// |kLiveTabCountBucketMins|, and |kLiveTabCountBucketNames|. If adding,
// removing, or changing bucket ranges, the existing metrics that use these
// functions for emitting histograms should be marked as obsolete, and new
// metrics should be created. This can be accomplished by versioning
// |kLiveTabCountBucketNames|, e.g.  ".ByLiveTabCount2.0Tabs", etc., and
// updating the histogram suffixes section of histograms.xml, creating a new
// entry for the new suffixes and marking the old suffixes obsolete.
constexpr size_t kLiveTabCountBucketMins[] = {0, 1, 2, 3, 5, 8, 20, 40};

// Text for the live tab count portion of metric names. These need to be kept
// in sync with |kLiveTabCountBucketMins|.
constexpr const char* kLiveTabCountBucketNames[]{
    ".ByLiveTabCount.0Tabs",      ".ByLiveTabCount.1Tab",
    ".ByLiveTabCount.2Tabs",      ".ByLiveTabCount.3To4Tabs",
    ".ByLiveTabCount.5To7Tabs",   ".ByLiveTabCount.8To19Tabs",
    ".ByLiveTabCount.20To39Tabs", ".ByLiveTabCount.40OrMoreTabs"};

std::string HistogramName(const std::string prefix, size_t bucket) {
  static_assert(
      base::size(kLiveTabCountBucketMins) == kNumLiveTabCountBuckets,
      "kLiveTabCountBucketMins must have kNumLiveTabCountBuckets elements.");
  static_assert(
      base::size(kLiveTabCountBucketNames) == kNumLiveTabCountBuckets,
      "kLiveTabCountBucketNames must have kNumLiveTabCountBuckets elements.");
  DCHECK_LT(bucket, kNumLiveTabCountBuckets);
  DCHECK(prefix.length());
  return prefix + kLiveTabCountBucketNames[bucket];
}

size_t BucketForLiveTabCount(size_t num_live_tabs) {
  for (size_t bucket = 0; bucket < kNumLiveTabCountBuckets; bucket++) {
    if (internal::IsInBucket(num_live_tabs, bucket))
      return bucket;
  }
  // There should be a bucket for any number of tabs >= 0.
  NOTREACHED();
  return kNumLiveTabCountBuckets;
}

namespace internal {

size_t BucketMin(size_t bucket) {
  DCHECK_LT(bucket, kNumLiveTabCountBuckets);
  return kLiveTabCountBucketMins[bucket];
}

size_t BucketMax(size_t bucket) {
  DCHECK_LT(bucket, kNumLiveTabCountBuckets);
  // The last bucket includes everything after the min bucket value.
  if (bucket == kNumLiveTabCountBuckets - 1)
    return std::numeric_limits<size_t>::max();
  return kLiveTabCountBucketMins[bucket + 1] - 1;
}

bool IsInBucket(size_t num_live_tabs, size_t bucket) {
  DCHECK_LT(bucket, kNumLiveTabCountBuckets);
  return num_live_tabs >= BucketMin(bucket) &&
         num_live_tabs <= BucketMax(bucket);
}

}  // namespace internal

}  // namespace live_tab_count_metrics

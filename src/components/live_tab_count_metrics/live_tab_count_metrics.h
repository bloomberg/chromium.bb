// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_TAB_COUNT_METRICS_LIVE_TAB_COUNT_METRICS_H_
#define COMPONENTS_LIVE_TAB_COUNT_METRICS_LIVE_TAB_COUNT_METRICS_H_

#include <string>

#include "base/component_export.h"

// This namespace contains functions for creating histograms bucketed by number
// of live tabs.
//
// All bucket parameters --- number of buckets, bucket sizes, bucket names ---
// are determined at compile time, and these methods are safe to call from any
// thread.
//
// A typical example of creating a histogram bucketed by live tab count using
// STATIC_HISTOGRAM_POINTER_GROUP looks something like this:
//  const size_t live_tab_count = GetLiveTabCount();
//  const size_t bucket =
//      live_tab_count_metrics::BucketForLiveTabCount(live_tab_count);
//  STATIC_HISTOGRAM_POINTER_GROUP(
//      live_tab_count_metrics::HistogramName(constant_histogram_prefix,
//                                            bucket),
//      static_cast<int>(bucket),
//      static_cast<int>(live_tab_count_metrics::kNumLiveTabCountBuckets),
//      Add(sample),
//      base::Histogram::FactoryGet(
//          live_tab_count_metrics::HistogramName(constant_histogram_prefix,
//                                                bucket),
//          MINIMUM_SAMPLE, MAXIMUM_SAMPLE, BUCKET_COUNT,
//          base::HistogramBase::kUmaTargetedHistogramFlag));
//  }
namespace live_tab_count_metrics {

// |kNumLiveTabCountBuckets| is used in various constexpr arrays and as a bound
// on the histogram array when using STATIC_HISTOGRAM_POINTER_GROUP. This value
// must be equal to the length of the array of live tab count bucket min values
// (|kLiveTabCountBucketMins|) and the array of bucket names
// (|kLiveTabCountBucketNames|) found in the corresponding .cc file.
constexpr size_t kNumLiveTabCountBuckets = 8;

// Returns the histogram name for |bucket|. The histogram name is the
// concatenation of |prefix| and the name corresponding to |bucket|, which is of
// the form |prefix| + ".ByLiveTabCount." + <BucketRangeText>, where
// <BucketRangeText> is a string describing the bucket range, e.g. "1Tab",
// "3To4Tabs", etc. See |kLiveTabCountBucketNames| for all of the bucket names.
// |bucket| must be in the interval [0, |kNumLiveTabCountBuckets|).
COMPONENT_EXPORT(LIVE_TAB_COUNT_METRICS)
std::string HistogramName(const std::string prefix, size_t bucket);

// Return the bucket index for the |num_live_tabs|.
COMPONENT_EXPORT(LIVE_TAB_COUNT_METRICS)
size_t BucketForLiveTabCount(size_t num_live_tabs);

// These are exposed for unit tests.
namespace internal {

// Returns the number of tabs corresponding to the minimum value of |bucket|.
// |bucket| must be in the interval [0, |kNumLiveTabCountBuckets|).
COMPONENT_EXPORT(LIVE_TAB_COUNT_METRICS)
size_t BucketMin(size_t bucket);

// Returns the number of tabs corresponding to the maximum value of |bucket|.
// |bucket| must be in the interval [0, |kNumLiveTabCountBuckets|).
COMPONENT_EXPORT(LIVE_TAB_COUNT_METRICS)
size_t BucketMax(size_t bucket);

// Returns true if |num_live_tabs| falls within |bucket|.
// |bucket| must be in the interval [0, |kNumLiveTabCountBuckets|).
COMPONENT_EXPORT(LIVE_TAB_COUNT_METRICS)
bool IsInBucket(size_t num_live_tabs, size_t bucket);

}  // namespace internal

}  // namespace live_tab_count_metrics

#endif  // COMPONENTS_LIVE_TAB_COUNT_METRICS_LIVE_TAB_COUNT_METRICS_H_

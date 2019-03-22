// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_tab_count_metrics/live_tab_count_metrics.h"

#include <algorithm>
#include <array>
#include <string>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace live_tab_count_metrics {

namespace {

// For unit tests, we create bucketed test enumeration histograms with a prefix
// of |kTestMetricPrefix| and a range of [0, |kMaxLiveTabCount|]. We want good
// test coverage in all buckets, but the overflow live tab count bucket is
// unbounded, so we cap the max number of live tabs at a reasonable value.
constexpr char kTestMetricPrefix[] = "TestMetric";
constexpr size_t kMaxLiveTabCount = 400;

// LiveTabCounts is a map of a live tab count to expected histogram counts,
// which is used to validate the test histogram.
//
// When we record a sample for test metrics, the number of live tabs is used as
// the bucket in the enumeration histogram. For example, if we record a sample
// with the number of live tabs equal to 10, we use 10 as the sample in the
// enumeration histogram. Suppose we have LiveTabCounts map, |expected_counts|.
// When recording the metric, we would increment the value of
// expected_counts[10]. |expected_counts| is then used to validate the
// histogram.
using LiveTabCounts = base::flat_map<size_t, size_t>;

// Array of LiveTabCounts, indexed by live tab count bucket.
//
// Each value in this array corresponds to the expected counts for a different
// histogram, i.e. the histogram corresponding to the live tab count bucket
// equal to the array index. For example, given a
// HistogramCountsByLiveTabCountBucket array, |expected_counts_by_bucket|,
// expected_counts_by_bucket[1] are the expected counts for the histogram
// corresponding to live tab count bucket 1.
using HistogramCountsByLiveTabCountBucket =
    std::array<LiveTabCounts, kNumLiveTabCountBuckets>;

}  // namespace

class LiveTabCountMetricsTest : public testing::Test {
 public:
  LiveTabCountMetricsTest() = default;
  ~LiveTabCountMetricsTest() override = default;

  void RecordLiveTabCountMetric(size_t live_tab_count) {
    EXPECT_LE(live_tab_count, kMaxLiveTabCount);
    const size_t bucket = BucketForLiveTabCount(live_tab_count);
    STATIC_HISTOGRAM_POINTER_GROUP(
        HistogramName(kTestMetricPrefix, bucket), static_cast<int>(bucket),
        static_cast<int>(kNumLiveTabCountBuckets), Add(live_tab_count),
        base::Histogram::FactoryGet(
            HistogramName(kTestMetricPrefix, bucket), 1, kMaxLiveTabCount,
            kMaxLiveTabCount + 1,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }

 protected:
  void ValidateHistograms(
      const HistogramCountsByLiveTabCountBucket& expected_counts) {
    for (size_t bucket = 0; bucket < expected_counts.size(); bucket++) {
      size_t expected_total_count = 0;
      std::string histogram_name = HistogramName(kTestMetricPrefix, bucket);
      for (const auto& live_tab_counts : expected_counts[bucket]) {
        histogram_tester_.ExpectBucketCount(
            histogram_name, live_tab_counts.first, live_tab_counts.second);
        expected_total_count += live_tab_counts.second;
      }
      histogram_tester_.ExpectTotalCount(histogram_name, expected_total_count);
    }
  }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(LiveTabCountMetricsTest, HistogramNames) {
  // Testing with hard-coded strings to check that the concatenated names
  // produced by HistogramName() are indeed what we expect. If the bucket ranges
  // change, these strings will need to as well.
  const std::string kTestMetricNameBucket1("TestMetric.ByLiveTabCount.1Tab");
  const std::string kTestMetricNameBucket3(
      "TestMetric.ByLiveTabCount.3To4Tabs");
  EXPECT_EQ(kTestMetricNameBucket1, HistogramName(kTestMetricPrefix, 1));
  EXPECT_EQ(kTestMetricNameBucket3, HistogramName(kTestMetricPrefix, 3));
}

TEST_F(LiveTabCountMetricsTest, RecordMetricsForBucketLimits) {
  // For each bucket, simulate recording metrics with the live tab count equal
  // to the lower and upper bounds of each bucket. For the last bucket, we use
  // an artificial upper bound since that bucket has no practical bound (max
  // size_t value) and do not want to create a histogram with that many buckets.
  HistogramCountsByLiveTabCountBucket expected_counts;
  for (size_t bucket = 0; bucket < expected_counts.size(); bucket++) {
    size_t num_live_tabs = internal::BucketMin(bucket);
    RecordLiveTabCountMetric(num_live_tabs);
    // Expect a count of 1 for the |num_live_tabs| case in the histogram
    // corresponding to |bucket|.
    expected_counts[bucket].emplace(num_live_tabs, 1);

    num_live_tabs = std::min(internal::BucketMax(bucket), kMaxLiveTabCount);
    RecordLiveTabCountMetric(num_live_tabs);
    // Ensure that we aren't setting the artificial maximum for the last bucket
    // too low.
    EXPECT_LE(internal::BucketMin(bucket), num_live_tabs);

    auto iter = expected_counts[bucket].find(num_live_tabs);
    if (iter != expected_counts[bucket].end()) {
      // If the lower bound for |bucket| is the same as the upper bound, we've
      // already recorded a value for |num_live_tabs| in |bucket|.
      EXPECT_EQ(iter->second, 1u);
      ++iter->second;
    } else {
      expected_counts[bucket].emplace(num_live_tabs, 1);
    }

    ValidateHistograms(expected_counts);
  }
}

TEST_F(LiveTabCountMetricsTest, RecordMetricsForAllBucketValues) {
  // For each bucket, simulate recording metrics with the live tab count equal
  // to each value in the bucket range, i.e. from BucketMin() to BucketMax().
  // For the last bucket, we use an artificial upper bound since that bucket has
  // no practical bound (max size_t value) and do not want to create a histogram
  // with that many buckets.
  HistogramCountsByLiveTabCountBucket expected_counts;
  for (size_t bucket = 0; bucket < expected_counts.size(); bucket++) {
    size_t num_live_tabs_lower_bound = internal::BucketMin(bucket);
    size_t num_live_tabs_upper_bound =
        std::min(internal::BucketMax(bucket), kMaxLiveTabCount);
    EXPECT_LE(num_live_tabs_lower_bound, num_live_tabs_upper_bound);
    for (size_t live_tab_count = num_live_tabs_lower_bound;
         live_tab_count <= num_live_tabs_upper_bound; live_tab_count++) {
      RecordLiveTabCountMetric(live_tab_count);
      expected_counts[bucket].emplace(live_tab_count, 1);
    }
    ValidateHistograms(expected_counts);
  }
}

TEST_F(LiveTabCountMetricsTest, BucketsDoNotOverlap) {
  // For any number of tabs >= 0, the tab should belong to exactly one bucket.
  for (size_t tab_count = 0; tab_count <= kMaxLiveTabCount; tab_count++) {
    bool has_bucket_for_tab_count = false;
    for (size_t bucket = 0; bucket < kNumLiveTabCountBuckets; bucket++) {
      if (internal::IsInBucket(tab_count, bucket)) {
        EXPECT_FALSE(has_bucket_for_tab_count);
        has_bucket_for_tab_count = true;
      }
    }
    EXPECT_TRUE(has_bucket_for_tab_count);
  }
}

}  // namespace live_tab_count_metrics

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"

namespace blink {

class LocalFrameUkmAggregatorTest : public testing::Test {
 public:
  LocalFrameUkmAggregatorTest() = default;
  ~LocalFrameUkmAggregatorTest() override = default;

  void SetUp() override {
    clock_.emplace();
    aggregator_.reset(new LocalFrameUkmAggregator(
        ukm::UkmRecorder::GetNewSourceID(), &recorder_));
  }

  void TearDown() override {
    aggregator_.reset();
    clock_.reset();
  }

  LocalFrameUkmAggregator& aggregator() {
    CHECK(aggregator_);
    return *aggregator_;
  }

  ukm::TestUkmRecorder& recorder() { return recorder_; }

  void ResetAggregator() { aggregator_.reset(); }

  WTF::ScopedMockClock& clock() { return *clock_; }

  std::string GetAverageMetricName(int index) {
    return std::string(
               LocalFrameUkmAggregator::metric_strings()[index].Utf8().data()) +
           ".Average";
  }

  std::string GetWorstCaseMetricName(int index) {
    return std::string(
               LocalFrameUkmAggregator::metric_strings()[index].Utf8().data()) +
           ".WorstCase";
  }

  std::string GetAverageRatioMetricName(int index) {
    return std::string(
               LocalFrameUkmAggregator::metric_strings()[index].Utf8().data()) +
           ".AverageRatio";
  }

  std::string GetWorstCaseRatioMetricName(int index) {
    return std::string(
               LocalFrameUkmAggregator::metric_strings()[index].Utf8().data()) +
           ".WorstCaseRatio";
  }

  base::TimeTicks Now() {
    return base::TimeTicks() + base::TimeDelta::FromSecondsD(clock_->Now());
  }

 private:
  base::Optional<WTF::ScopedMockClock> clock_;
  std::unique_ptr<LocalFrameUkmAggregator> aggregator_;
  ukm::TestUkmRecorder recorder_;
};

TEST_F(LocalFrameUkmAggregatorTest, EmptyEventsNotRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  clock().Advance(TimeDelta::FromSeconds(10));
  ResetAggregator();

  EXPECT_EQ(recorder().sources_count(), 0u);
  EXPECT_EQ(recorder().entries_count(), 0u);
}

TEST_F(LocalFrameUkmAggregatorTest, EventsRecordedPerSecond) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The records should be recorded once every 30 seconds.
  // Set this up so that we get 3 events, each with 10 recordings per-metric,
  // then a final event with 3 recordings per metric. That's 3 * kCount * 10
  // steps + kCount * 3 more steps. A recording is made every 30s, so we need
  // each step to be 30s / (kCount * 10), rounded up to be sure we are past 30s
  // when the last metric is recorded, converted to ms.
  base::TimeTicks start_time = base::TimeTicks();
  int num_runs = 33 * LocalFrameUkmAggregator::kCount;
  unsigned millisecond_per_step =
      (unsigned)ceil(3000.0 / (double)LocalFrameUkmAggregator::kCount);
  for (int i = 0; i < num_runs; ++i) {
    aggregator().BeginMainFrame();
    {
      auto timer =
          aggregator().GetScopedTimer(i % LocalFrameUkmAggregator::kCount);
      clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
    }
    aggregator().RecordEndOfFrameMetrics(start_time, Now());
    start_time = Now();
  }

  EXPECT_EQ(recorder().entries_count(), 3u);

  // Once we reset, we record any remaining samples into one more entry, for a
  // total of 4.
  ResetAggregator();

  EXPECT_EQ(recorder().entries_count(), 4u);
  auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
  EXPECT_EQ(entries.size(), 4u);

  for (auto* entry : entries) {
    for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetAverageMetricName(i)));
      const int64_t* metric1_average =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetAverageMetricName(i));
      EXPECT_NEAR(*metric1_average / 1e3, millisecond_per_step, 0.001);

      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetWorstCaseMetricName(i)));
      const int64_t* metric1_worst = ukm::TestUkmRecorder::GetEntryMetric(
          entry, GetWorstCaseMetricName(i));
      EXPECT_NEAR(*metric1_worst / 1e3, millisecond_per_step, 0.001);

      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetAverageRatioMetricName(i)));
      const int64_t* metric1_average_ratio =
          ukm::TestUkmRecorder::GetEntryMetric(entry,
                                               GetAverageRatioMetricName(i));
      EXPECT_NEAR(*metric1_average_ratio, 14.0, 0.001);

      EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
          entry, GetWorstCaseRatioMetricName(i)));
      const int64_t* metric1_worst_ratio = ukm::TestUkmRecorder::GetEntryMetric(
          entry, GetWorstCaseRatioMetricName(i));
      EXPECT_NEAR(*metric1_worst_ratio, 100.0, 0.001);
    }
  }
}

TEST_F(LocalFrameUkmAggregatorTest, EventsAveragedCorrectly) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  aggregator().BeginMainFrame();

  // 1, 2, and 3 seconds.
  for (int i = 1; i <= 3; ++i) {
    {
      auto timer = aggregator().GetScopedTimer(0);
      clock().Advance(TimeDelta::FromSeconds(i));
    }
  }

  // 3, 3, 3, and then 1 outside of the loop.
  for (int i = 0; i < 3; ++i) {
    auto timer = aggregator().GetScopedTimer(1);
    clock().Advance(TimeDelta::FromSeconds(3));
  }
  {
    auto timer = aggregator().GetScopedTimer(1);
    clock().Advance(TimeDelta::FromSeconds(1));
  }

  aggregator().RecordEndOfFrameMetrics(
      base::TimeTicks(), base::TimeTicks() + base::TimeDelta::FromSecondsD(20));

  ResetAggregator();
  auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
  EXPECT_EQ(entries.size(), 1u);
  auto* entry = entries[0];

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, GetAverageMetricName(0)));
  const int64_t* metric1_average =
      ukm::TestUkmRecorder::GetEntryMetric(entry, GetAverageMetricName(0));
  // metric1 (1, 2, 3) average is 2
  EXPECT_NEAR(*metric1_average / 1e6, 2.0, 0.0001);

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, GetWorstCaseMetricName(0)));
  const int64_t* metric1_worst =
      ukm::TestUkmRecorder::GetEntryMetric(entry, GetWorstCaseMetricName(0));
  // metric1 (1, 2, 3) worst case is 3
  EXPECT_NEAR(*metric1_worst / 1e6, 3.0, 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entry, GetAverageRatioMetricName(0)));
  const int64_t* metric1_average_ratio =
      ukm::TestUkmRecorder::GetEntryMetric(entry, GetAverageRatioMetricName(0));
  // metric1 (1, 2, 3) sums to 6 seconds of time with a frame time of 20
  EXPECT_NEAR(*metric1_average_ratio, floor(6.0 * 100.0 / 20.0), 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entry, GetWorstCaseRatioMetricName(0)));
  const int64_t* metric1_worst_ratio = ukm::TestUkmRecorder::GetEntryMetric(
      entry, GetWorstCaseRatioMetricName(0));
  // Only one main frame sample, so worst is same as average
  EXPECT_NEAR(*metric1_worst_ratio, floor(6.0 * 100.0 / 20.0), 0.0001);

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, GetAverageMetricName(1)));
  const int64_t* metric2_average =
      ukm::TestUkmRecorder::GetEntryMetric(entry, GetAverageMetricName(1));
  // metric1 (3, 3, 3, 1) average is 2.5
  EXPECT_NEAR(*metric2_average / 1e6, 2.5, 0.0001);

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, GetWorstCaseMetricName(1)));
  const int64_t* metric2_worst =
      ukm::TestUkmRecorder::GetEntryMetric(entry, GetWorstCaseMetricName(1));
  // metric1 (3, 3, 3, 1) worst case is 3
  EXPECT_NEAR(*metric2_worst / 1e6, 3.0, 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entry, GetAverageRatioMetricName(1)));
  const int64_t* metric2_average_ratio =
      ukm::TestUkmRecorder::GetEntryMetric(entry, GetAverageRatioMetricName(1));
  // metric2 (3, 3, 3, 1) sums to 10 seconds of time with a frame time of 20
  EXPECT_NEAR(*metric2_average_ratio, floor(10.0 * 100.0 / 20.0), 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entry, GetWorstCaseRatioMetricName(1)));
  const int64_t* metric2_worst_ratio = ukm::TestUkmRecorder::GetEntryMetric(
      entry, GetWorstCaseRatioMetricName(1));

  // Only one main frame sample, so worst is same as average
  EXPECT_NEAR(*metric2_worst_ratio, floor(10.0 * 100.0 / 20.0), 0.0001);
}

}  // namespace blink

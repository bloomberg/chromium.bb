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
    aggregator_ = base::MakeRefCounted<LocalFrameUkmAggregator>(
        ukm::UkmRecorder::GetNewSourceID(), &recorder_);
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

  std::string GetPrimaryMetricName() {
    return std::string(
        LocalFrameUkmAggregator::primary_metric_name().Utf8().data());
  }

  std::string GetMetricName(int index) {
    return std::string(
        LocalFrameUkmAggregator::metrics_data()[index].name.Utf8().data());
  }

  std::string GetPercentageMetricName(int index) {
    return std::string(LocalFrameUkmAggregator::metrics_data()[index]
                           .name.Utf8()
                           .data()) +
           "Percentage";
  }

  void FramesToNextEventForTest(unsigned delta) {
    aggregator().FramesToNextEventForTest(delta);
  }

  base::TimeTicks Now() {
    return base::TimeTicks() + base::TimeDelta::FromSecondsD(clock_->Now());
  }

  void VerifyEntries(unsigned expected_num_entries,
                     unsigned expected_primary_metric,
                     unsigned expected_sub_metric,
                     unsigned expected_percentage) {
    EXPECT_EQ(recorder().entries_count(), expected_num_entries);
    auto entries = recorder().GetEntriesByName("Blink.UpdateTime");
    EXPECT_EQ(entries.size(), expected_num_entries);

    for (auto* entry : entries) {
      EXPECT_TRUE(
          ukm::TestUkmRecorder::EntryHasMetric(entry, GetPrimaryMetricName()));
      const int64_t* primary_metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, GetPrimaryMetricName());
      EXPECT_NEAR(*primary_metric_value / 1e3, expected_primary_metric, 0.001);
      for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
        EXPECT_TRUE(
            ukm::TestUkmRecorder::EntryHasMetric(entry, GetMetricName(i)));
        const int64_t* metric_value =
            ukm::TestUkmRecorder::GetEntryMetric(entry, GetMetricName(i));
        EXPECT_NEAR(*metric_value / 1e3, expected_sub_metric, 0.001);

        EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
            entry, GetPercentageMetricName(i)));
        const int64_t* metric_percentage = ukm::TestUkmRecorder::GetEntryMetric(
            entry, GetPercentageMetricName(i));
        EXPECT_NEAR(*metric_percentage, expected_percentage, 0.001);
      }
    }
  }

 private:
  base::Optional<WTF::ScopedMockClock> clock_;
  scoped_refptr<LocalFrameUkmAggregator> aggregator_;
  ukm::TestUkmRecorder recorder_;
};

TEST_F(LocalFrameUkmAggregatorTest, EmptyEventsNotRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // There is no BeginMainFrame, so no metrics get recorded.
  clock().Advance(TimeDelta::FromSeconds(10));
  ResetAggregator();

  EXPECT_EQ(recorder().sources_count(), 0u);
  EXPECT_EQ(recorder().entries_count(), 0u);
}

TEST_F(LocalFrameUkmAggregatorTest, FirstFrameIsRecorded) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The initial interval is always zero, so we should see one set of metrics
  // for the initial frame, regardless of the initial interval.
  base::TimeTicks start_time = Now();
  FramesToNextEventForTest(1);
  unsigned millisecond_for_step = 1;
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer =
        aggregator().GetScopedTimer(i % LocalFrameUkmAggregator::kCount);
    clock().Advance(TimeDelta::FromMilliseconds(millisecond_for_step));
  }
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  EXPECT_EQ(recorder().entries_count(), 1u);

  float expected_primary_metric =
      millisecond_for_step * LocalFrameUkmAggregator::kCount;
  float expected_sub_metric = millisecond_for_step;
  float expected_percentage =
      floor(100.0 / (float)LocalFrameUkmAggregator::kCount);

  VerifyEntries(1u, expected_primary_metric, expected_sub_metric,
                expected_percentage);

  // Reset the aggregator. Should not record any more.
  ResetAggregator();

  VerifyEntries(1u, expected_primary_metric, expected_sub_metric,
                expected_percentage);
}

TEST_F(LocalFrameUkmAggregatorTest, EventsRecordedAtIntervals) {
  // Although the tests use a mock clock, the UKM aggregator checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  // The records should be recorded in the first frame after every interval,
  // and no sooner.

  // Set the first sample interval to 2.
  FramesToNextEventForTest(2);
  unsigned millisecond_per_step = 50 / (LocalFrameUkmAggregator::kCount + 1);
  unsigned millisecond_per_frame =
      millisecond_per_step * (LocalFrameUkmAggregator::kCount + 1);

  base::TimeTicks start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // We should have a sample after the very first step, regardless of the
  // interval. The FirstFrameIsRecorded test above also tests this.
  float expected_percentage =
      floor(millisecond_per_step * 100.0 / (float)millisecond_per_frame);
  VerifyEntries(1u, millisecond_per_frame, millisecond_per_step,
                expected_percentage);

  // Another step does not get us past the sample interval.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  VerifyEntries(1u, millisecond_per_frame, millisecond_per_step,
                expected_percentage);

  // Another step should tick us past the sample interval.
  // Note that the sample is a single frame, so even if we've taken
  // multiple steps we should see just one frame's time.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  VerifyEntries(2u, millisecond_per_frame, millisecond_per_step,
                expected_percentage);

  // Step one more frame so we don't sample again.
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // Should be no more samples.
  VerifyEntries(2u, millisecond_per_frame, millisecond_per_step,
                expected_percentage);

  // And one more step to generate one more sample
  start_time = Now();
  aggregator().BeginMainFrame();
  for (int i = 0; i < LocalFrameUkmAggregator::kCount; ++i) {
    auto timer = aggregator().GetScopedTimer(i);
    clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  }
  clock().Advance(TimeDelta::FromMilliseconds(millisecond_per_step));
  aggregator().RecordEndOfFrameMetrics(start_time, Now());

  // We should have 3 more events, once for the prior interval and 2 for the
  // new interval.
  VerifyEntries(3u, millisecond_per_frame, millisecond_per_step,
                expected_percentage);
}

}  // namespace blink

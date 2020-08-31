// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class WriteBarrierPerfTest : public TestSupportingGC {};

namespace {

constexpr char kMetricPrefixWriteBarrier[] = "WriteBarrier.";
constexpr char kMetricWritesDuringGcRunsPerS[] = "writes_during_gc";
constexpr char kMetricWritesOutsideGcRunsPerS[] = "writes_outside_gc";
constexpr char kMetricRelativeSpeedDifferenceUnitless[] =
    "relative_speed_difference";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixWriteBarrier, story_name);
  reporter.RegisterImportantMetric(kMetricWritesDuringGcRunsPerS, "runs/s");
  reporter.RegisterImportantMetric(kMetricWritesOutsideGcRunsPerS, "runs/s");
  reporter.RegisterImportantMetric(kMetricRelativeSpeedDifferenceUnitless,
                                   "unitless");
  return reporter;
}

class PerfDummyObject : public GarbageCollected<PerfDummyObject> {
 public:
  PerfDummyObject() = default;
  virtual void Trace(Visitor*) {}
};

base::TimeDelta TimedRun(base::RepeatingCallback<void()> callback) {
  const base::TimeTicks start = base::TimeTicks::Now();
  callback.Run();
  return base::TimeTicks::Now() - start;
}

}  // namespace

TEST_F(WriteBarrierPerfTest, MemberWritePerformance) {
  // Setup.
  constexpr wtf_size_t kNumElements = 100000;
  Persistent<HeapVector<Member<PerfDummyObject>>> holder(
      MakeGarbageCollected<HeapVector<Member<PerfDummyObject>>>());
  for (wtf_size_t i = 0; i < kNumElements; ++i) {
    holder->push_back(MakeGarbageCollected<PerfDummyObject>());
  }
  PreciselyCollectGarbage();
  // Benchmark.
  base::RepeatingCallback<void()> benchmark = base::BindRepeating(
      [](const Persistent<HeapVector<Member<PerfDummyObject>>>& holder) {
        for (wtf_size_t i = 0; i < kNumElements / 2; ++i) {
          (*holder)[i].Swap((*holder)[kNumElements / 2 + i]);
        }
      },
      holder);

  // During GC.
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  base::TimeDelta during_gc_duration = TimedRun(benchmark);
  driver.FinishSteps();
  PreciselyCollectGarbage();

  // Outside GC.
  base::TimeDelta outside_gc_duration = TimedRun(benchmark);

  // Cleanup.
  holder.Clear();
  PreciselyCollectGarbage();

  // Reporting.
  auto reporter = SetUpReporter("member_write_performance");
  reporter.AddResult(
      kMetricWritesDuringGcRunsPerS,
      static_cast<double>(kNumElements) / during_gc_duration.InSecondsF());
  reporter.AddResult(
      kMetricWritesOutsideGcRunsPerS,
      static_cast<double>(kNumElements) / outside_gc_duration.InSecondsF());
  reporter.AddResult(
      kMetricRelativeSpeedDifferenceUnitless,
      during_gc_duration.InSecondsF() / outside_gc_duration.InSecondsF());
}

}  // namespace blink

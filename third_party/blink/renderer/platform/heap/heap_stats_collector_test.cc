// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// =============================================================================
// ThreadHeapStatsCollector. ===================================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, InitialEmpty) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  for (int i = 0; i < ThreadHeapStatsCollector::Scope::Id::kNumIds; i++) {
    EXPECT_DOUBLE_EQ(0.0, stats_collector.current().scope_data[i]);
  }
  stats_collector.Stop();
}

TEST(ThreadHeapStatsCollectorTest, IncreaseScopeTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep, 1.0);
  EXPECT_DOUBLE_EQ(
      1.0, stats_collector.current().scope_data
               [ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StopMovesCurrentToPrevious) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep, 1.0);
  stats_collector.Stop();
  EXPECT_DOUBLE_EQ(
      1.0, stats_collector.previous().scope_data
               [ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StopResetsCurrent) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep, 1.0);
  stats_collector.Stop();
  EXPECT_DOUBLE_EQ(
      0.0, stats_collector.current().scope_data
               [ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StartStop) {
  ThreadHeapStatsCollector stats_collector;
  EXPECT_FALSE(stats_collector.is_started());
  stats_collector.Start(BlinkGC::kTesting);
  EXPECT_TRUE(stats_collector.is_started());
  stats_collector.Stop();
  EXPECT_FALSE(stats_collector.is_started());
}

// =============================================================================
// ThreadHeapStatsCollector::Scope. ============================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, ScopeToString) {
  EXPECT_STREQ(
      "BlinkGC.IncrementalMarkingStartMarking",
      ThreadHeapStatsCollector::Scope::ToString(
          ThreadHeapStatsCollector::Scope::kIncrementalMarkingStartMarking));
}

// =============================================================================
// ThreadHeapStatsCollector::Event. ============================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, EventMarkedObjectSize) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseMarkedObjectSize(1024);
  stats_collector.Stop();
  EXPECT_EQ(1024u, stats_collector.previous().marked_object_size);
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimeInMsFromIncrementalGC) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingStartMarking, 7.0);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingStep, 2.0);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingFinalizeMarking, 1.0);
  // Ignore the full finalization.
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kIncrementalMarkingFinalize, 3.0);
  stats_collector.Stop();
  EXPECT_DOUBLE_EQ(10.0, stats_collector.previous().marking_time_in_ms());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimeInMsFromFullGC) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kFullGCMarking, 11.0);
  stats_collector.Stop();
  EXPECT_DOUBLE_EQ(11.0, stats_collector.previous().marking_time_in_ms());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimePerByteInS) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseMarkedObjectSize(1000);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kFullGCMarking, 1000.0);
  stats_collector.Stop();
  EXPECT_DOUBLE_EQ(.001,
                   stats_collector.previous().marking_time_per_byte_in_s());
}

TEST(ThreadHeapStatsCollectorTest, SweepingTimeInMs) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.Start(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kLazySweepInIdle, 1.0);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kLazySweepInIdle, 2.0);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kLazySweepInIdle, 3.0);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kLazySweepOnAllocation, 4.0);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::Scope::kCompleteSweep, 5.0);
  stats_collector.Stop();
  EXPECT_DOUBLE_EQ(15.0, stats_collector.previous().sweeping_time_in_ms());
}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_

#include <stddef.h>

#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Manages counters and statistics across garbage collection cycles.
//
// Usage:
//   ThreadHeapStatsCollector stats_collector;
//   stats_collector.Start(<BlinkGC::GCReason>);
//   // Use tracer.
//   // Current event is available using stats_collector.current().
//   stats_collector.Stop();
//   // Previous event is available using stats_collector.previous().
class PLATFORM_EXPORT ThreadHeapStatsCollector {
 public:
  // Trace a particular scope. Will emit a trace event and record the time in
  // the corresponding ThreadHeapStatsCollector.
  class PLATFORM_EXPORT Scope {
   public:
    // These ids will form human readable names when used in Scopes.
    enum Id {
      kCompleteSweep,
      kEagerSweep,
      kIncrementalMarkingStartMarking,
      kIncrementalMarkingStep,
      kIncrementalMarkingFinalize,
      kIncrementalMarkingFinalizeMarking,
      kLazySweepInIdle,
      kLazySweepOnAllocation,
      kFullGCMarking,
      kNumIds,
    };

    static const char* ToString(Id);

    Scope(ThreadHeapStatsCollector*, Id);
    ~Scope();

   private:
    ThreadHeapStatsCollector* const tracer_;
    const double start_time_;
    const Id id_;
  };

  struct PLATFORM_EXPORT Event {
    void reset();

    double marking_time_in_ms() const;
    double marking_time_per_byte_in_s() const;
    double sweeping_time_in_ms() const;

    size_t marked_object_size = 0;
    double scope_data[Scope::kNumIds] = {0};
    BlinkGC::GCReason reason;
  };

  void Start(BlinkGC::GCReason);
  void Stop();

  void IncreaseScopeTime(Scope::Id, double);
  void IncreaseMarkedObjectSize(size_t);

  bool is_started() const { return is_started_; }
  const Event& current() const { return current_; }
  const Event& previous() const { return previous_; }

 private:
  Event current_;
  Event previous_;
  bool is_started_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_

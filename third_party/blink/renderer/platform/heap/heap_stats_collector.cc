// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

ThreadHeapStatsCollector::Scope::Scope(ThreadHeapStatsCollector* tracer,
                                       ThreadHeapStatsCollector::Scope::Id id)
    : tracer_(tracer),
      start_time_(WTF::CurrentTimeTicksInMilliseconds()),
      id_(id) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                     ThreadHeapStatsCollector::Scope::ToString(id_));
}

ThreadHeapStatsCollector::Scope::~Scope() {
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                   ThreadHeapStatsCollector::Scope::ToString(id_));
  tracer_->IncreaseScopeTime(
      id_, WTF::CurrentTimeTicksInMilliseconds() - start_time_);
}

const char* ThreadHeapStatsCollector::Scope::ToString(
    ThreadHeapStatsCollector::Scope::Id id) {
  switch (id) {
    case Scope::kCompleteSweep:
      return "BlinkGC.CompleteSweep";
    case Scope::kEagerSweep:
      return "BlinkGC.EagerSweep";
    case Scope::kIncrementalMarkingStartMarking:
      return "BlinkGC.IncrementalMarkingStartMarking";
    case Scope::kIncrementalMarkingStep:
      return "BlinkGC.IncrementalMarkingStep";
    case Scope::kIncrementalMarkingFinalize:
      return "BlinkGC.IncrementalMarkingFinalize";
    case Scope::kIncrementalMarkingFinalizeMarking:
      return "BlinkGC.IncrementalMarkingFinalizeMarking";
    case Scope::kLazySweepInIdle:
      return "BlinkGC.LazySweepInIdle";
    case Scope::kLazySweepOnAllocation:
      return "BlinkGC.LazySweepOnAllocation";
    case Scope::kFullGCMarking:
      return "BlinkGC.FullGCMarking";
    case Scope::kNumIds:
      break;
  }
  CHECK(false);
  return nullptr;
}

void ThreadHeapStatsCollector::IncreaseScopeTime(
    ThreadHeapStatsCollector::Scope::Id id,
    double time) {
  DCHECK(is_started_);
  current_.scope_data[id] += time;
}

void ThreadHeapStatsCollector::IncreaseMarkedObjectSize(size_t size) {
  DCHECK(is_started_);
  current_.marked_object_size += size;
}

void ThreadHeapStatsCollector::Start(BlinkGC::GCReason reason) {
  DCHECK(!is_started_);
  is_started_ = true;
  current_.reason = reason;
}

void ThreadHeapStatsCollector::Stop() {
  is_started_ = false;
  previous_ = std::move(current_);
  current_.reset();
}

void ThreadHeapStatsCollector::Event::reset() {
  marked_object_size = 0;
  memset(scope_data, 0, sizeof(scope_data));
  reason = BlinkGC::kTesting;
}

double ThreadHeapStatsCollector::Event::marking_time_in_ms() const {
  return scope_data[Scope::kIncrementalMarkingStartMarking] +
         scope_data[Scope::kIncrementalMarkingStep] +
         scope_data[Scope::kIncrementalMarkingFinalizeMarking] +
         scope_data[Scope::kFullGCMarking];
}

double ThreadHeapStatsCollector::Event::marking_time_per_byte_in_s() const {
  return marked_object_size ? marking_time_in_ms() / 1000 / marked_object_size
                            : 0.0;
}

double ThreadHeapStatsCollector::Event::sweeping_time_in_ms() const {
  return scope_data[Scope::kCompleteSweep] + scope_data[Scope::kEagerSweep] +
         scope_data[Scope::kLazySweepInIdle] +
         scope_data[Scope::kLazySweepOnAllocation];
}

}  // namespace blink

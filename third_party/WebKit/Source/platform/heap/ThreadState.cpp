/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/heap/ThreadState.h"

#include <v8.h>

#include <memory>

#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "platform/Histogram.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapCompact.h"
#include "platform/heap/PagePool.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/Visitor.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "platform/instrumentation/tracing/web_process_memory_dump.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/DataLog.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StackUtil.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/allocator/Partitions.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"

#if defined(OS_WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if defined(OS_FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::thread_specific_ = nullptr;
uint8_t ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

const size_t kDefaultAllocatedObjectSizeThreshold = 100 * 1024;

const char* ThreadState::GcReasonString(BlinkGC::GCReason reason) {
  switch (reason) {
    case BlinkGC::kIdleGC:
      return "IdleGC";
    case BlinkGC::kPreciseGC:
      return "PreciseGC";
    case BlinkGC::kConservativeGC:
      return "ConservativeGC";
    case BlinkGC::kForcedGC:
      return "ForcedGC";
    case BlinkGC::kMemoryPressureGC:
      return "MemoryPressureGC";
    case BlinkGC::kPageNavigationGC:
      return "PageNavigationGC";
    case BlinkGC::kThreadTerminationGC:
      return "ThreadTerminationGC";
  }
  return "<Unknown>";
}

ThreadState::ThreadState()
    : thread_(CurrentThread()),
      persistent_region_(std::make_unique<PersistentRegion>()),
      start_of_stack_(reinterpret_cast<intptr_t*>(WTF::GetStackStart())),
      end_of_stack_(reinterpret_cast<intptr_t*>(WTF::GetStackStart())),
      safe_point_scope_marker_(nullptr),
      sweep_forbidden_(false),
      no_allocation_count_(0),
      gc_forbidden_count_(0),
      mixins_being_constructed_count_(0),
      accumulated_sweeping_time_(0),
      object_resurrection_forbidden_(false),
      gc_mixin_marker_(nullptr),
      gc_state_(kNoGCScheduled),
      isolate_(nullptr),
      trace_dom_wrappers_(nullptr),
      invalidate_dead_objects_in_wrappers_marking_deque_(nullptr),
      perform_cleanup_(nullptr),
      wrapper_tracing_in_progress_(false),
#if defined(ADDRESS_SANITIZER)
      asan_fake_stack_(__asan_get_current_fake_stack()),
#endif
#if defined(LEAK_SANITIZER)
      disabled_static_persistent_registration_(0),
#endif
      reported_memory_to_v8_(0) {
  DCHECK(CheckThread());
  DCHECK(!**thread_specific_);
  **thread_specific_ = this;

  heap_ = WTF::WrapUnique(new ThreadHeap(this));
}

ThreadState::~ThreadState() {
  DCHECK(CheckThread());
  if (IsMainThread())
    DCHECK_EQ(Heap().HeapStats().AllocatedSpace(), 0u);
  CHECK(GcState() == ThreadState::kNoGCScheduled);

  **thread_specific_ = nullptr;
}

void ThreadState::AttachMainThread() {
  thread_specific_ = new WTF::ThreadSpecific<ThreadState*>();
  new (main_thread_state_storage_) ThreadState();
}

void ThreadState::AttachCurrentThread() {
  new ThreadState();
}

void ThreadState::DetachCurrentThread() {
  ThreadState* state = Current();
  DCHECK(!state->IsMainThread());
  state->RunTerminationGC();
  delete state;
}

void ThreadState::RunTerminationGC() {
  DCHECK(!IsMainThread());
  DCHECK(CheckThread());

  // Finish sweeping.
  CompleteSweep();

  ReleaseStaticPersistentNodes();

  ProcessHeap::GetCrossThreadPersistentRegion()
      .PrepareForThreadStateTermination(this);

  // Do thread local GC's as long as the count of thread local Persistents
  // changes and is above zero.
  int old_count = -1;
  int current_count = GetPersistentRegion()->NumberOfPersistents();
  DCHECK_GE(current_count, 0);
  while (current_count != old_count) {
    CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kGCWithSweep,
                   BlinkGC::kThreadTerminationGC);
    // Release the thread-local static persistents that were
    // instantiated while running the termination GC.
    ReleaseStaticPersistentNodes();
    old_count = current_count;
    current_count = GetPersistentRegion()->NumberOfPersistents();
  }
  // We should not have any persistents left when getting to this point,
  // if we have it is probably a bug so adding a debug ASSERT to catch this.
  DCHECK(!current_count);
  // All of pre-finalizers should be consumed.
  DCHECK(ordered_pre_finalizers_.IsEmpty());
  CHECK_EQ(GcState(), kNoGCScheduled);

  Heap().RemoveAllPages();
}

NO_SANITIZE_ADDRESS
void ThreadState::VisitAsanFakeStackForPointer(Visitor* visitor, Address ptr) {
#if defined(ADDRESS_SANITIZER)
  Address* start = reinterpret_cast<Address*>(start_of_stack_);
  Address* end = reinterpret_cast<Address*>(end_of_stack_);
  Address* fake_frame_start = nullptr;
  Address* fake_frame_end = nullptr;
  Address* maybe_fake_frame = reinterpret_cast<Address*>(ptr);
  Address* real_frame_for_fake_frame = reinterpret_cast<Address*>(
      __asan_addr_is_in_fake_stack(asan_fake_stack_, maybe_fake_frame,
                                   reinterpret_cast<void**>(&fake_frame_start),
                                   reinterpret_cast<void**>(&fake_frame_end)));
  if (real_frame_for_fake_frame) {
    // This is a fake frame from the asan fake stack.
    if (real_frame_for_fake_frame > end && start > real_frame_for_fake_frame) {
      // The real stack address for the asan fake frame is
      // within the stack range that we need to scan so we need
      // to visit the values in the fake frame.
      for (Address* p = fake_frame_start; p < fake_frame_end; ++p)
        heap_->CheckAndMarkPointer(visitor, *p);
    }
  }
#endif
}

// Stack scanning may overrun the bounds of local objects and/or race with
// other threads that use this stack.
NO_SANITIZE_ADDRESS
NO_SANITIZE_THREAD
void ThreadState::VisitStack(Visitor* visitor) {
  if (stack_state_ == BlinkGC::kNoHeapPointersOnStack)
    return;

  Address* start = reinterpret_cast<Address*>(start_of_stack_);
  // If there is a safepoint scope marker we should stop the stack
  // scanning there to not touch active parts of the stack. Anything
  // interesting beyond that point is in the safepoint stack copy.
  // If there is no scope marker the thread is blocked and we should
  // scan all the way to the recorded end stack pointer.
  Address* end = reinterpret_cast<Address*>(end_of_stack_);
  Address* safe_point_scope_marker =
      reinterpret_cast<Address*>(safe_point_scope_marker_);
  Address* current = safe_point_scope_marker ? safe_point_scope_marker : end;

  // Ensure that current is aligned by address size otherwise the loop below
  // will read past start address.
  current = reinterpret_cast<Address*>(reinterpret_cast<intptr_t>(current) &
                                       ~(sizeof(Address) - 1));

  for (; current < start; ++current) {
    Address ptr = *current;
#if defined(MEMORY_SANITIZER)
    // |ptr| may be uninitialized by design. Mark it as initialized to keep
    // MSan from complaining.
    // Note: it may be tempting to get rid of |ptr| and simply use |current|
    // here, but that would be incorrect. We intentionally use a local
    // variable because we don't want to unpoison the original stack.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    heap_->CheckAndMarkPointer(visitor, ptr);
    VisitAsanFakeStackForPointer(visitor, ptr);
  }

  for (Address ptr : safe_point_stack_copy_) {
#if defined(MEMORY_SANITIZER)
    // See the comment above.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    heap_->CheckAndMarkPointer(visitor, ptr);
    VisitAsanFakeStackForPointer(visitor, ptr);
  }
}

void ThreadState::VisitPersistents(Visitor* visitor) {
  persistent_region_->TracePersistentNodes(visitor);
  if (trace_dom_wrappers_) {
    TRACE_EVENT0("blink_gc", "V8GCController::traceDOMWrappers");
    trace_dom_wrappers_(isolate_, visitor);
  }
}

ThreadState::GCSnapshotInfo::GCSnapshotInfo(size_t num_object_types)
    : live_count(Vector<int>(num_object_types)),
      dead_count(Vector<int>(num_object_types)),
      live_size(Vector<size_t>(num_object_types)),
      dead_size(Vector<size_t>(num_object_types)) {}

size_t ThreadState::TotalMemorySize() {
  return heap_->HeapStats().AllocatedObjectSize() +
         heap_->HeapStats().MarkedObjectSize() +
         WTF::Partitions::TotalSizeOfCommittedPages();
}

size_t ThreadState::EstimatedLiveSize(size_t estimation_base_size,
                                      size_t size_at_last_gc) {
  if (heap_->HeapStats().WrapperCountAtLastGC() == 0)
    return estimation_base_size;

  // (estimated size) = (estimation base size) - (heap size at the last GC) /
  //   (# of persistent handles at the last GC) *
  //     (# of persistent handles collected since the last GC);
  size_t size_retained_by_collected_persistents = static_cast<size_t>(
      1.0 * size_at_last_gc / heap_->HeapStats().WrapperCountAtLastGC() *
      heap_->HeapStats().CollectedWrapperCount());
  if (estimation_base_size < size_retained_by_collected_persistents)
    return 0;
  return estimation_base_size - size_retained_by_collected_persistents;
}

double ThreadState::HeapGrowingRate() {
  size_t current_size = heap_->HeapStats().AllocatedObjectSize() +
                        heap_->HeapStats().MarkedObjectSize();
  size_t estimated_size = EstimatedLiveSize(
      heap_->HeapStats().MarkedObjectSizeAtLastCompleteSweep(),
      heap_->HeapStats().MarkedObjectSizeAtLastCompleteSweep());

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growing_rate =
      estimated_size > 0 ? 1.0 * current_size / estimated_size : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapEstimatedSizeKB",
                 std::min(estimated_size / 1024, static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapGrowingRate",
                 static_cast<int>(100 * growing_rate));
  return growing_rate;
}

double ThreadState::PartitionAllocGrowingRate() {
  size_t current_size = WTF::Partitions::TotalSizeOfCommittedPages();
  size_t estimated_size = EstimatedLiveSize(
      current_size, heap_->HeapStats().PartitionAllocSizeAtLastGC());

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growing_rate =
      estimated_size > 0 ? 1.0 * current_size / estimated_size : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocEstimatedSizeKB",
                 std::min(estimated_size / 1024, static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocGrowingRate",
                 static_cast<int>(100 * growing_rate));
  return growing_rate;
}

// TODO(haraken): We should improve the GC heuristics. The heuristics affect
// performance significantly.
bool ThreadState::JudgeGCThreshold(size_t allocated_object_size_threshold,
                                   size_t total_memory_size_threshold,
                                   double heap_growing_rate_threshold) {
  // If the allocated object size or the total memory size is small, don't
  // trigger a GC.
  if (heap_->HeapStats().AllocatedObjectSize() <
          allocated_object_size_threshold ||
      TotalMemorySize() < total_memory_size_threshold)
    return false;
// If the growing rate of Oilpan's heap or PartitionAlloc is high enough,
// trigger a GC.
#if PRINT_HEAP_STATS
  DataLogF("heapGrowingRate=%.1lf, partitionAllocGrowingRate=%.1lf\n",
           HeapGrowingRate(), PartitionAllocGrowingRate());
#endif
  return HeapGrowingRate() >= heap_growing_rate_threshold ||
         PartitionAllocGrowingRate() >= heap_growing_rate_threshold;
}

bool ThreadState::ShouldScheduleIncrementalMarking() const {
  // TODO(mluppautz): For now immediately schedule incremental marking if
  // the runtime flag is provided, basically exercising a stress test.
  return (GcState() == kNoGCScheduled || GcState() == kSweeping) &&
         RuntimeEnabledFeatures::HeapIncrementalMarkingEnabled();
}

bool ThreadState::ShouldScheduleIdleGC() {
  if (GcState() != kNoGCScheduled)
    return false;
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold, 1024 * 1024,
                          1.5);
}

bool ThreadState::ShouldScheduleV8FollowupGC() {
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024, 1.5);
}

bool ThreadState::ShouldSchedulePageNavigationGC(
    float estimated_removal_ratio) {
  // If estimatedRemovalRatio is low we should let IdleGC handle this.
  if (estimated_removal_ratio < 0.01)
    return false;
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024,
                          1.5 * (1 - estimated_removal_ratio));
}

bool ThreadState::ShouldForceConservativeGC() {
  // TODO(haraken): 400% is too large. Lower the heap growing factor.
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024, 5.0);
}

// If we're consuming too much memory, trigger a conservative GC
// aggressively. This is a safe guard to avoid OOM.
bool ThreadState::ShouldForceMemoryPressureGC() {
  if (TotalMemorySize() < 300 * 1024 * 1024)
    return false;
  return JudgeGCThreshold(0, 0, 1.5);
}

void ThreadState::ScheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType gc_type) {
  DCHECK(CheckThread());
  ThreadHeap::ReportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
  DataLogF("ThreadState::scheduleV8FollowupGCIfNeeded (gcType=%s)\n",
           gc_type == BlinkGC::kV8MajorGC ? "MajorGC" : "MinorGC");
#endif

  if (IsGCForbidden())
    return;

  // This completeSweep() will do nothing in common cases since we've
  // called completeSweep() before V8 starts minor/major GCs.
  CompleteSweep();
  DCHECK(!IsSweepingInProgress());
  DCHECK(!SweepForbidden());

  if ((gc_type == BlinkGC::kV8MajorGC && ShouldForceMemoryPressureGC()) ||
      ShouldScheduleV8FollowupGC()) {
#if PRINT_HEAP_STATS
    DataLogF("Scheduled PreciseGC\n");
#endif
    SchedulePreciseGC();
    return;
  }
  if (gc_type == BlinkGC::kV8MajorGC && ShouldScheduleIdleGC()) {
#if PRINT_HEAP_STATS
    DataLogF("Scheduled IdleGC\n");
#endif
    ScheduleIdleGC();
    return;
  }
}

void ThreadState::WillStartV8GC(BlinkGC::V8GCType gc_type) {
  // Finish Oilpan's complete sweeping before running a V8 major GC.
  // This will let the GC collect more V8 objects.
  //
  // TODO(haraken): It's a bit too late for a major GC to schedule
  // completeSweep() here, because gcPrologue for a major GC is called
  // not at the point where the major GC started but at the point where
  // the major GC requests object grouping.
  if (gc_type == BlinkGC::kV8MajorGC)
    CompleteSweep();
}

void ThreadState::SchedulePageNavigationGCIfNeeded(
    float estimated_removal_ratio) {
  DCHECK(CheckThread());
  ThreadHeap::ReportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
  DataLogF(
      "ThreadState::schedulePageNavigationGCIfNeeded "
      "(estimatedRemovalRatio=%.2lf)\n",
      estimated_removal_ratio);
#endif

  if (IsGCForbidden())
    return;

  // Finish on-going lazy sweeping.
  // TODO(haraken): It might not make sense to force completeSweep() for all
  // page navigations.
  CompleteSweep();
  DCHECK(!IsSweepingInProgress());
  DCHECK(!SweepForbidden());

  if (ShouldForceMemoryPressureGC()) {
#if PRINT_HEAP_STATS
    DataLogF("Scheduled MemoryPressureGC\n");
#endif
    CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kGCWithoutSweep,
                   BlinkGC::kMemoryPressureGC);
    return;
  }
  if (ShouldSchedulePageNavigationGC(estimated_removal_ratio)) {
#if PRINT_HEAP_STATS
    DataLogF("Scheduled PageNavigationGC\n");
#endif
    SchedulePageNavigationGC();
  }
}

void ThreadState::SchedulePageNavigationGC() {
  DCHECK(CheckThread());
  DCHECK(!IsSweepingInProgress());
  SetGCState(kPageNavigationGCScheduled);
}

void ThreadState::ScheduleGCIfNeeded() {
  DCHECK(CheckThread());
  ThreadHeap::ReportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
  DataLogF("ThreadState::scheduleGCIfNeeded\n");
#endif

  // Allocation is allowed during sweeping, but those allocations should not
  // trigger nested GCs.
  if (IsGCForbidden() || SweepForbidden())
    return;

  ReportMemoryToV8();

  if (ShouldForceMemoryPressureGC()) {
    CompleteSweep();
    if (ShouldForceMemoryPressureGC()) {
#if PRINT_HEAP_STATS
      DataLogF("Scheduled MemoryPressureGC\n");
#endif
      CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kGCWithoutSweep,
                     BlinkGC::kMemoryPressureGC);
      return;
    }
  }

  if (ShouldForceConservativeGC()) {
    CompleteSweep();
    if (ShouldForceConservativeGC()) {
#if PRINT_HEAP_STATS
      DataLogF("Scheduled ConservativeGC\n");
#endif
      CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kGCWithoutSweep,
                     BlinkGC::kConservativeGC);
      return;
    }
  }

  if (ShouldScheduleIdleGC()) {
#if PRINT_HEAP_STATS
    DataLogF("Scheduled IdleGC\n");
#endif
    ScheduleIdleGC();
    return;
  }

  if (ShouldScheduleIncrementalMarking()) {
#if PRINT_HEAP_STATS
    DataLogF("Scheduled IncrementalMarking\n");
#endif
    ScheduleIncrementalMarkingStart();
  }
}

ThreadState* ThreadState::FromObject(const void* object) {
  DCHECK(object);
  BasePage* page = PageFromObject(object);
  DCHECK(page);
  DCHECK(page->Arena());
  return page->Arena()->GetThreadState();
}

void ThreadState::PerformIdleGC(double deadline_seconds) {
  DCHECK(CheckThread());
  DCHECK(Platform::Current()->CurrentThread()->Scheduler());

  if (GcState() != kIdleGCScheduled)
    return;

  if (IsGCForbidden()) {
    // If GC is forbidden at this point, try again.
    ScheduleIdleGC();
    return;
  }

  double idle_delta_in_seconds =
      deadline_seconds - MonotonicallyIncreasingTime();
  if (idle_delta_in_seconds <= heap_->HeapStats().EstimatedMarkingTime() &&
      !Platform::Current()
           ->CurrentThread()
           ->Scheduler()
           ->CanExceedIdleDeadlineIfRequired()) {
    // If marking is estimated to take longer than the deadline and we can't
    // exceed the deadline, then reschedule for the next idle period.
    ScheduleIdleGC();
    return;
  }

  TRACE_EVENT2("blink_gc", "ThreadState::performIdleGC", "idleDeltaInSeconds",
               idle_delta_in_seconds, "estimatedMarkingTime",
               heap_->HeapStats().EstimatedMarkingTime());
  CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kGCWithoutSweep,
                 BlinkGC::kIdleGC);
}

void ThreadState::PerformIdleLazySweep(double deadline_seconds) {
  DCHECK(CheckThread());

  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // This check is here to prevent performIdleLazySweep() from being called
  // recursively. I'm not sure if it can happen but it would be safer to have
  // the check just in case.
  if (SweepForbidden())
    return;

  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kPerformIdleLazySweep);

  TRACE_EVENT1("blink_gc,devtools.timeline",
               "ThreadState::performIdleLazySweep", "idleDeltaInSeconds",
               deadline_seconds - MonotonicallyIncreasingTime());

  SweepForbiddenScope scope(this);
  ScriptForbiddenScope script_forbidden_scope;

  double start_time = WTF::MonotonicallyIncreasingTimeMS();
  bool sweep_completed = Heap().AdvanceLazySweep(deadline_seconds);
  // We couldn't finish the sweeping within the deadline.
  // We request another idle task for the remaining sweeping.
  if (!sweep_completed)
    ScheduleIdleLazySweep();
  AccumulateSweepingTime(WTF::MonotonicallyIncreasingTimeMS() - start_time);

  if (sweep_completed)
    PostSweep();
}

void ThreadState::ScheduleIncrementalMarkingStart() {
  // TODO(mlippautz): Incorporate incremental sweeping into incremental steps.
  if (IsSweepingInProgress())
    CompleteSweep();

  SetGCState(kIncrementalMarkingStartScheduled);
}

void ThreadState::ScheduleIncrementalMarkingStep() {
  CHECK(!IsSweepingInProgress());

  SetGCState(kIncrementalMarkingStepScheduled);
}

void ThreadState::ScheduleIncrementalMarkingFinalize() {
  CHECK(!IsSweepingInProgress());

  SetGCState(kIncrementalMarkingFinalizeScheduled);
}

void ThreadState::ScheduleIdleGC() {
  if (IsSweepingInProgress()) {
    SetGCState(kSweepingAndIdleGCScheduled);
    return;
  }

  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  if (!Platform::Current()->CurrentThread()->Scheduler())
    return;

  Platform::Current()->CurrentThread()->Scheduler()->PostNonNestableIdleTask(
      BLINK_FROM_HERE,
      WTF::Bind(&ThreadState::PerformIdleGC, WTF::Unretained(this)));
  SetGCState(kIdleGCScheduled);
}

void ThreadState::ScheduleIdleLazySweep() {
  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  if (!Platform::Current()->CurrentThread()->Scheduler())
    return;

  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      BLINK_FROM_HERE,
      WTF::Bind(&ThreadState::PerformIdleLazySweep, WTF::Unretained(this)));
}

void ThreadState::SchedulePreciseGC() {
  DCHECK(CheckThread());
  if (IsSweepingInProgress()) {
    SetGCState(kSweepingAndPreciseGCScheduled);
    return;
  }

  SetGCState(kPreciseGCScheduled);
}

namespace {

#define UNEXPECTED_GCSTATE(s)                                   \
  case ThreadState::s:                                          \
    LOG(FATAL) << "Unexpected transition while in GCState " #s; \
    return

void UnexpectedGCState(ThreadState::GCState gc_state) {
  switch (gc_state) {
    UNEXPECTED_GCSTATE(kNoGCScheduled);
    UNEXPECTED_GCSTATE(kIdleGCScheduled);
    UNEXPECTED_GCSTATE(kPreciseGCScheduled);
    UNEXPECTED_GCSTATE(kFullGCScheduled);
    UNEXPECTED_GCSTATE(kGCRunning);
    UNEXPECTED_GCSTATE(kSweeping);
    UNEXPECTED_GCSTATE(kSweepingAndIdleGCScheduled);
    UNEXPECTED_GCSTATE(kSweepingAndPreciseGCScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStartScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStepScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingFinalizeScheduled);
    UNEXPECTED_GCSTATE(kPageNavigationGCScheduled);
  }
}

#undef UNEXPECTED_GCSTATE

}  // namespace

#define VERIFY_STATE_TRANSITION(condition) \
  if (UNLIKELY(!(condition)))              \
  UnexpectedGCState(gc_state_)

void ThreadState::SetGCState(GCState gc_state) {
  switch (gc_state) {
    case kNoGCScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(
          gc_state_ == kSweeping || gc_state_ == kSweepingAndIdleGCScheduled ||
          gc_state_ == kIncrementalMarkingFinalizeScheduled);
      break;
    case kIncrementalMarkingStartScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION((gc_state_ == kSweeping) ||
                              (gc_state_ == kNoGCScheduled));
      break;
    case kIncrementalMarkingStepScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStartScheduled ||
                              gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    case kIncrementalMarkingFinalizeScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    case kIdleGCScheduled:
    case kPreciseGCScheduled:
    case kFullGCScheduled:
    case kPageNavigationGCScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(
          gc_state_ == kNoGCScheduled || gc_state_ == kIdleGCScheduled ||
          gc_state_ == kPreciseGCScheduled || gc_state_ == kFullGCScheduled ||
          gc_state_ == kPageNavigationGCScheduled || gc_state_ == kSweeping ||
          gc_state_ == kSweepingAndIdleGCScheduled ||
          gc_state_ == kSweepingAndPreciseGCScheduled);
      CompleteSweep();
      break;
    case kGCRunning:
      DCHECK(!IsInGC());
      VERIFY_STATE_TRANSITION(gc_state_ != kGCRunning);
      break;
    case kSweeping:
      DCHECK(IsInGC());
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kGCRunning);
      break;
    case kSweepingAndIdleGCScheduled:
    case kSweepingAndPreciseGCScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kSweeping ||
                              gc_state_ == kSweepingAndIdleGCScheduled ||
                              gc_state_ == kSweepingAndPreciseGCScheduled);
      break;
    default:
      NOTREACHED();
  }
  gc_state_ = gc_state;
}

#undef VERIFY_STATE_TRANSITION

void ThreadState::RunScheduledGC(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());
  if (stack_state != BlinkGC::kNoHeapPointersOnStack)
    return;

  // If a safe point is entered while initiating a GC, we clearly do
  // not want to do another as part of that -- the safe point is only
  // entered after checking if a scheduled GC ought to run first.
  // Prevent that from happening by marking GCs as forbidden while
  // one is initiated and later running.
  if (IsGCForbidden())
    return;

  switch (GcState()) {
    case kFullGCScheduled:
      CollectAllGarbage();
      break;
    case kPreciseGCScheduled:
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kGCWithoutSweep,
                     BlinkGC::kPreciseGC);
      break;
    case kPageNavigationGCScheduled:
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kGCWithSweep,
                     BlinkGC::kPageNavigationGC);
      break;
    case kIdleGCScheduled:
      // Idle time GC will be scheduled by Blink Scheduler.
      break;
    case kIncrementalMarkingStartScheduled:
      IncrementalMarkingStart();
      break;
    case kIncrementalMarkingStepScheduled:
      IncrementalMarkingStep();
      break;
    case kIncrementalMarkingFinalizeScheduled:
      IncrementalMarkingFinalize();
      break;
    default:
      break;
  }
}

void ThreadState::PreSweep(BlinkGC::GCType gc_type) {
  DCHECK(IsInGC());
  DCHECK(CheckThread());
  Heap().PrepareForSweep();

  if (gc_type == BlinkGC::kTakeSnapshot) {
    Heap().TakeSnapshot(ThreadHeap::SnapshotType::kHeapSnapshot);

    // This unmarks all marked objects and marks all unmarked objects dead.
    Heap().MakeConsistentForMutator();

    Heap().TakeSnapshot(ThreadHeap::SnapshotType::kFreelistSnapshot);

    // Force setting NoGCScheduled to circumvent checkThread()
    // in setGCState().
    gc_state_ = kNoGCScheduled;
    return;
  }

  // We have to set the GCState to Sweeping before calling pre-finalizers
  // to disallow a GC during the pre-finalizers.
  SetGCState(kSweeping);

  // Allocation is allowed during the pre-finalizers and destructors.
  // However, they must not mutate an object graph in a way in which
  // a dead object gets resurrected.
  InvokePreFinalizers();

  accumulated_sweeping_time_ = 0;

  EagerSweep();

  // Any sweep compaction must happen after pre-finalizers and eager
  // sweeping, as it will finalize dead objects in compactable arenas
  // (e.g., backing stores for container objects.)
  //
  // As per-contract for prefinalizers, those finalizable objects must
  // still be accessible when the prefinalizer runs, hence we cannot
  // schedule compaction until those have run. Similarly for eager sweeping.
  {
    SweepForbiddenScope scope(this);
    ScriptForbiddenScope script_forbidden_scope;
    NoAllocationScope no_allocation_scope(this);
    Heap().Compact();
  }

#if defined(ADDRESS_SANITIZER)
  Heap().PoisonAllHeaps();
#endif

  if (gc_type == BlinkGC::kGCWithSweep) {
    // Eager sweeping should happen only in testing.
    CompleteSweep();
  } else {
    DCHECK(gc_type == BlinkGC::kGCWithoutSweep);
    // The default behavior is lazy sweeping.
    ScheduleIdleLazySweep();
  }
}

void ThreadState::EagerSweep() {
#if defined(ADDRESS_SANITIZER)
  Heap().PoisonEagerArena();
#endif
  DCHECK(CheckThread());
  // Some objects need to be finalized promptly and cannot be handled
  // by lazy sweeping. Keep those in a designated heap and sweep it
  // eagerly.
  DCHECK(IsSweepingInProgress());

  SweepForbiddenScope scope(this);
  ScriptForbiddenScope script_forbidden_scope;

  double start_time = WTF::MonotonicallyIncreasingTimeMS();
  Heap().Arena(BlinkGC::kEagerSweepArenaIndex)->CompleteSweep();
  AccumulateSweepingTime(WTF::MonotonicallyIncreasingTimeMS() - start_time);
}

void ThreadState::CompleteSweep() {
  DCHECK(CheckThread());
  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // completeSweep() can be called recursively if finalizers can allocate
  // memory and the allocation triggers completeSweep(). This check prevents
  // the sweeping from being executed recursively.
  if (SweepForbidden())
    return;

  SweepForbiddenScope scope(this);
  ScriptForbiddenScope script_forbidden_scope;

  TRACE_EVENT0("blink_gc,devtools.timeline", "ThreadState::completeSweep");
  double start_time = WTF::MonotonicallyIncreasingTimeMS();

  Heap().CompleteSweep();

  double time_for_complete_sweep =
      WTF::MonotonicallyIncreasingTimeMS() - start_time;
  AccumulateSweepingTime(time_for_complete_sweep);

  if (IsMainThread()) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, complete_sweep_histogram,
                        ("BlinkGC.CompleteSweep", 1, 10 * 1000, 50));
    complete_sweep_histogram.Count(time_for_complete_sweep);
  }

  PostSweep();
}

BlinkGCObserver::BlinkGCObserver(ThreadState* thread_state)
    : thread_state_(thread_state) {
  thread_state_->AddObserver(this);
}

BlinkGCObserver::~BlinkGCObserver() {
  thread_state_->RemoveObserver(this);
}

void ThreadState::PostSweep() {
  DCHECK(CheckThread());
  ThreadHeap::ReportMemoryUsageForTracing();

  if (IsMainThread()) {
    double collection_rate = 0;
    if (heap_->HeapStats().ObjectSizeAtLastGC() > 0)
      collection_rate = 1 - 1.0 * heap_->HeapStats().MarkedObjectSize() /
                                heap_->HeapStats().ObjectSizeAtLastGC();
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                   "ThreadState::collectionRate",
                   static_cast<int>(100 * collection_rate));

#if PRINT_HEAP_STATS
    DataLogF("ThreadState::postSweep (collectionRate=%d%%)\n",
             static_cast<int>(100 * collection_rate));
#endif

    // ThreadHeap::markedObjectSize() may be underestimated here if any other
    // thread has not yet finished lazy sweeping.
    heap_->HeapStats().SetMarkedObjectSizeAtLastCompleteSweep(
        heap_->HeapStats().MarkedObjectSize());

    DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_before_gc_histogram,
                        ("BlinkGC.ObjectSizeBeforeGC", 1, 4 * 1024 * 1024, 50));
    object_size_before_gc_histogram.Count(
        heap_->HeapStats().ObjectSizeAtLastGC() / 1024);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_after_gc_histogram,
                        ("BlinkGC.ObjectSizeAfterGC", 1, 4 * 1024 * 1024, 50));
    object_size_after_gc_histogram.Count(heap_->HeapStats().MarkedObjectSize() /
                                         1024);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, collection_rate_histogram,
                        ("BlinkGC.CollectionRate", 1, 100, 20));
    collection_rate_histogram.Count(static_cast<int>(100 * collection_rate));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, time_for_sweep_histogram,
        ("BlinkGC.TimeForSweepingAllObjects", 1, 10 * 1000, 50));
    time_for_sweep_histogram.Count(accumulated_sweeping_time_);

#define COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(GCReason)              \
  case BlinkGC::k##GCReason: {                                              \
    DEFINE_STATIC_LOCAL(CustomCountHistogram, histogram,                    \
                        ("BlinkGC.CollectionRate_" #GCReason, 1, 100, 20)); \
    histogram.Count(static_cast<int>(100 * collection_rate));               \
    break;                                                                  \
  }

    switch (current_gc_data_.reason) {
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(IdleGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(PreciseGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(ConservativeGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(ForcedGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(MemoryPressureGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(PageNavigationGC)
      default:
        break;
    }
  }

  switch (GcState()) {
    case kSweeping:
      SetGCState(kNoGCScheduled);
      break;
    case kSweepingAndPreciseGCScheduled:
      SetGCState(kPreciseGCScheduled);
      break;
    case kSweepingAndIdleGCScheduled:
      SetGCState(kNoGCScheduled);
      ScheduleIdleGC();
      break;
    default:
      NOTREACHED();
  }

  gc_age_++;

  for (const auto& observer : observers_)
    observer->OnCompleteSweepDone();
}

void ThreadState::SafePoint(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());
  ThreadHeap::ReportMemoryUsageForTracing();

  RunScheduledGC(stack_state);
  stack_state_ = BlinkGC::kHeapPointersOnStack;
}

#ifdef ADDRESS_SANITIZER
// When we are running under AddressSanitizer with
// detect_stack_use_after_return=1 then stack marker obtained from
// SafePointScope will point into a fake stack.  Detect this case by checking if
// it falls in between current stack frame and stack start and use an arbitrary
// high enough value for it.  Don't adjust stack marker in any other case to
// match behavior of code running without AddressSanitizer.
NO_SANITIZE_ADDRESS static void* AdjustScopeMarkerForAdressSanitizer(
    void* scope_marker) {
  Address start = reinterpret_cast<Address>(WTF::GetStackStart());
  Address end = reinterpret_cast<Address>(&start);
  CHECK_LT(end, start);

  if (end <= scope_marker && scope_marker < start)
    return scope_marker;

  // 256 is as good an approximation as any else.
  const size_t kBytesToCopy = sizeof(Address) * 256;
  if (static_cast<size_t>(start - end) < kBytesToCopy)
    return start;

  return end + kBytesToCopy;
}
#endif

// TODO(haraken): The first void* pointer is unused. Remove it.
using PushAllRegistersCallback = void (*)(void*, ThreadState*, intptr_t*);
extern "C" void PushAllRegisters(void*, ThreadState*, PushAllRegistersCallback);

static void EnterSafePointAfterPushRegisters(void*,
                                             ThreadState* state,
                                             intptr_t* stack_end) {
  state->RecordStackEnd(stack_end);
  state->CopyStackUntilSafePointScope();
}

void ThreadState::EnterSafePoint(BlinkGC::StackState stack_state,
                                 void* scope_marker) {
  DCHECK(CheckThread());
#ifdef ADDRESS_SANITIZER
  if (stack_state == BlinkGC::kHeapPointersOnStack)
    scope_marker = AdjustScopeMarkerForAdressSanitizer(scope_marker);
#endif
  DCHECK(stack_state == BlinkGC::kNoHeapPointersOnStack || scope_marker);
  DCHECK(IsGCForbidden());
  stack_state_ = stack_state;
  safe_point_scope_marker_ = scope_marker;
  PushAllRegisters(nullptr, this, EnterSafePointAfterPushRegisters);
}

void ThreadState::LeaveSafePoint() {
  DCHECK(CheckThread());
  stack_state_ = BlinkGC::kHeapPointersOnStack;
  ClearSafePointScopeMarker();
}

void ThreadState::AddObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void ThreadState::RemoveObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

void ThreadState::ReportMemoryToV8() {
  if (!isolate_)
    return;

  size_t current_heap_size = heap_->HeapStats().AllocatedObjectSize() +
                             heap_->HeapStats().MarkedObjectSize();
  int64_t diff = static_cast<int64_t>(current_heap_size) -
                 static_cast<int64_t>(reported_memory_to_v8_);
  isolate_->AdjustAmountOfExternalAllocatedMemory(diff);
  reported_memory_to_v8_ = current_heap_size;
}

void ThreadState::CopyStackUntilSafePointScope() {
  if (!safe_point_scope_marker_ ||
      stack_state_ == BlinkGC::kNoHeapPointersOnStack)
    return;

  Address* to = reinterpret_cast<Address*>(safe_point_scope_marker_);
  Address* from = reinterpret_cast<Address*>(end_of_stack_);
  CHECK_LT(from, to);
  CHECK_LE(to, reinterpret_cast<Address*>(start_of_stack_));
  size_t slot_count = static_cast<size_t>(to - from);
// Catch potential performance issues.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
  // ASan/LSan use more space on the stack and we therefore
  // increase the allowed stack copying for those builds.
  DCHECK_LT(slot_count, 2048u);
#else
  DCHECK_LT(slot_count, 1024u);
#endif

  DCHECK(!safe_point_stack_copy_.size());
  safe_point_stack_copy_.resize(slot_count);
  for (size_t i = 0; i < slot_count; ++i) {
    safe_point_stack_copy_[i] = from[i];
  }
}

void ThreadState::RegisterStaticPersistentNode(
    PersistentNode* node,
    PersistentClearCallback callback) {
#if defined(LEAK_SANITIZER)
  if (disabled_static_persistent_registration_)
    return;
#endif

  DCHECK(!static_persistents_.Contains(node));
  static_persistents_.insert(node, callback);
}

void ThreadState::ReleaseStaticPersistentNodes() {
  HashMap<PersistentNode*, ThreadState::PersistentClearCallback>
      static_persistents;
  static_persistents.swap(static_persistents_);

  PersistentRegion* persistent_region = GetPersistentRegion();
  for (const auto& it : static_persistents)
    persistent_region->ReleasePersistentNode(it.key, it.value);
}

void ThreadState::FreePersistentNode(PersistentNode* persistent_node) {
  PersistentRegion* persistent_region = GetPersistentRegion();
  persistent_region->FreePersistentNode(persistent_node);
  // Do not allow static persistents to be freed before
  // they're all released in releaseStaticPersistentNodes().
  //
  // There's no fundamental reason why this couldn't be supported,
  // but no known use for it.
  DCHECK(!static_persistents_.Contains(persistent_node));
}

#if defined(LEAK_SANITIZER)
void ThreadState::enterStaticReferenceRegistrationDisabledScope() {
  disabled_static_persistent_registration_++;
}

void ThreadState::leaveStaticReferenceRegistrationDisabledScope() {
  DCHECK(disabled_static_persistent_registration_);
  disabled_static_persistent_registration_--;
}
#endif

void ThreadState::InvokePreFinalizers() {
  DCHECK(CheckThread());
  DCHECK(!SweepForbidden());
  TRACE_EVENT0("blink_gc", "ThreadState::invokePreFinalizers");

  SweepForbiddenScope sweep_forbidden(this);
  ScriptForbiddenScope script_forbidden;
  // Pre finalizers may access unmarked objects but are forbidden from
  // ressurecting them.
  ObjectResurrectionForbiddenScope object_resurrection_forbidden(this);

  double start_time = WTF::MonotonicallyIncreasingTimeMS();
  if (!ordered_pre_finalizers_.IsEmpty()) {
    // Call the prefinalizers in the opposite order to their registration.
    //
    // The prefinalizer callback wrapper returns |true| when its associated
    // object is unreachable garbage and the prefinalizer callback has run.
    // The registered prefinalizer entry must then be removed and deleted.
    //
    auto it = --ordered_pre_finalizers_.end();
    bool done;
    do {
      auto entry = it;
      done = it == ordered_pre_finalizers_.begin();
      if (!done)
        --it;
      if ((entry->second)(entry->first))
        ordered_pre_finalizers_.erase(entry);
    } while (!done);
  }
  if (IsMainThread()) {
    double time_for_invoking_pre_finalizers =
        WTF::MonotonicallyIncreasingTimeMS() - start_time;
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, pre_finalizers_histogram,
        ("BlinkGC.TimeForInvokingPreFinalizers", 1, 10 * 1000, 50));
    pre_finalizers_histogram.Count(time_for_invoking_pre_finalizers);
  }
}

void ThreadState::IncrementalMarkingStart() {
  DataLogF("IncrementalMarkingStart\n");
  Heap().EnableIncrementalMarkingBarrier();
  ScheduleIncrementalMarkingStep();
}

void ThreadState::IncrementalMarkingStep() {
  DataLogF("IncrementalMarkingStep\n");
  ScheduleIncrementalMarkingFinalize();
}

void ThreadState::IncrementalMarkingFinalize() {
  DataLogF("IncrementalMarkingFinalize\n");
  Heap().DisableIncrementalMarkingBarrier();
  SetGCState(kNoGCScheduled);
}

void ThreadState::CollectGarbage(BlinkGC::StackState stack_state,
                                 BlinkGC::GCType gc_type,
                                 BlinkGC::GCReason reason) {
  // Nested collectGarbage() invocations aren't supported.
  CHECK(!IsGCForbidden());
  CompleteSweep();

  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kCollectGarbage);

  GCForbiddenScope gc_forbidden_scope(this);

  {
    // Access to the CrossThreadPersistentRegion has to be prevented
    // while in the marking phase because otherwise other threads may
    // allocate or free PersistentNodes and we can't handle
    // that. Grabbing this lock also prevents non-attached threads
    // from accessing any GCed heap while a GC runs.
    CrossThreadPersistentRegion::LockScope persistent_lock(
        ProcessHeap::GetCrossThreadPersistentRegion());

    {
      TRACE_EVENT2("blink_gc,devtools.timeline", "BlinkGCMarking",
                   "lazySweeping", gc_type == BlinkGC::kGCWithoutSweep,
                   "gcReason", GcReasonString(reason));
      MarkPhasePrologue(stack_state, gc_type, reason);
      MarkPhaseVisitRoots();
      CHECK(MarkPhaseAdvanceMarking(std::numeric_limits<double>::infinity()));
      MarkPhaseEpilogue();
    }
  }

#if PRINT_HEAP_STATS
  DataLogF(
      "ThreadHeap::collectGarbage (gcReason=%s, lazySweeping=%d, "
      "time=%.1lfms)\n",
      GcReasonString(reason), gc_type == BlinkGC::kGCWithoutSweep,
      current_gc_data_.marking_time_in_milliseconds);
#endif

  PreSweep(gc_type);
}

void ThreadState::MarkPhasePrologue(BlinkGC::StackState stack_state,
                                    BlinkGC::GCType gc_type,
                                    BlinkGC::GCReason reason) {
  current_gc_data_.stack_state = stack_state;
  current_gc_data_.gc_type = gc_type;
  current_gc_data_.reason = reason;
  current_gc_data_.marking_time_in_milliseconds = 0;

  if (gc_type == BlinkGC::kTakeSnapshot) {
    current_gc_data_.visitor = Visitor::Create(this, Visitor::kSnapshotMarking);
  } else {
    DCHECK(gc_type == BlinkGC::kGCWithSweep ||
           gc_type == BlinkGC::kGCWithoutSweep);
    if (Heap().Compaction()->ShouldCompact(&Heap(), stack_state, gc_type,
                                           reason)) {
      Heap().Compaction()->Initialize(this);
      current_gc_data_.visitor =
          Visitor::Create(this, Visitor::kGlobalMarkingWithCompaction);
    } else {
      current_gc_data_.visitor = Visitor::Create(this, Visitor::kGlobalMarking);
    }
  }

  if (gc_type == BlinkGC::kTakeSnapshot)
    BlinkGCMemoryDumpProvider::Instance()->ClearProcessDumpForCurrentGC();

  Heap().CommitCallbackStacks();
  if (isolate_ && perform_cleanup_)
    perform_cleanup_(isolate_);

  DCHECK(!IsInGC());
  SetGCState(kGCRunning);
  Heap().MakeConsistentForGC();
  Heap().FlushHeapDoesNotContainCacheIfNeeded();
  Heap().ClearArenaAges();

  current_gc_data_.marked_object_size =
      Heap().HeapStats().AllocatedObjectSize() +
      Heap().HeapStats().MarkedObjectSize();
  if (gc_type != BlinkGC::kTakeSnapshot)
    Heap().ResetHeapCounters();
}

void ThreadState::MarkPhaseVisitRoots() {
  double start_time = WTF::MonotonicallyIncreasingTimeMS();

  ScriptForbiddenScope script_forbidden;
  // Disallow allocation during garbage collection (but not during the
  // finalization that happens when the visitorScope is torn down).
  NoAllocationScope no_allocation_scope(this);
  StackFrameDepthScope stack_depth_scope(&Heap().GetStackFrameDepth());

  // 1. Trace persistent roots.
  Heap().VisitPersistentRoots(current_gc_data_.visitor.get());

  // 2. Trace objects reachable from the stack.
  {
    SafePointScope safe_point_scope(current_gc_data_.stack_state, this);
    Heap().VisitStackRoots(current_gc_data_.visitor.get());
  }
  current_gc_data_.marking_time_in_milliseconds +=
      WTF::MonotonicallyIncreasingTimeMS() - start_time;
}

bool ThreadState::MarkPhaseAdvanceMarking(double deadline_seconds) {
  double start_time = WTF::MonotonicallyIncreasingTimeMS();

  ScriptForbiddenScope script_forbidden;
  // Disallow allocation during garbage collection (but not during the
  // finalization that happens when the visitorScope is torn down).
  NoAllocationScope no_allocation_scope(this);
  StackFrameDepthScope stack_depth_scope(&Heap().GetStackFrameDepth());

  // 3. Transitive closure to trace objects including ephemerons.
  bool complete = Heap().AdvanceMarkingStackProcessing(
      current_gc_data_.visitor.get(), deadline_seconds);

  current_gc_data_.marking_time_in_milliseconds +=
      WTF::MonotonicallyIncreasingTimeMS() - start_time;
  return complete;
}

void ThreadState::MarkPhaseEpilogue() {
  Heap().PostMarkingProcessing(current_gc_data_.visitor.get());
  Heap().WeakProcessing(current_gc_data_.visitor.get());
  Heap().DecommitCallbackStacks();

  Heap().HeapStats().SetEstimatedMarkingTimePerByte(
      current_gc_data_.marked_object_size
          ? (current_gc_data_.marking_time_in_milliseconds / 1000 /
             current_gc_data_.marked_object_size)
          : 0);

  ThreadHeap::ReportMemoryUsageHistogram();
  WTF::Partitions::ReportMemoryUsageHistogram();

  if (invalidate_dead_objects_in_wrappers_marking_deque_)
    invalidate_dead_objects_in_wrappers_marking_deque_(isolate_);

  DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, marking_time_histogram,
                                  ("BlinkGC.CollectGarbage", 0, 10 * 1000, 50));
  marking_time_histogram.Count(current_gc_data_.marking_time_in_milliseconds);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_object_space_histogram,
      ("BlinkGC.TotalObjectSpace", 0, 4 * 1024 * 1024, 50));
  total_object_space_histogram.Count(ProcessHeap::TotalAllocatedObjectSize() /
                                     1024);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_allocated_space_histogram,
      ("BlinkGC.TotalAllocatedSpace", 0, 4 * 1024 * 1024, 50));
  total_allocated_space_histogram.Count(ProcessHeap::TotalAllocatedSpace() /
                                        1024);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gc_reason_histogram,
      ("BlinkGC.GCReason", BlinkGC::kLastGCReason + 1));
  gc_reason_histogram.Count(current_gc_data_.reason);
}

void ThreadState::CollectAllGarbage() {
  // We need to run multiple GCs to collect a chain of persistent handles.
  size_t previous_live_objects = 0;
  for (int i = 0; i < 5; ++i) {
    CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kGCWithSweep,
                   BlinkGC::kForcedGC);
    size_t live_objects = Heap().HeapStats().MarkedObjectSize();
    if (live_objects == previous_live_objects)
      break;
    previous_live_objects = live_objects;
  }
}

}  // namespace blink

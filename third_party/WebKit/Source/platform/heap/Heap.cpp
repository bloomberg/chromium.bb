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

#include "platform/heap/Heap.h"

#include <memory>
#include "base/trace_event/process_memory_dump.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/HeapCompact.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/PagePool.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "platform/instrumentation/tracing/web_process_memory_dump.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/DataLog.h"
#include "platform/wtf/LeakAnnotations.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/allocator/Partitions.h"
#include "public/platform/Platform.h"

namespace blink {

HeapAllocHooks::AllocationHook* HeapAllocHooks::allocation_hook_ = nullptr;
HeapAllocHooks::FreeHook* HeapAllocHooks::free_hook_ = nullptr;

void ThreadHeap::FlushHeapDoesNotContainCache() {
  heap_does_not_contain_cache_->Flush();
}

void ProcessHeap::Init() {
  total_allocated_space_ = 0;
  total_allocated_object_size_ = 0;
  total_marked_object_size_ = 0;

  GCInfoTable::Init();
  CallbackStackMemoryPool::Instance().Initialize();
}

void ProcessHeap::ResetHeapCounters() {
  total_allocated_object_size_ = 0;
  total_marked_object_size_ = 0;
}

CrossThreadPersistentRegion& ProcessHeap::GetCrossThreadPersistentRegion() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion,
                                  persistent_region, ());
  return persistent_region;
}

size_t ProcessHeap::total_allocated_space_ = 0;
size_t ProcessHeap::total_allocated_object_size_ = 0;
size_t ProcessHeap::total_marked_object_size_ = 0;

ThreadHeapStats::ThreadHeapStats()
    : allocated_space_(0),
      allocated_object_size_(0),
      object_size_at_last_gc_(0),
      marked_object_size_(0),
      marked_object_size_at_last_complete_sweep_(0),
      wrapper_count_(0),
      wrapper_count_at_last_gc_(0),
      collected_wrapper_count_(0),
      partition_alloc_size_at_last_gc_(
          WTF::Partitions::TotalSizeOfCommittedPages()),
      estimated_marking_time_per_byte_(0.0) {}

double ThreadHeapStats::EstimatedMarkingTime() {
  // Use 8 ms as initial estimated marking time.
  // 8 ms is long enough for low-end mobile devices to mark common
  // real-world object graphs.
  if (estimated_marking_time_per_byte_ == 0)
    return 0.008;

  // Assuming that the collection rate of this GC will be mostly equal to
  // the collection rate of the last GC, estimate the marking time of this GC.
  return estimated_marking_time_per_byte_ *
         (AllocatedObjectSize() + MarkedObjectSize());
}

void ThreadHeapStats::Reset() {
  object_size_at_last_gc_ = allocated_object_size_ + marked_object_size_;
  partition_alloc_size_at_last_gc_ =
      WTF::Partitions::TotalSizeOfCommittedPages();
  allocated_object_size_ = 0;
  marked_object_size_ = 0;
  wrapper_count_at_last_gc_ = wrapper_count_;
  collected_wrapper_count_ = 0;
}

void ThreadHeapStats::IncreaseAllocatedObjectSize(size_t delta) {
  allocated_object_size_ += delta;
  ProcessHeap::IncreaseTotalAllocatedObjectSize(delta);
}

void ThreadHeapStats::DecreaseAllocatedObjectSize(size_t delta) {
  allocated_object_size_ -= delta;
  ProcessHeap::DecreaseTotalAllocatedObjectSize(delta);
}

void ThreadHeapStats::IncreaseMarkedObjectSize(size_t delta) {
  marked_object_size_ += delta;
  ProcessHeap::IncreaseTotalMarkedObjectSize(delta);
}

void ThreadHeapStats::IncreaseAllocatedSpace(size_t delta) {
  allocated_space_ += delta;
  ProcessHeap::IncreaseTotalAllocatedSpace(delta);
}

void ThreadHeapStats::DecreaseAllocatedSpace(size_t delta) {
  allocated_space_ -= delta;
  ProcessHeap::DecreaseTotalAllocatedSpace(delta);
}

ThreadHeap::ThreadHeap(ThreadState* thread_state)
    : thread_state_(thread_state),
      region_tree_(std::make_unique<RegionTree>()),
      heap_does_not_contain_cache_(
          WTF::WrapUnique(new HeapDoesNotContainCache)),
      free_page_pool_(WTF::WrapUnique(new PagePool)),
      marking_stack_(CallbackStack::Create()),
      post_marking_callback_stack_(CallbackStack::Create()),
      weak_callback_stack_(CallbackStack::Create()),
      ephemeron_stack_(CallbackStack::Create()),
      vector_backing_arena_index_(BlinkGC::kVector1ArenaIndex),
      current_arena_ages_(0),
      should_flush_heap_does_not_contain_cache_(false) {
  if (ThreadState::Current()->IsMainThread())
    main_thread_heap_ = this;

  for (int arena_index = 0; arena_index < BlinkGC::kLargeObjectArenaIndex;
       arena_index++)
    arenas_[arena_index] = new NormalPageArena(thread_state_, arena_index);
  arenas_[BlinkGC::kLargeObjectArenaIndex] =
      new LargeObjectArena(thread_state_, BlinkGC::kLargeObjectArenaIndex);

  likely_to_be_promptly_freed_ =
      WrapArrayUnique(new int[kLikelyToBePromptlyFreedArraySize]);
  ClearArenaAges();
}

ThreadHeap::~ThreadHeap() {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    delete arenas_[i];
}

Address ThreadHeap::CheckAndMarkPointer(Visitor* visitor, Address address) {
  DCHECK(thread_state_->IsInGC());

#if !DCHECK_IS_ON()
  if (heap_does_not_contain_cache_->Lookup(address))
    return nullptr;
#endif

  if (BasePage* page = LookupPageForAddress(address)) {
#if DCHECK_IS_ON()
    DCHECK(page->Contains(address));
#endif
    DCHECK(!heap_does_not_contain_cache_->Lookup(address));
    DCHECK(&visitor->Heap() == &page->Arena()->GetThreadState()->Heap());
    page->CheckAndMarkPointer(visitor, address);
    return address;
  }

#if !DCHECK_IS_ON()
  heap_does_not_contain_cache_->AddEntry(address);
#else
  if (!heap_does_not_contain_cache_->Lookup(address))
    heap_does_not_contain_cache_->AddEntry(address);
#endif
  return nullptr;
}

#if DCHECK_IS_ON()
// To support unit testing of the marking of off-heap root references
// into the heap, provide a checkAndMarkPointer() version with an
// extra notification argument.
Address ThreadHeap::CheckAndMarkPointer(
    Visitor* visitor,
    Address address,
    MarkedPointerCallbackForTesting callback) {
  DCHECK(thread_state_->IsInGC());

  if (BasePage* page = LookupPageForAddress(address)) {
    DCHECK(page->Contains(address));
    DCHECK(!heap_does_not_contain_cache_->Lookup(address));
    DCHECK(&visitor->Heap() == &page->Arena()->GetThreadState()->Heap());
    page->CheckAndMarkPointer(visitor, address, callback);
    return address;
  }
  if (!heap_does_not_contain_cache_->Lookup(address))
    heap_does_not_contain_cache_->AddEntry(address);
  return nullptr;
}
#endif

void ThreadHeap::PushTraceCallback(void* object, TraceCallback callback) {
  DCHECK(thread_state_->IsInGC());

  CallbackStack::Item* slot = marking_stack_->AllocateEntry();
  *slot = CallbackStack::Item(object, callback);
}

bool ThreadHeap::PopAndInvokeTraceCallback(Visitor* visitor) {
  CallbackStack::Item* item = marking_stack_->Pop();
  if (!item)
    return false;
  item->Call(visitor);
  return true;
}

void ThreadHeap::PushPostMarkingCallback(void* object, TraceCallback callback) {
  DCHECK(thread_state_->IsInGC());

  CallbackStack::Item* slot = post_marking_callback_stack_->AllocateEntry();
  *slot = CallbackStack::Item(object, callback);
}

bool ThreadHeap::PopAndInvokePostMarkingCallback(Visitor* visitor) {
  if (CallbackStack::Item* item = post_marking_callback_stack_->Pop()) {
    item->Call(visitor);
    return true;
  }
  return false;
}

void ThreadHeap::PushWeakCallback(void* closure, WeakCallback callback) {
  DCHECK(thread_state_->IsInGC());

  CallbackStack::Item* slot = weak_callback_stack_->AllocateEntry();
  *slot = CallbackStack::Item(closure, callback);
}

bool ThreadHeap::PopAndInvokeWeakCallback(Visitor* visitor) {
  if (CallbackStack::Item* item = weak_callback_stack_->Pop()) {
    item->Call(visitor);
    return true;
  }
  return false;
}

void ThreadHeap::RegisterWeakTable(void* table,
                                   EphemeronCallback iteration_callback,
                                   EphemeronCallback iteration_done_callback) {
  DCHECK(thread_state_->IsInGC());

  CallbackStack::Item* slot = ephemeron_stack_->AllocateEntry();
  *slot = CallbackStack::Item(table, iteration_callback);

  // Register a post-marking callback to tell the tables that
  // ephemeron iteration is complete.
  PushPostMarkingCallback(table, iteration_done_callback);
}

#if DCHECK_IS_ON()
bool ThreadHeap::WeakTableRegistered(const void* table) {
  DCHECK(ephemeron_stack_);
  return ephemeron_stack_->HasCallbackForObject(table);
}
#endif

void ThreadHeap::CommitCallbackStacks() {
  marking_stack_->Commit();
  post_marking_callback_stack_->Commit();
  weak_callback_stack_->Commit();
  ephemeron_stack_->Commit();
}

HeapCompact* ThreadHeap::Compaction() {
  if (!compaction_)
    compaction_ = HeapCompact::Create();
  return compaction_.get();
}

void ThreadHeap::RegisterMovingObjectReference(MovableReference* slot) {
  DCHECK(slot);
  DCHECK(*slot);
  Compaction()->RegisterMovingObjectReference(slot);
}

void ThreadHeap::RegisterMovingObjectCallback(MovableReference reference,
                                              MovingObjectCallback callback,
                                              void* callback_data) {
  DCHECK(reference);
  Compaction()->RegisterMovingObjectCallback(reference, callback,
                                             callback_data);
}

void ThreadHeap::DecommitCallbackStacks() {
  marking_stack_->Decommit();
  post_marking_callback_stack_->Decommit();
  weak_callback_stack_->Decommit();
  ephemeron_stack_->Decommit();
}

void ThreadHeap::ProcessMarkingStack(Visitor* visitor) {
  bool complete = AdvanceMarkingStackProcessing(
      visitor, std::numeric_limits<double>::infinity());
  CHECK(complete);
}

bool ThreadHeap::AdvanceMarkingStackProcessing(Visitor* visitor,
                                               double deadline_seconds) {
  const size_t kDeadlineCheckInterval = 2500;
  size_t processed_callback_count = 0;
  // Ephemeron fixed point loop.
  do {
    {
      // Iteratively mark all objects that are reachable from the objects
      // currently pushed onto the marking stack.
      TRACE_EVENT0("blink_gc", "ThreadHeap::processMarkingStackSingleThreaded");
      while (PopAndInvokeTraceCallback(visitor)) {
        processed_callback_count++;
        if (processed_callback_count % kDeadlineCheckInterval == 0) {
          if (deadline_seconds <= MonotonicallyIncreasingTime()) {
            return false;
          }
        }
      }
    }

    {
      // Mark any strong pointers that have now become reachable in
      // ephemeron maps.
      TRACE_EVENT0("blink_gc", "ThreadHeap::processEphemeronStack");
      ephemeron_stack_->InvokeEphemeronCallbacks(visitor);
    }

    // Rerun loop if ephemeron processing queued more objects for tracing.
  } while (!marking_stack_->IsEmpty());
  return true;
}

void ThreadHeap::PostMarkingProcessing(Visitor* visitor) {
  TRACE_EVENT0("blink_gc", "ThreadHeap::postMarkingProcessing");
  // Call post-marking callbacks including:
  // 1. the ephemeronIterationDone callbacks on weak tables to do cleanup
  //    (specifically to clear the queued bits for weak hash tables), and
  // 2. the markNoTracing callbacks on collection backings to mark them
  //    if they are only reachable from their front objects.
  while (PopAndInvokePostMarkingCallback(visitor)) {
  }

  // Post-marking callbacks should not trace any objects and
  // therefore the marking stack should be empty after the
  // post-marking callbacks.
  DCHECK(marking_stack_->IsEmpty());
}

void ThreadHeap::WeakProcessing(Visitor* visitor) {
  TRACE_EVENT0("blink_gc", "ThreadHeap::weakProcessing");
  double start_time = WTF::MonotonicallyIncreasingTimeMS();

  // Weak processing may access unmarked objects but are forbidden from
  // ressurecting them.
  ThreadState::ObjectResurrectionForbiddenScope object_resurrection_forbidden(
      ThreadState::Current());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  while (PopAndInvokeWeakCallback(visitor)) {
  }

  // It is not permitted to trace pointers of live objects in the weak
  // callback phase, so the marking stack should still be empty here.
  DCHECK(marking_stack_->IsEmpty());

  double time_for_weak_processing =
      WTF::MonotonicallyIncreasingTimeMS() - start_time;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, weak_processing_time_histogram,
      ("BlinkGC.TimeForGlobalWeakProcessing", 1, 10 * 1000, 50));
  weak_processing_time_histogram.Count(time_for_weak_processing);
}

void ThreadHeap::ReportMemoryUsageHistogram() {
  static size_t supported_max_size_in_mb = 4 * 1024;
  static size_t observed_max_size_in_mb = 0;

  // We only report the memory in the main thread.
  if (!IsMainThread())
    return;
  // +1 is for rounding up the sizeInMB.
  size_t size_in_mb =
      ThreadState::Current()->Heap().HeapStats().AllocatedSpace() / 1024 /
          1024 +
      1;
  if (size_in_mb >= supported_max_size_in_mb)
    size_in_mb = supported_max_size_in_mb - 1;
  if (size_in_mb > observed_max_size_in_mb) {
    // Send a UseCounter only when we see the highest memory usage
    // we've ever seen.
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, commited_size_histogram,
        ("BlinkGC.CommittedSize", supported_max_size_in_mb));
    commited_size_histogram.Count(size_in_mb);
    observed_max_size_in_mb = size_in_mb;
  }
}

void ThreadHeap::ReportMemoryUsageForTracing() {
#if PRINT_HEAP_STATS
// dataLogF("allocatedSpace=%ldMB, allocatedObjectSize=%ldMB, "
//          "markedObjectSize=%ldMB, partitionAllocSize=%ldMB, "
//          "wrapperCount=%ld, collectedWrapperCount=%ld\n",
//          ThreadHeap::allocatedSpace() / 1024 / 1024,
//          ThreadHeap::allocatedObjectSize() / 1024 / 1024,
//          ThreadHeap::markedObjectSize() / 1024 / 1024,
//          WTF::Partitions::totalSizeOfCommittedPages() / 1024 / 1024,
//          ThreadHeap::wrapperCount(), ThreadHeap::collectedWrapperCount());
#endif

  bool gc_tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                                     &gc_tracing_enabled);
  if (!gc_tracing_enabled)
    return;

  ThreadHeap& heap = ThreadState::Current()->Heap();
  // These values are divided by 1024 to avoid overflow in practical cases
  // (TRACE_COUNTER values are 32-bit ints).
  // They are capped to INT_MAX just in case.
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::allocatedObjectSizeKB",
                 std::min(heap.HeapStats().AllocatedObjectSize() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::markedObjectSizeKB",
                 std::min(heap.HeapStats().MarkedObjectSize() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"),
      "ThreadHeap::markedObjectSizeAtLastCompleteSweepKB",
      std::min(heap.HeapStats().MarkedObjectSizeAtLastCompleteSweep() / 1024,
               static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::allocatedSpaceKB",
                 std::min(heap.HeapStats().AllocatedSpace() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::objectSizeAtLastGCKB",
                 std::min(heap.HeapStats().ObjectSizeAtLastGC() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::wrapperCount",
      std::min(heap.HeapStats().WrapperCount(), static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::wrapperCountAtLastGC",
                 std::min(heap.HeapStats().WrapperCountAtLastGC(),
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::collectedWrapperCount",
                 std::min(heap.HeapStats().CollectedWrapperCount(),
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::partitionAllocSizeAtLastGCKB",
                 std::min(heap.HeapStats().PartitionAllocSizeAtLastGC() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "Partitions::totalSizeOfCommittedPagesKB",
                 std::min(WTF::Partitions::TotalSizeOfCommittedPages() / 1024,
                          static_cast<size_t>(INT_MAX)));
}

size_t ThreadHeap::ObjectPayloadSizeForTesting() {
  size_t object_payload_size = 0;
  thread_state_->SetGCState(ThreadState::kGCRunning);
  thread_state_->Heap().MakeConsistentForGC();
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    object_payload_size += arenas_[i]->ObjectPayloadSizeForTesting();
  thread_state_->SetGCState(ThreadState::kSweeping);
  thread_state_->SetGCState(ThreadState::kNoGCScheduled);
  return object_payload_size;
}

void ThreadHeap::ShouldFlushHeapDoesNotContainCache() {
  should_flush_heap_does_not_contain_cache_ = true;
}

void ThreadHeap::FlushHeapDoesNotContainCacheIfNeeded() {
  if (should_flush_heap_does_not_contain_cache_) {
    FlushHeapDoesNotContainCache();
    should_flush_heap_does_not_contain_cache_ = false;
  }
}

bool ThreadHeap::IsAddressInHeapDoesNotContainCache(Address address) {
  // If the cache has been marked as invalidated, it's cleared prior
  // to performing the next GC. Hence, consider the cache as being
  // effectively empty.
  if (should_flush_heap_does_not_contain_cache_)
    return false;
  return heap_does_not_contain_cache_->Lookup(address);
}

void ThreadHeap::VisitPersistentRoots(Visitor* visitor) {
  DCHECK(thread_state_->IsInGC());
  TRACE_EVENT0("blink_gc", "ThreadHeap::visitPersistentRoots");
  ProcessHeap::GetCrossThreadPersistentRegion().TracePersistentNodes(visitor);

  thread_state_->VisitPersistents(visitor);
}

void ThreadHeap::VisitStackRoots(Visitor* visitor) {
  DCHECK(thread_state_->IsInGC());
  TRACE_EVENT0("blink_gc", "ThreadHeap::visitStackRoots");
  thread_state_->VisitStack(visitor);
}

BasePage* ThreadHeap::LookupPageForAddress(Address address) {
  DCHECK(thread_state_->IsInGC());
  if (PageMemoryRegion* region = region_tree_->Lookup(address)) {
    return region->PageFromAddress(address);
  }
  return nullptr;
}

void ThreadHeap::ResetHeapCounters() {
  DCHECK(thread_state_->IsInGC());

  ThreadHeap::ReportMemoryUsageForTracing();

  ProcessHeap::DecreaseTotalAllocatedObjectSize(stats_.AllocatedObjectSize());
  ProcessHeap::DecreaseTotalMarkedObjectSize(stats_.MarkedObjectSize());

  stats_.Reset();
}

void ThreadHeap::MakeConsistentForGC() {
  DCHECK(thread_state_->IsInGC());
  TRACE_EVENT0("blink_gc", "ThreadHeap::MakeConsistentForGC");
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->MakeConsistentForGC();
}

void ThreadHeap::MakeConsistentForMutator() {
  DCHECK(thread_state_->IsInGC());
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->MakeConsistentForMutator();
}

void ThreadHeap::Compact() {
  if (!Compaction()->IsCompacting())
    return;

  // Compaction is done eagerly and before the mutator threads get
  // to run again. Doing it lazily is problematic, as the mutator's
  // references to live objects could suddenly be invalidated by
  // compaction of a page/heap. We do know all the references to
  // the relocating objects just after marking, but won't later.
  // (e.g., stack references could have been created, new objects
  // created which refer to old collection objects, and so on.)

  // Compact the hash table backing store arena first, it usually has
  // higher fragmentation and is larger.
  //
  // TODO: implement bail out wrt any overall deadline, not compacting
  // the remaining arenas if the time budget has been exceeded.
  Compaction()->StartThreadCompaction();
  for (int i = BlinkGC::kHashTableArenaIndex; i >= BlinkGC::kVector1ArenaIndex;
       --i)
    static_cast<NormalPageArena*>(arenas_[i])->SweepAndCompact();
  Compaction()->FinishThreadCompaction();
}

void ThreadHeap::PrepareForSweep() {
  DCHECK(thread_state_->IsInGC());
  DCHECK(thread_state_->CheckThread());
  for (int i = 0; i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->PrepareForSweep();
}

void ThreadHeap::RemoveAllPages() {
  DCHECK(thread_state_->CheckThread());
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->RemoveAllPages();
}

void ThreadHeap::CompleteSweep() {
  static_assert(BlinkGC::kEagerSweepArenaIndex == 0,
                "Eagerly swept arenas must be processed first.");
  for (int i = 0; i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->CompleteSweep();
}

void ThreadHeap::ClearArenaAges() {
  memset(arena_ages_, 0, sizeof(size_t) * BlinkGC::kNumberOfArenas);
  memset(likely_to_be_promptly_freed_.get(), 0,
         sizeof(int) * kLikelyToBePromptlyFreedArraySize);
  current_arena_ages_ = 0;
}

int ThreadHeap::ArenaIndexOfVectorArenaLeastRecentlyExpanded(
    int begin_arena_index,
    int end_arena_index) {
  size_t min_arena_age = arena_ages_[begin_arena_index];
  int arena_index_with_min_arena_age = begin_arena_index;
  for (int arena_index = begin_arena_index + 1; arena_index <= end_arena_index;
       arena_index++) {
    if (arena_ages_[arena_index] < min_arena_age) {
      min_arena_age = arena_ages_[arena_index];
      arena_index_with_min_arena_age = arena_index;
    }
  }
  DCHECK(IsVectorArenaIndex(arena_index_with_min_arena_age));
  return arena_index_with_min_arena_age;
}

BaseArena* ThreadHeap::ExpandedVectorBackingArena(size_t gc_info_index) {
  size_t entry_index = gc_info_index & kLikelyToBePromptlyFreedArrayMask;
  --likely_to_be_promptly_freed_[entry_index];
  int arena_index = vector_backing_arena_index_;
  arena_ages_[arena_index] = ++current_arena_ages_;
  vector_backing_arena_index_ = ArenaIndexOfVectorArenaLeastRecentlyExpanded(
      BlinkGC::kVector1ArenaIndex, BlinkGC::kVector4ArenaIndex);
  return arenas_[arena_index];
}

void ThreadHeap::AllocationPointAdjusted(int arena_index) {
  arena_ages_[arena_index] = ++current_arena_ages_;
  if (vector_backing_arena_index_ == arena_index) {
    vector_backing_arena_index_ = ArenaIndexOfVectorArenaLeastRecentlyExpanded(
        BlinkGC::kVector1ArenaIndex, BlinkGC::kVector4ArenaIndex);
  }
}

void ThreadHeap::PromptlyFreed(size_t gc_info_index) {
  DCHECK(thread_state_->CheckThread());
  size_t entry_index = gc_info_index & kLikelyToBePromptlyFreedArrayMask;
  // See the comment in vectorBackingArena() for why this is +3.
  likely_to_be_promptly_freed_[entry_index] += 3;
}

#if defined(ADDRESS_SANITIZER)
void ThreadHeap::PoisonAllHeaps() {
  CrossThreadPersistentRegion::LockScope persistent_lock(
      ProcessHeap::GetCrossThreadPersistentRegion());
  // Poisoning all unmarked objects in the other arenas.
  for (int i = 1; i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->PoisonArena();
  // CrossThreadPersistents in unmarked objects may be accessed from other
  // threads (e.g. in CrossThreadPersistentRegion::shouldTracePersistent) and
  // that would be fine.
  ProcessHeap::GetCrossThreadPersistentRegion()
      .UnpoisonCrossThreadPersistents();
}

void ThreadHeap::PoisonEagerArena() {
  CrossThreadPersistentRegion::LockScope persistent_lock(
      ProcessHeap::GetCrossThreadPersistentRegion());
  arenas_[BlinkGC::kEagerSweepArenaIndex]->PoisonArena();
  // CrossThreadPersistents in unmarked objects may be accessed from other
  // threads (e.g. in CrossThreadPersistentRegion::shouldTracePersistent) and
  // that would be fine.
  ProcessHeap::GetCrossThreadPersistentRegion()
      .UnpoisonCrossThreadPersistents();
}
#endif

#if DCHECK_IS_ON()
BasePage* ThreadHeap::FindPageFromAddress(Address address) {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i) {
    if (BasePage* page = arenas_[i]->FindPageFromAddress(address))
      return page;
  }
  return nullptr;
}
#endif

void ThreadHeap::TakeSnapshot(SnapshotType type) {
  DCHECK(thread_state_->IsInGC());

  // 0 is used as index for freelist entries. Objects are indexed 1 to
  // gcInfoIndex.
  ThreadState::GCSnapshotInfo info(GCInfoTable::GcInfoIndex() + 1);
  String thread_dump_name =
      String::Format("blink_gc/thread_%lu",
                     static_cast<unsigned long>(thread_state_->ThreadId()));
  const String heaps_dump_name = thread_dump_name + "/heaps";
  const String classes_dump_name = thread_dump_name + "/classes";

  int number_of_heaps_reported = 0;
#define SNAPSHOT_HEAP(ArenaType)                                          \
  {                                                                       \
    number_of_heaps_reported++;                                           \
    switch (type) {                                                       \
      case SnapshotType::kHeapSnapshot:                                   \
        arenas_[BlinkGC::k##ArenaType##ArenaIndex]->TakeSnapshot(         \
            heaps_dump_name + "/" #ArenaType, info);                      \
        break;                                                            \
      case SnapshotType::kFreelistSnapshot:                               \
        arenas_[BlinkGC::k##ArenaType##ArenaIndex]->TakeFreelistSnapshot( \
            heaps_dump_name + "/" #ArenaType);                            \
        break;                                                            \
      default:                                                            \
        NOTREACHED();                                                     \
    }                                                                     \
  }

  SNAPSHOT_HEAP(NormalPage1);
  SNAPSHOT_HEAP(NormalPage2);
  SNAPSHOT_HEAP(NormalPage3);
  SNAPSHOT_HEAP(NormalPage4);
  SNAPSHOT_HEAP(EagerSweep);
  SNAPSHOT_HEAP(Vector1);
  SNAPSHOT_HEAP(Vector2);
  SNAPSHOT_HEAP(Vector3);
  SNAPSHOT_HEAP(Vector4);
  SNAPSHOT_HEAP(InlineVector);
  SNAPSHOT_HEAP(HashTable);
  SNAPSHOT_HEAP(LargeObject);
  FOR_EACH_TYPED_ARENA(SNAPSHOT_HEAP);

  DCHECK_EQ(number_of_heaps_reported, BlinkGC::kNumberOfArenas);

#undef SNAPSHOT_HEAP

  if (type == SnapshotType::kFreelistSnapshot)
    return;

  size_t total_live_count = 0;
  size_t total_dead_count = 0;
  size_t total_live_size = 0;
  size_t total_dead_size = 0;
  for (size_t gc_info_index = 1; gc_info_index <= GCInfoTable::GcInfoIndex();
       ++gc_info_index) {
    total_live_count += info.live_count[gc_info_index];
    total_dead_count += info.dead_count[gc_info_index];
    total_live_size += info.live_size[gc_info_index];
    total_dead_size += info.dead_size[gc_info_index];
  }

  base::trace_event::MemoryAllocatorDump* thread_dump =
      BlinkGCMemoryDumpProvider::Instance()
          ->CreateMemoryAllocatorDumpForCurrentGC(thread_dump_name);
  thread_dump->AddScalar("live_count", "objects", total_live_count);
  thread_dump->AddScalar("dead_count", "objects", total_dead_count);
  thread_dump->AddScalar("live_size", "bytes", total_live_size);
  thread_dump->AddScalar("dead_size", "bytes", total_dead_size);

  base::trace_event::MemoryAllocatorDump* heaps_dump =
      BlinkGCMemoryDumpProvider::Instance()
          ->CreateMemoryAllocatorDumpForCurrentGC(heaps_dump_name);
  base::trace_event::MemoryAllocatorDump* classes_dump =
      BlinkGCMemoryDumpProvider::Instance()
          ->CreateMemoryAllocatorDumpForCurrentGC(classes_dump_name);
  BlinkGCMemoryDumpProvider::Instance()
      ->CurrentProcessMemoryDump()
      ->AddOwnershipEdge(classes_dump->guid(), heaps_dump->guid());
}

bool ThreadHeap::AdvanceLazySweep(double deadline_seconds) {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; i++) {
    // lazySweepWithDeadline() won't check the deadline until it sweeps
    // 10 pages. So we give a small slack for safety.
    double slack = 0.001;
    double remaining_budget =
        deadline_seconds - slack - MonotonicallyIncreasingTime();
    if (remaining_budget <= 0 ||
        !arenas_[i]->LazySweepWithDeadline(deadline_seconds)) {
      return false;
    }
  }
  return true;
}

void ThreadHeap::EnableIncrementalMarkingBarrier() {
  thread_state_->SetIncrementalMarking(true);
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->EnableIncrementalMarkingBarrier();
}

void ThreadHeap::DisableIncrementalMarkingBarrier() {
  thread_state_->SetIncrementalMarking(false);
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->DisableIncrementalMarkingBarrier();
}

ThreadHeap* ThreadHeap::main_thread_heap_ = nullptr;

}  // namespace blink

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

#include "third_party/blink/renderer/platform/heap/heap.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/address_cache.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/page_memory.h"
#include "third_party/blink/renderer/platform/heap/page_pool.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"

namespace blink {

HeapAllocHooks::AllocationHook* HeapAllocHooks::allocation_hook_ = nullptr;
HeapAllocHooks::FreeHook* HeapAllocHooks::free_hook_ = nullptr;

class ProcessHeapReporter final : public ThreadHeapStatsObserver {
 public:
  void IncreaseAllocatedSpace(size_t bytes) final {
    ProcessHeap::IncreaseTotalAllocatedSpace(bytes);
  }

  void DecreaseAllocatedSpace(size_t bytes) final {
    ProcessHeap::DecreaseTotalAllocatedSpace(bytes);
  }

  void ResetAllocatedObjectSize(size_t bytes) final {
    ProcessHeap::DecreaseTotalAllocatedObjectSize(prev_incremented_);
    ProcessHeap::IncreaseTotalAllocatedObjectSize(bytes);
    prev_incremented_ = bytes;
  }

  void IncreaseAllocatedObjectSize(size_t bytes) final {
    ProcessHeap::IncreaseTotalAllocatedObjectSize(bytes);
    prev_incremented_ += bytes;
  }

  void DecreaseAllocatedObjectSize(size_t bytes) final {
    ProcessHeap::DecreaseTotalAllocatedObjectSize(bytes);
    prev_incremented_ -= bytes;
  }

 private:
  size_t prev_incremented_ = 0;
};

ThreadHeap::ThreadHeap(ThreadState* thread_state)
    : thread_state_(thread_state),
      heap_stats_collector_(std::make_unique<ThreadHeapStatsCollector>()),
      region_tree_(std::make_unique<RegionTree>()),
      address_cache_(std::make_unique<AddressCache>()),
      free_page_pool_(std::make_unique<PagePool>()),
      process_heap_reporter_(std::make_unique<ProcessHeapReporter>()),
      marking_worklist_(nullptr),
      not_fully_constructed_worklist_(nullptr),
      weak_callback_worklist_(nullptr),
      movable_reference_worklist_(nullptr),
      weak_table_worklist_(nullptr),
      backing_store_callback_worklist_(nullptr),
      v8_references_worklist_(nullptr),
      vector_backing_arena_index_(BlinkGC::kVector1ArenaIndex),
      current_arena_ages_(0) {
  if (ThreadState::Current()->IsMainThread())
    main_thread_heap_ = this;

  for (int arena_index = 0; arena_index < BlinkGC::kLargeObjectArenaIndex;
       arena_index++)
    arenas_[arena_index] = new NormalPageArena(thread_state_, arena_index);
  arenas_[BlinkGC::kLargeObjectArenaIndex] =
      new LargeObjectArena(thread_state_, BlinkGC::kLargeObjectArenaIndex);

  likely_to_be_promptly_freed_ =
      std::make_unique<int[]>(kLikelyToBePromptlyFreedArraySize);
  ClearArenaAges();

  stats_collector()->RegisterObserver(process_heap_reporter_.get());
}

ThreadHeap::~ThreadHeap() {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    delete arenas_[i];
}

Address ThreadHeap::CheckAndMarkPointer(MarkingVisitor* visitor,
                                        Address address) {
  DCHECK(thread_state_->InAtomicMarkingPause());

#if !DCHECK_IS_ON()
  if (address_cache_->Lookup(address))
    return nullptr;
#endif

  if (BasePage* page = LookupPageForAddress(address)) {
#if DCHECK_IS_ON()
    DCHECK(page->Contains(address));
#endif
    DCHECK(!address_cache_->Lookup(address));
    DCHECK(&visitor->Heap() == &page->Arena()->GetThreadState()->Heap());
    visitor->ConservativelyMarkAddress(page, address);
    return address;
  }

#if !DCHECK_IS_ON()
  address_cache_->AddEntry(address);
#else
  if (!address_cache_->Lookup(address))
    address_cache_->AddEntry(address);
#endif
  return nullptr;
}

void ThreadHeap::SetupWorklists() {
  marking_worklist_.reset(new MarkingWorklist());
  not_fully_constructed_worklist_.reset(new NotFullyConstructedWorklist());
  previously_not_fully_constructed_worklist_.reset(
      new NotFullyConstructedWorklist());
  weak_callback_worklist_.reset(new WeakCallbackWorklist());
  movable_reference_worklist_.reset(new MovableReferenceWorklist());
  weak_table_worklist_.reset(new WeakTableWorklist);
  backing_store_callback_worklist_.reset(new BackingStoreCallbackWorklist());
  v8_references_worklist_.reset(new V8ReferencesWorklist());
  DCHECK(ephemeron_callbacks_.IsEmpty());
}

void ThreadHeap::DestroyMarkingWorklists(BlinkGC::StackState stack_state) {
  marking_worklist_.reset(nullptr);
  previously_not_fully_constructed_worklist_.reset(nullptr);
  weak_callback_worklist_.reset(nullptr);
  weak_table_worklist_.reset();
  v8_references_worklist_.reset();
  ephemeron_callbacks_.clear();

  // The fixed point iteration may have found not-fully-constructed objects.
  // Such objects should have already been found through the stack scan though
  // and should thus already be marked.
  //
  // Possible reasons for encountering unmarked objects here:
  // - Object is not allocated through MakeGarbageCollected.
  // - Type is missing a USING_GARBAGE_COLLECTED_MIXIN annotation which means
  //   that the GC will always find pointers as in construction.
  // - Broken stack (roots) scanning.
  if (!not_fully_constructed_worklist_->IsGlobalEmpty()) {
#if DCHECK_IS_ON()
    const bool conservative_gc =
        BlinkGC::StackState::kHeapPointersOnStack == stack_state;
    NotFullyConstructedItem item;
    while (not_fully_constructed_worklist_->Pop(WorklistTaskId::MainThread,
                                                &item)) {
      HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress(
          reinterpret_cast<Address>(const_cast<void*>(item)));
      DCHECK(conservative_gc && header->IsMarked())
          << " conservative: " << (conservative_gc ? "yes" : "no")
          << " type: " << header->Name();
    }
#else
    not_fully_constructed_worklist_->Clear();
#endif
  }
  not_fully_constructed_worklist_.reset(nullptr);
}

void ThreadHeap::DestroyCompactionWorklists() {
  movable_reference_worklist_.reset();
  backing_store_callback_worklist_.reset();
}

HeapCompact* ThreadHeap::Compaction() {
  if (!compaction_)
    compaction_ = std::make_unique<HeapCompact>(this);
  return compaction_.get();
}

bool ThreadHeap::ShouldRegisterMovingAddress(Address address) {
  return Compaction()->ShouldRegisterMovingAddress(address);
}

void ThreadHeap::FlushNotFullyConstructedObjects() {
  if (!not_fully_constructed_worklist_->IsGlobalEmpty()) {
    not_fully_constructed_worklist_->FlushToGlobal(WorklistTaskId::MainThread);
    previously_not_fully_constructed_worklist_->MergeGlobalPool(
        not_fully_constructed_worklist_.get());
  }
  DCHECK(not_fully_constructed_worklist_->IsGlobalEmpty());
}

void ThreadHeap::MarkNotFullyConstructedObjects(MarkingVisitor* visitor) {
  DCHECK(!thread_state_->IsIncrementalMarking());
  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(),
      ThreadHeapStatsCollector::kMarkNotFullyConstructedObjects);

  DCHECK_EQ(WorklistTaskId::MainThread, visitor->task_id());
  NotFullyConstructedItem item;
  while (
      not_fully_constructed_worklist_->Pop(WorklistTaskId::MainThread, &item)) {
    BasePage* const page = PageFromObject(item);
    visitor->ConservativelyMarkAddress(page, reinterpret_cast<Address>(item));
  }
}

void ThreadHeap::InvokeEphemeronCallbacks(MarkingVisitor* visitor) {
  // Mark any strong pointers that have now become reachable in ephemeron maps.
  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(),
      ThreadHeapStatsCollector::kMarkInvokeEphemeronCallbacks);

  // MarkingVisitor records callbacks for weak tables in a worklist.
  // ThreadHeap reads from the worklist before invoking ephemeron callbacks.
  // We need to flush the callback worklist so ThreadHeap can access it.
  //
  // TODO(omerkatz): In concurrent marking, this should flush all visitors.
  if (thread_state_->in_atomic_pause())
    visitor->FlushWeakTableCallbacks();

  // We first reiterate over known callbacks from previous iterations.
  for (auto& tuple : ephemeron_callbacks_)
    tuple.value(visitor, tuple.key);

  DCHECK_EQ(WorklistTaskId::MainThread, visitor->task_id());

  // Then we iterate over the new callbacks found by the marking visitor.
  while (!weak_table_worklist_->IsGlobalEmpty()) {
    // Read ephemeron callbacks from worklist to ephemeron_callbacks_ hashmap.
    WeakTableWorklist::View ephemerons_worklist(weak_table_worklist_.get(),
                                                WorklistTaskId::MainThread);
    WeakTableItem item;
    while (ephemerons_worklist.Pop(&item)) {
      auto result = ephemeron_callbacks_.insert(item.object, item.callback);
      DCHECK(result.is_new_entry ||
             result.stored_value->value == item.callback);
      if (result.is_new_entry) {
        item.callback(visitor, item.object);
      }
    }
  }
}

namespace {

template <typename Worklist, typename Callback>
bool DrainWorklistWithDeadline(base::TimeTicks deadline,
                               Worklist* worklist,
                               Callback callback,
                               int task_id) {
  const size_t kDeadlineCheckInterval = 2500;

  size_t processed_callback_count = 0;
  typename Worklist::EntryType item;
  while (worklist->Pop(task_id, &item)) {
    callback(item);
    processed_callback_count++;
    if (++processed_callback_count == kDeadlineCheckInterval) {
      if (deadline <= base::TimeTicks::Now()) {
        return false;
      }
      processed_callback_count = 0;
    }
  }
  return true;
}

}  // namespace

bool ThreadHeap::AdvanceMarking(MarkingVisitor* visitor,
                                base::TimeTicks deadline) {
  DCHECK_EQ(WorklistTaskId::MainThread, visitor->task_id());

  FlushV8References(visitor);

  bool finished;
  // Ephemeron fixed point loop.
  do {
    {
      // Iteratively mark all objects that are reachable from the objects
      // currently pushed onto the marking worklist.
      ThreadHeapStatsCollector::Scope stats_scope(
          stats_collector(), ThreadHeapStatsCollector::kMarkProcessWorklist);

      finished = DrainWorklistWithDeadline(
          deadline, marking_worklist_.get(),
          [visitor](const MarkingItem& item) {
            DCHECK(!HeapObjectHeader::FromPayload(item.object)
                        ->IsInConstruction());
            item.callback(visitor, item.object);
          },
          WorklistTaskId::MainThread);
      if (!finished)
        return false;

      // Iteratively mark all objects that were previously discovered while
      // being in construction. The objects can be processed incrementally once
      // a safepoint was reached.
      finished = DrainWorklistWithDeadline(
          deadline, previously_not_fully_constructed_worklist_.get(),
          [visitor](const NotFullyConstructedItem& item) {
            visitor->DynamicallyMarkAddress(reinterpret_cast<Address>(item));
          },
          WorklistTaskId::MainThread);
      if (!finished)
        return false;
    }

    InvokeEphemeronCallbacks(visitor);

    // Rerun loop if ephemeron processing queued more objects for tracing.
  } while (!marking_worklist_->IsGlobalEmpty());

  // In case we reached here, we might not enter these method again for a
  // while. Flush all V8 references before returning so we don't get left with
  /// unflushed references.
  FlushV8References(visitor);

  return true;
}

void ThreadHeap::WeakProcessing(MarkingVisitor* visitor) {
  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(), ThreadHeapStatsCollector::kMarkWeakProcessing);

  // Weak processing may access unmarked objects but are forbidden from
  // resurrecting them or allocating new ones.
  ThreadState::NoAllocationScope allocation_forbidden(ThreadState::Current());

  DCHECK_EQ(WorklistTaskId::MainThread, visitor->task_id());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  CustomCallbackItem item;
  while (weak_callback_worklist_->Pop(WorklistTaskId::MainThread, &item)) {
    item.callback(visitor, item.object);
  }
  // Weak callbacks should not add any new objects for marking.
  DCHECK(marking_worklist_->IsGlobalEmpty());
}

void ThreadHeap::VerifyMarking() {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i) {
    arenas_[i]->VerifyMarking();
  }
}

size_t ThreadHeap::ObjectPayloadSizeForTesting() {
  ThreadState::AtomicPauseScope atomic_pause_scope(thread_state_);
  ScriptForbiddenScope script_forbidden_scope;
  size_t object_payload_size = 0;
  thread_state_->SetGCPhase(ThreadState::GCPhase::kMarking);
  thread_state_->Heap().MakeConsistentForGC();
  thread_state_->Heap().PrepareForSweep();
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    object_payload_size += arenas_[i]->ObjectPayloadSizeForTesting();
  MakeConsistentForMutator();
  thread_state_->SetGCPhase(ThreadState::GCPhase::kSweeping);
  thread_state_->SetGCPhase(ThreadState::GCPhase::kNone);
  return object_payload_size;
}

void ThreadHeap::ResetAllocationPointForTesting() {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->ResetAllocationPoint();
}

BasePage* ThreadHeap::LookupPageForAddress(Address address) {
  if (PageMemoryRegion* region = region_tree_->Lookup(address)) {
    return region->PageFromAddress(address);
  }
  return nullptr;
}

void ThreadHeap::MakeConsistentForGC() {
  DCHECK(thread_state_->InAtomicMarkingPause());
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->MakeConsistentForGC();
}

void ThreadHeap::MakeConsistentForMutator() {
  DCHECK(thread_state_->InAtomicMarkingPause());
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i)
    arenas_[i]->MakeConsistentForMutator();
}

void ThreadHeap::Compact() {
  if (!Compaction()->IsCompacting())
    return;

  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(), ThreadHeapStatsCollector::kAtomicPauseCompaction);
  // Compaction is done eagerly and before the mutator threads get
  // to run again. Doing it lazily is problematic, as the mutator's
  // references to live objects could suddenly be invalidated by
  // compaction of a page/heap. We do know all the references to
  // the relocating objects just after marking, but won't later.
  // (e.g., stack references could have been created, new objects
  // created which refer to old collection objects, and so on.)

  // Compact the hash table backing store arena first, it usually has
  // higher fragmentation and is larger.
  for (int i = BlinkGC::kHashTableArenaIndex; i >= BlinkGC::kVector1ArenaIndex;
       --i)
    static_cast<NormalPageArena*>(arenas_[i])->SweepAndCompact();
  Compaction()->Finish();
}

void ThreadHeap::PrepareForSweep() {
  DCHECK(thread_state_->InAtomicMarkingPause());
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
  for (int i = 0; i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->CompleteSweep();
}

void ThreadHeap::InvokeFinalizersOnSweptPages() {
  for (size_t i = BlinkGC::kNormalPage1ArenaIndex;
       i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->InvokeFinalizersOnSweptPages();
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

BaseArena* ThreadHeap::ExpandedVectorBackingArena(uint32_t gc_info_index) {
  uint32_t entry_index = gc_info_index & kLikelyToBePromptlyFreedArrayMask;
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

void ThreadHeap::PromptlyFreed(uint32_t gc_info_index) {
  DCHECK(thread_state_->CheckThread());
  uint32_t entry_index = gc_info_index & kLikelyToBePromptlyFreedArrayMask;
  // See the comment in vectorBackingArena() for why this is +3.
  likely_to_be_promptly_freed_[entry_index] += 3;
}

#if defined(ADDRESS_SANITIZER)
void ThreadHeap::PoisonUnmarkedObjects() {
  // Poisoning all unmarked objects in the other arenas.
  for (int i = 1; i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->PoisonUnmarkedObjects();
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
  DCHECK(thread_state_->InAtomicMarkingPause());

  // 0 is used as index for freelist entries. Objects are indexed 1 to
  // gcInfoIndex.
  ThreadState::GCSnapshotInfo info(GCInfoTable::Get().GcInfoIndex() + 1);
  String thread_dump_name =
      String("blink_gc/thread_") + String::Number(thread_state_->ThreadId());
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
  for (uint32_t gc_info_index = 1;
       gc_info_index <= GCInfoTable::Get().GcInfoIndex(); ++gc_info_index) {
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

bool ThreadHeap::AdvanceLazySweep(base::TimeTicks deadline) {
  static const base::TimeDelta slack = base::TimeDelta::FromSecondsD(0.001);
  for (int i = 0; i < BlinkGC::kNumberOfArenas; i++) {
    // lazySweepWithDeadline() won't check the deadline until it sweeps
    // 10 pages. So we give a small slack for safety.
    const base::TimeDelta remaining_budget =
        deadline - slack - base::TimeTicks::Now();
    if (remaining_budget <= base::TimeDelta() ||
        !arenas_[i]->LazySweepWithDeadline(deadline)) {
      return false;
    }
  }
  return true;
}

void ThreadHeap::ConcurrentSweep() {
  // Concurrent sweep simply sweeps pages not calling finalizers.
  for (size_t i = BlinkGC::kNormalPage1ArenaIndex;
       i < BlinkGC::kNumberOfArenas; i++) {
    arenas_[i]->SweepOnConcurrentThread();
  }
}

// TODO(omerkatz): Temporary solution until concurrent marking is ready. see
// https://crrev.com/c/1730054 for details. Eventually this will be removed.
void ThreadHeap::FlushV8References(MarkingVisitor* visitor) {
  if (!thread_state_->IsUnifiedGCMarkingInProgress())
    return;

  UnifiedHeapMarkingVisitor* unified_visitor =
      reinterpret_cast<UnifiedHeapMarkingVisitor*>(visitor);

  DCHECK((unified_visitor->task_id() != WorklistTaskId::MainThread) ||
         v8_references_worklist_->IsGlobalEmpty());

  // TODO(omerkatz): In concurrent marking, this should flush all visitors
  unified_visitor->FlushV8References();

  DCHECK_EQ(WorklistTaskId::MainThread, visitor->task_id());

  V8ReferencesWorklist::View v8_references(v8_references_worklist_.get(),
                                           WorklistTaskId::MainThread);
  V8Reference reference;
  v8::EmbedderHeapTracer* controller =
      reinterpret_cast<v8::EmbedderHeapTracer*>(
          thread_state_->unified_heap_controller());
  while (v8_references.Pop(&reference)) {
    controller->RegisterEmbedderReference(reference->Get());
  }
}

ThreadHeap* ThreadHeap::main_thread_heap_ = nullptr;

}  // namespace blink

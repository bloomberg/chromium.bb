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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/page_bloom_filter.h"
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
      page_bloom_filter_(std::make_unique<PageBloomFilter>()),
      free_page_pool_(std::make_unique<PagePool>()),
      process_heap_reporter_(std::make_unique<ProcessHeapReporter>()) {
  if (ThreadState::Current()->IsMainThread())
    main_thread_heap_ = this;

  for (int arena_index = 0; arena_index < BlinkGC::kLargeObjectArenaIndex;
       arena_index++) {
    arenas_[arena_index] = new NormalPageArena(thread_state_, arena_index);
  }
  arenas_[BlinkGC::kLargeObjectArenaIndex] =
      new LargeObjectArena(thread_state_, BlinkGC::kLargeObjectArenaIndex);

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
  if (!page_bloom_filter_->MayContain(address)) {
    return nullptr;
  }
#endif

  if (BasePage* page = LookupPageForAddress(address)) {
#if DCHECK_IS_ON()
    DCHECK(page->Contains(address));
#endif
    DCHECK(page_bloom_filter_->MayContain(address));
    DCHECK(&visitor->Heap() == &page->Arena()->GetThreadState()->Heap());
    visitor->ConservativelyMarkAddress(page, address);
    return address;
  }

  return nullptr;
}

void ThreadHeap::VisitRememberedSets(MarkingVisitor* visitor) {
  static_assert(BlinkGC::kLargeObjectArenaIndex + 1 == BlinkGC::kNumberOfArenas,
                "LargeObject arena must be the last one.");
  const auto visit_header = [visitor](HeapObjectHeader* header) {
    // Process only old objects.
    if (header->IsOld<HeapObjectHeader::AccessMode::kNonAtomic>()) {
      // The design of young generation requires collections to be executed at
      // the top level (with the guarantee that no objects are currently being
      // in construction). This can be ensured by running young GCs from safe
      // points or by reintroducing nested allocation scopes that avoid
      // finalization.
      DCHECK(header->IsMarked());
      DCHECK(!MarkingVisitor::IsInConstruction(header));
      const GCInfo& gc_info = GCInfo::From(header->GcInfoIndex());
      gc_info.trace(visitor, header->Payload());
    }
  };
  for (size_t i = 0; i < BlinkGC::kLargeObjectArenaIndex; ++i) {
    static_cast<NormalPageArena*>(arenas_[i])
        ->IterateAndClearCardTables(visit_header);
  }
  static_cast<LargeObjectArena*>(arenas_[BlinkGC::kLargeObjectArenaIndex])
      ->IterateAndClearRememberedPages(visit_header);
}

void ThreadHeap::SetupWorklists(bool should_initialize_compaction_worklists) {
  marking_worklist_.reset(new MarkingWorklist());
  write_barrier_worklist_.reset(new WriteBarrierWorklist());
  not_fully_constructed_worklist_.reset(new NotFullyConstructedWorklist());
  previously_not_fully_constructed_worklist_.reset(
      new NotFullyConstructedWorklist());
  weak_callback_worklist_.reset(new WeakCallbackWorklist());
  weak_table_worklist_.reset(new WeakTableWorklist);
  v8_references_worklist_.reset(new V8ReferencesWorklist());
  not_safe_to_concurrently_trace_worklist_.reset(
      new NotSafeToConcurrentlyTraceWorklist());
  DCHECK(ephemeron_callbacks_.IsEmpty());
  if (should_initialize_compaction_worklists) {
    movable_reference_worklist_.reset(new MovableReferenceWorklist());
    backing_store_callback_worklist_.reset(new BackingStoreCallbackWorklist());
  }
}

void ThreadHeap::DestroyMarkingWorklists(BlinkGC::StackState stack_state) {
  marking_worklist_.reset(nullptr);
  write_barrier_worklist_.reset(nullptr);
  previously_not_fully_constructed_worklist_.reset(nullptr);
  weak_callback_worklist_.reset(nullptr);
  weak_table_worklist_.reset();
  v8_references_worklist_.reset();
  not_safe_to_concurrently_trace_worklist_.reset();
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
    while (not_fully_constructed_worklist_->Pop(WorklistTaskId::MutatorThread,
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

bool ThreadHeap::ShouldRegisterMovingAddress() {
  return Compaction()->ShouldRegisterMovingAddress();
}

void ThreadHeap::FlushNotFullyConstructedObjects() {
  NotFullyConstructedWorklist::View view(not_fully_constructed_worklist_.get(),
                                         WorklistTaskId::MutatorThread);
  if (!view.IsLocalViewEmpty()) {
    view.FlushToGlobal();
    previously_not_fully_constructed_worklist_->MergeGlobalPool(
        not_fully_constructed_worklist_.get());
  }
  DCHECK(view.IsLocalViewEmpty());
}

void ThreadHeap::MarkNotFullyConstructedObjects(MarkingVisitor* visitor) {
  DCHECK(!thread_state_->IsIncrementalMarking());
  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(),
      ThreadHeapStatsCollector::kMarkNotFullyConstructedObjects);

  DCHECK_EQ(WorklistTaskId::MutatorThread, visitor->task_id());
  NotFullyConstructedItem item;
  while (not_fully_constructed_worklist_->Pop(WorklistTaskId::MutatorThread,
                                              &item)) {
    BasePage* const page = PageFromObject(item);
    visitor->ConservativelyMarkAddress(page,
                                       reinterpret_cast<ConstAddress>(item));
  }
}

namespace {

template <typename Worklist, typename Callback>
bool DrainWorklistWithDeadline(base::TimeTicks deadline,
                               Worklist* worklist,
                               Callback callback,
                               int task_id) {
  const size_t kDeadlineCheckInterval = 1250;

  size_t processed_callback_count = 0;
  typename Worklist::EntryType item;
  while (worklist->Pop(task_id, &item)) {
    callback(item);
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

bool ThreadHeap::InvokeEphemeronCallbacks(MarkingVisitor* visitor,
                                          base::TimeTicks deadline) {
  // Mark any strong pointers that have now become reachable in ephemeron maps.
  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(),
      ThreadHeapStatsCollector::kMarkInvokeEphemeronCallbacks);

  // We first reiterate over known callbacks from previous iterations.
  constexpr size_t kDeadlineCheckInterval = 250;
  size_t processed_callback_count = 0;
  for (auto& tuple : ephemeron_callbacks_) {
    tuple.value(visitor, tuple.key);
    if (++processed_callback_count == kDeadlineCheckInterval) {
      if (deadline <= base::TimeTicks::Now()) {
        return false;
      }
      processed_callback_count = 0;
    }
  }

  DCHECK_EQ(WorklistTaskId::MutatorThread, visitor->task_id());

  // Then we iterate over the new callbacks found by the marking visitor.
  // Callbacks found by the concurrent marking will be flushed eventually
  // and then invoked by the mutator thread (in the atomic pause at latest).
  return DrainWorklistWithDeadline(
      deadline, weak_table_worklist_.get(),
      [this, visitor](const WeakTableItem& item) {
        auto result = ephemeron_callbacks_.insert(item.base_object_payload,
                                                  item.callback);
        DCHECK(result.is_new_entry ||
               result.stored_value->value == item.callback);
        if (result.is_new_entry) {
          item.callback(visitor, item.base_object_payload);
        }
      },
      WorklistTaskId::MutatorThread);
}

bool ThreadHeap::AdvanceMarking(MarkingVisitor* visitor,
                                base::TimeTicks deadline) {
  DCHECK_EQ(WorklistTaskId::MutatorThread, visitor->task_id());

  bool finished;
  // Ephemeron fixed point loop.
  do {
    {
      // Iteratively mark all objects that are reachable from the objects
      // currently pushed onto the marking worklist.
      ThreadHeapStatsCollector::Scope stats_scope(
          stats_collector(), ThreadHeapStatsCollector::kMarkProcessWorklist);

      // Start with mutator-thread-only worklists (not fully constructed).
      // If time runs out, concurrent markers can take care of the rest.

      // Convert |previously_not_fully_constructed_worklist_| to
      // |marking_worklist_|. This merely re-adds items with the proper
      // callbacks.
      finished = DrainWorklistWithDeadline(
          deadline, previously_not_fully_constructed_worklist_.get(),
          [visitor](NotFullyConstructedItem& item) {
            visitor->DynamicallyMarkAddress(
                reinterpret_cast<ConstAddress>(item));
          },
          WorklistTaskId::MutatorThread);
      if (!finished)
        break;

      {
        ThreadHeapStatsCollector::EnabledScope bailout_scope(
            stats_collector(), ThreadHeapStatsCollector::kMarkBailOutObjects);
        finished = DrainWorklistWithDeadline(
            deadline, not_safe_to_concurrently_trace_worklist_.get(),
            [visitor](const MarkingItem& item) {
              item.callback(visitor, item.base_object_payload);
            },
            WorklistTaskId::MutatorThread);
        if (!finished)
          break;
      }

      finished = FlushV8References(deadline);
      if (!finished)
        break;

      finished = DrainWorklistWithDeadline(
          deadline, marking_worklist_.get(),
          [visitor](const MarkingItem& item) {
            HeapObjectHeader* header =
                HeapObjectHeader::FromPayload(item.base_object_payload);
            DCHECK(!MarkingVisitor::IsInConstruction(header));
            item.callback(visitor, item.base_object_payload);
            visitor->AccountMarkedBytes(header);
          },
          WorklistTaskId::MutatorThread);
      if (!finished)
        break;

      finished = DrainWorklistWithDeadline(
          deadline, write_barrier_worklist_.get(),
          [visitor](HeapObjectHeader* header) {
            DCHECK(!MarkingVisitor::IsInConstruction(header));
            GCInfo::From(header->GcInfoIndex())
                .trace(visitor, header->Payload());
            visitor->AccountMarkedBytes(header);
          },
          WorklistTaskId::MutatorThread);
      if (!finished)
        break;
    }

    finished = InvokeEphemeronCallbacks(visitor, deadline);
    if (!finished)
      break;

    // Rerun loop if ephemeron processing queued more objects for tracing.
  } while (!marking_worklist_->IsLocalViewEmpty(WorklistTaskId::MutatorThread));

  return finished;
}

bool ThreadHeap::HasWorkForConcurrentMarking() const {
  return !marking_worklist_->IsGlobalPoolEmpty() ||
         !write_barrier_worklist_->IsGlobalPoolEmpty();
}

bool ThreadHeap::AdvanceConcurrentMarking(ConcurrentMarkingVisitor* visitor,
                                          base::TimeTicks deadline) {
  bool finished;
  do {
    // Iteratively mark all objects that are reachable from the objects
    // currently pushed onto the marking worklist.
    finished = DrainWorklistWithDeadline(
        deadline, marking_worklist_.get(),
        [visitor](const MarkingItem& item) {
          HeapObjectHeader* header =
              HeapObjectHeader::FromPayload(item.base_object_payload);
          PageFromObject(header)->SynchronizedLoad();
          DCHECK(!ConcurrentMarkingVisitor::IsInConstruction(header));
          item.callback(visitor, item.base_object_payload);
          visitor->AccountMarkedBytesSafe(header);
        },
        visitor->task_id());
    if (!finished)
      break;

    finished = DrainWorklistWithDeadline(
        deadline, write_barrier_worklist_.get(),
        [visitor](HeapObjectHeader* header) {
          PageFromObject(header)->SynchronizedLoad();
          DCHECK(!ConcurrentMarkingVisitor::IsInConstruction(header));
          GCInfo::From(header->GcInfoIndex()).trace(visitor, header->Payload());
          visitor->AccountMarkedBytesSafe(header);
        },
        visitor->task_id());
    if (!finished)
      break;
  } while (HasWorkForConcurrentMarking());

  return finished;
}

void ThreadHeap::WeakProcessing(MarkingVisitor* visitor) {
  ThreadHeapStatsCollector::Scope stats_scope(
      stats_collector(), ThreadHeapStatsCollector::kMarkWeakProcessing);

  // Weak processing may access unmarked objects but are forbidden from
  // resurrecting them or allocating new ones.
  ThreadState::NoAllocationScope allocation_forbidden(ThreadState::Current());

  DCHECK_EQ(WorklistTaskId::MutatorThread, visitor->task_id());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  CustomCallbackItem item;
  LivenessBroker broker = internal::LivenessBrokerFactory::Create();
  while (weak_callback_worklist_->Pop(WorklistTaskId::MutatorThread, &item)) {
    item.callback(broker, item.parameter);
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
  thread_state_->Heap().PrepareForSweep(BlinkGC::CollectionType::kMajor);
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

BasePage* ThreadHeap::LookupPageForAddress(ConstAddress address) {
  if (PageMemoryRegion* region = region_tree_->Lookup(address)) {
    return region->PageFromAddress(address);
  }
  return nullptr;
}

void ThreadHeap::MakeConsistentForGC() {
  DCHECK(thread_state_->InAtomicMarkingPause());
  for (BaseArena* arena : arenas_) {
    arena->MakeConsistentForGC();
  }
}

void ThreadHeap::MakeConsistentForMutator() {
  DCHECK(thread_state_->InAtomicMarkingPause());
  for (BaseArena* arena : arenas_) {
    arena->MakeConsistentForMutator();
  }
}

void ThreadHeap::Unmark() {
  DCHECK(thread_state_->InAtomicMarkingPause());
  for (BaseArena* arena : arenas_) {
    arena->Unmark();
  }
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
  for (int i = BlinkGC::kHashTableArenaIndex; i >= BlinkGC::kVectorArenaIndex;
       --i)
    static_cast<NormalPageArena*>(arenas_[i])->SweepAndCompact();
  Compaction()->Finish();
}

void ThreadHeap::PrepareForSweep(BlinkGC::CollectionType collection_type) {
  DCHECK(thread_state_->InAtomicMarkingPause());
  DCHECK(thread_state_->CheckThread());
  for (int i = 0; i < BlinkGC::kNumberOfArenas; i++)
    arenas_[i]->PrepareForSweep(collection_type);
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

void ThreadHeap::CollectStatistics(ThreadState::Statistics* stats) {
#define SNAPSHOT_ARENA(name)                                \
  arenas_[BlinkGC::k##name##ArenaIndex]->CollectStatistics( \
      BlinkGC::ToString(BlinkGC::k##name##ArenaIndex), stats);

  FOR_EACH_ARENA(SNAPSHOT_ARENA)
#undef SNAPSHOT_ARENA
}

bool ThreadHeap::AdvanceLazySweep(base::TimeTicks deadline) {
  static constexpr base::TimeDelta slack = base::TimeDelta::FromSecondsD(0.001);
  for (size_t i = 0; i < BlinkGC::kNumberOfArenas; i++) {
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

bool ThreadHeap::AdvanceConcurrentSweep(base::JobDelegate* job) {
  for (size_t i = 0; i < BlinkGC::kNumberOfArenas; i++) {
    while (!arenas_[i]->ConcurrentSweepOnePage()) {
      if (job->ShouldYield())
        return false;
    }
  }
  return true;
}

// TODO(omerkatz): Temporary solution until concurrent marking is ready. see
// https://crrev.com/c/1730054 for details. Eventually this will be removed.
bool ThreadHeap::FlushV8References(base::TimeTicks deadline) {
  if (!thread_state_->IsUnifiedGCMarkingInProgress())
    return true;

  DCHECK(base::FeatureList::IsEnabled(
             blink::features::kBlinkHeapConcurrentMarking) ||
         v8_references_worklist_->IsGlobalEmpty());

  v8::EmbedderHeapTracer* controller =
      reinterpret_cast<v8::EmbedderHeapTracer*>(
          thread_state_->unified_heap_controller());
  return DrainWorklistWithDeadline(
      deadline, v8_references_worklist_.get(),
      [controller](const V8Reference& reference) {
        if (!reference->Get().IsEmpty()) {
          controller->RegisterEmbedderReference(
              reference->template Cast<v8::Data>().Get());
        }
      },
      WorklistTaskId::MutatorThread);
}

ThreadHeap* ThreadHeap::main_thread_heap_ = nullptr;

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_compact.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/sparse_heap_bitmap.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

bool HeapCompact::force_compaction_gc_ = false;

// The real worker behind heap compaction, recording references to movable
// objects ("slots".) When the objects end up being compacted and moved,
// relocate() will adjust the slots to point to the new location of the
// object along with handling fixups for interior pointers.
//
// The "fixups" object is created and maintained for the lifetime of one
// heap compaction-enhanced GC.
class HeapCompact::MovableObjectFixups final {
 public:
  static std::unique_ptr<MovableObjectFixups> Create(ThreadHeap* heap) {
    return base::WrapUnique(new MovableObjectFixups(heap));
  }

  ~MovableObjectFixups() = default;

  // For the arenas being compacted, record all pages belonging to them.
  // This is needed to handle 'interior slots', pointers that themselves
  // can move (independently from the reference the slot points to.)
  void AddCompactingPage(BasePage* page) {
    DCHECK(!page->IsLargeObjectPage());
    relocatable_pages_.insert(page);
  }

  void AddInteriorFixup(MovableReference* slot) {
    auto it = interior_fixups_.find(slot);
    // Ephemeron fixpoint iterations may cause repeated registrations.
    if (UNLIKELY(it != interior_fixups_.end())) {
      DCHECK(!it->value);
      return;
    }
    interior_fixups_.insert(slot, nullptr);
    LOG_HEAP_COMPACTION() << "Interior slot: " << slot;
    Address slot_address = reinterpret_cast<Address>(slot);
    if (!interiors_) {
      interiors_ = SparseHeapBitmap::Create(slot_address);
      return;
    }
    interiors_->Add(slot_address);
  }

  void Add(MovableReference* slot) {
    DCHECK(*slot);
    MovableReference reference = *slot;
    BasePage* ref_page =
        heap_->LookupPageForAddress(reinterpret_cast<Address>(reference));

    // ref_page is null if *slot is pointing to an off-heap region. This may
    // happy if *slot is pointing to an inline buffer of HeapVector with inline
    // capacity.
    if (!ref_page)
      return;
    // Nothing to compact on a large object's page.
    if (ref_page->IsLargeObjectPage())
      return;

    if (!HeapCompact::IsCompactableArena(ref_page->Arena()->ArenaIndex()))
      return;
#if DCHECK_IS_ON()
    auto it = fixups_.find(reference);
    DCHECK(it == fixups_.end() || it->value == slot);
#endif

    // TODO: when updateHeapResidency() becomes more discriminating about
    // leaving out arenas that aren't worth compacting, a check for
    // isCompactingArena() would be appropriate here, leaving early if
    // |refPage|'s arena isn't in the set.

    fixups_.insert(reference, slot);

    // Note: |slot| will reside outside the Oilpan heap if it is a
    // PersistentHeapCollectionBase. Hence pageFromObject() cannot be
    // used, as it sanity checks the |BasePage| it returns. Simply
    // derive the raw BasePage address here and check if it is a member
    // of the compactable and relocatable page address set.
    Address slot_address = reinterpret_cast<Address>(slot);
    void* slot_page_address =
        BlinkPageAddress(slot_address) + kBlinkGuardPageSize;
    if (LIKELY(!relocatable_pages_.Contains(slot_page_address)))
      return;
#if DCHECK_IS_ON()
    BasePage* slot_page = reinterpret_cast<BasePage*>(slot_page_address);
    DCHECK(slot_page->Contains(slot_address));
#endif
    // Unlikely case, the slot resides on a compacting arena's page.
    //  => It is an 'interior slot' (interior to a movable backing store.)
    // Record it as an interior slot, which entails:
    //
    //  - Storing it in the interior map, which maps the slot to
    //    its (eventual) location. Initially nullptr.
    //  - Mark it as being interior pointer within the page's
    //    "interior" bitmap. This bitmap is used when moving a backing
    //    store, quickly/ier checking if interior slots will have to
    //    be additionally redirected.
    AddInteriorFixup(slot);
  }

  void AddFixupCallback(MovableReference* slot,
                        MovingObjectCallback callback,
                        void* callback_data) {
    DCHECK(!fixup_callbacks_.Contains(slot));
    fixup_callbacks_.insert(
        slot, std::pair<void*, MovingObjectCallback>(callback_data, callback));
  }

  void RemoveFixupCallback(MovableReference* slot) {
    auto it = fixup_callbacks_.find(slot);
    if (it != fixup_callbacks_.end())
      fixup_callbacks_.erase(it);
  }

  void RelocateInteriorFixups(Address from, Address to, size_t size) {
    SparseHeapBitmap* range = interiors_->HasRange(from, size);
    if (LIKELY(!range))
      return;

    // Scan through the payload, looking for interior pointer slots
    // to adjust. If the backing store of such an interior slot hasn't
    // been moved already, update the slot -> real location mapping.
    // When the backing store is eventually moved, it'll use that location.
    for (size_t offset = 0; offset < size; offset += sizeof(void*)) {
      MovableReference* slot =
          reinterpret_cast<MovableReference*>(from + offset);

      // Early bailout.
      if (!range->IsSet(reinterpret_cast<Address>(slot)))
        continue;

      auto it = interior_fixups_.find(slot);
      if (it == interior_fixups_.end())
        continue;

      // If |slot|'s mapping is set, then the slot has been adjusted already.
      if (it->value)
        continue;

      Address fixup = to + offset;
      LOG_HEAP_COMPACTION() << "Range interior fixup: " << (from + offset)
                            << " " << it->value << " " << fixup;
      // Fill in the relocated location of the original slot at |slot|.
      // when the backing store corresponding to |slot| is eventually
      // moved/compacted, it'll update |to + offset| with a pointer to the
      // moved backing store.
      interior_fixups_.Set(slot, fixup);

      // If the |slot|'s content is pointing into the region [from, from + size)
      // we are dealing with an interior pointer that does not point to a valid
      // HeapObjectHeader. Such references need to be fixed up immediately.
      Address fixup_contents = *reinterpret_cast<Address*>(fixup);
      if (fixup_contents > from && fixup_contents < (from + size)) {
        *reinterpret_cast<Address*>(fixup) = fixup_contents - from + to;
        continue;
      }
    }
  }

  void Relocate(Address from, Address to) {
    auto it = fixups_.find(from);
    // This means that there is no corresponding slot for a live backing store.
    // This may happen because a mutator may change the slot to point to a
    // different backing store because e.g.:
    // - Incremental marking marked a backing store as live that was later on
    //   replaced.
    // - Backings were changed when being processed in
    //   EagerSweep/PreFinalizer/WeakProcessing.
    if (it == fixups_.end())
      return;

#if DCHECK_IS_ON()
    BasePage* from_page = PageFromObject(from);
    DCHECK(relocatable_pages_.Contains(from_page));
#endif

    // If the object is referenced by a slot that is contained on a compacted
    // area itself, check whether it can be updated already.
    MovableReference* slot = reinterpret_cast<MovableReference*>(it->value);
    auto interior = interior_fixups_.find(slot);
    if (interior != interior_fixups_.end()) {
      MovableReference* slot_location =
          reinterpret_cast<MovableReference*>(interior->value);
      if (!slot_location) {
        interior_fixups_.Set(slot, to);
      } else {
        LOG_HEAP_COMPACTION()
            << "Redirected slot: " << slot << " => " << slot_location;
        slot = slot_location;
      }
    }

    // If the slot has subsequently been updated, a prefinalizer or
    // a destructor having mutated and expanded/shrunk the collection,
    // do not update and relocate the slot -- |from| is no longer valid
    // and referenced.
    //
    // The slot's contents may also have been cleared during weak processing;
    // no work to be done in that case either.
    if (UNLIKELY(*slot != from)) {
      LOG_HEAP_COMPACTION()
          << "No relocation: slot = " << slot << ", *slot = " << *slot
          << ", from = " << from << ", to = " << to;
      VerifyUpdatedSlot(slot);
      return;
    }

    // Update the slots new value.
    *slot = to;

    size_t size = 0;

    // Execute potential fixup callbacks.
    MovableReference* callback_slot =
        reinterpret_cast<MovableReference*>(it->value);
    auto callback = fixup_callbacks_.find(callback_slot);
    if (UNLIKELY(callback != fixup_callbacks_.end())) {
      size = HeapObjectHeader::FromPayload(to)->PayloadSize();
      callback->value.second(callback->value.first, from, to, size);
    }

    if (!interiors_)
      return;

    if (!size)
      size = HeapObjectHeader::FromPayload(to)->PayloadSize();
    RelocateInteriorFixups(from, to, size);
  }

#if DEBUG_HEAP_COMPACTION
  void dumpDebugStats() {
    LOG_HEAP_COMPACTION() << "Fixups: pages=" << relocatable_pages_.size()
                          << " objects=" << fixups_.size()
                          << " callbacks=" << fixup_callbacks_.size()
                          << " interior-size="
                          << (interiors_ ? interiors_->IntervalCount() : 0,
                              interior_fixups_.size());
  }
#endif

 private:
  MovableObjectFixups(ThreadHeap* heap) : heap_(heap) {}

  void VerifyUpdatedSlot(MovableReference* slot);

  ThreadHeap* heap_;

  // Tracking movable and updatable references. For now, we keep a
  // map which for each movable object, recording the slot that
  // points to it. Upon moving the object, that slot needs to be
  // updated.
  //
  // (TODO: consider in-place updating schemes.)
  HashMap<MovableReference, MovableReference*> fixups_;

  // Map from movable reference to callbacks that need to be invoked
  // when the object moves.
  HashMap<MovableReference*, std::pair<void*, MovingObjectCallback>>
      fixup_callbacks_;

  // Slot => relocated slot/final location.
  HashMap<MovableReference*, Address> interior_fixups_;

  // All pages that are being compacted. The set keeps references to
  // BasePage instances. The void* type was selected to allow to check
  // arbitrary addresses.
  HashSet<void*> relocatable_pages_;

  std::unique_ptr<SparseHeapBitmap> interiors_;
};

void HeapCompact::MovableObjectFixups::VerifyUpdatedSlot(
    MovableReference* slot) {
// Verify that the already updated slot is valid, meaning:
//  - has been cleared.
//  - has been updated & expanded with a large object backing store.
//  - has been updated with a larger, freshly allocated backing store.
//    (on a fresh page in a compactable arena that is not being
//    compacted.)
#if DCHECK_IS_ON()
  if (!*slot)
    return;
  BasePage* slot_page =
      heap_->LookupPageForAddress(reinterpret_cast<Address>(*slot));
  // ref_page is null if *slot is pointing to an off-heap region. This may
  // happy if *slot is pointing to an inline buffer of HeapVector with
  // inline capacity.
  if (!slot_page)
    return;
  DCHECK(slot_page->IsLargeObjectPage() ||
         (HeapCompact::IsCompactableArena(slot_page->Arena()->ArenaIndex()) &&
          !relocatable_pages_.Contains(slot_page)));
#endif  // DCHECK_IS_ON()
}

HeapCompact::HeapCompact(ThreadHeap* heap)
    : heap_(heap),
      do_compact_(false),
      gc_count_since_last_compaction_(0),
      free_list_size_(0),
      compactable_arenas_(0u) {
  // The heap compaction implementation assumes the contiguous range,
  //
  //   [Vector1ArenaIndex, HashTableArenaIndex]
  //
  // in a few places. Use static asserts here to not have that assumption
  // be silently invalidated by ArenaIndices changes.
  static_assert(BlinkGC::kVector1ArenaIndex + 3 == BlinkGC::kVector4ArenaIndex,
                "unexpected ArenaIndices ordering");
  static_assert(
      BlinkGC::kVector4ArenaIndex + 1 == BlinkGC::kInlineVectorArenaIndex,
      "unexpected ArenaIndices ordering");
  static_assert(
      BlinkGC::kInlineVectorArenaIndex + 1 == BlinkGC::kHashTableArenaIndex,
      "unexpected ArenaIndices ordering");
}

HeapCompact::~HeapCompact() = default;

HeapCompact::MovableObjectFixups& HeapCompact::Fixups() {
  if (!fixups_)
    fixups_ = MovableObjectFixups::Create(heap_);
  return *fixups_;
}

bool HeapCompact::ShouldCompact(ThreadHeap* heap,
                                BlinkGC::StackState stack_state,
                                BlinkGC::MarkingType marking_type,
                                BlinkGC::GCReason reason) {
#if !ENABLE_HEAP_COMPACTION
  return false;
#else
  if (!RuntimeEnabledFeatures::HeapCompactionEnabled())
    return false;

  LOG_HEAP_COMPACTION() << "shouldCompact(): gc=" << static_cast<int>(reason)
                        << " count=" << gc_count_since_last_compaction_
                        << " free=" << free_list_size_;
  gc_count_since_last_compaction_++;

  // If the GCing thread requires a stack scan, do not compact.
  // Why? Should the stack contain an iterator pointing into its
  // associated backing store, its references wouldn't be
  // correctly relocated.
  if (stack_state == BlinkGC::kHeapPointersOnStack)
    return false;

  if (reason == BlinkGC::GCReason::kTesting) {
    UpdateHeapResidency();
    return force_compaction_gc_;
  }

  // TODO(keishi): Should be enable after fixing the crashes.
  if (marking_type == BlinkGC::kIncrementalMarking)
    return false;

  // TODO(harukamt): Add kIncrementalIdleGC and kIncrementalV8FollowupGC when we
  // enable heap compaction for incremental marking.
  if (reason != BlinkGC::GCReason::kIdleGC &&
      reason != BlinkGC::GCReason::kPreciseGC &&
      reason != BlinkGC::GCReason::kForcedGC)
    return false;

  // Compaction enable rules:
  //  - It's been a while since the last time.
  //  - "Considerable" amount of heap memory is bound up in freelist
  //    allocations. For now, use a fixed limit irrespective of heap
  //    size.
  //
  // As this isn't compacting all arenas, the cost of doing compaction
  // isn't a worry as it will additionally only be done by idle GCs.
  // TODO: add some form of compaction overhead estimate to the marking
  // time estimate.

  UpdateHeapResidency();

#if STRESS_TEST_HEAP_COMPACTION
  // Exercise the handling of object movement by compacting as
  // often as possible.
  return true;
#else
  return force_compaction_gc_ || (gc_count_since_last_compaction_ >
                                      kGCCountSinceLastCompactionThreshold &&
                                  free_list_size_ > kFreeListSizeThreshold);
#endif
#endif
}

void HeapCompact::Initialize(ThreadState* state) {
  DCHECK(RuntimeEnabledFeatures::HeapCompactionEnabled());
  LOG_HEAP_COMPACTION() << "Compacting: free=" << free_list_size_;
  do_compact_ = true;
  fixups_.reset();
  gc_count_since_last_compaction_ = 0;
  force_compaction_gc_ = false;
}

void HeapCompact::RemoveSlot(MovableReference* slot) {
  auto it = traced_slots_.find(slot);
  if (it != traced_slots_.end())
    traced_slots_.erase(it);
  Fixups().RemoveFixupCallback(slot);
}

void HeapCompact::RegisterMovingObjectReference(MovableReference* slot) {
  if (!do_compact_)
    return;

  traced_slots_.insert(slot);
}

void HeapCompact::RegisterMovingObjectCallback(MovableReference* slot,
                                               MovingObjectCallback callback,
                                               void* callback_data) {
  if (!do_compact_)
    return;

  Fixups().AddFixupCallback(slot, callback, callback_data);
}

void HeapCompact::UpdateHeapResidency() {
  size_t total_arena_size = 0;
  size_t total_free_list_size = 0;

  compactable_arenas_ = 0;
#if DEBUG_HEAP_FREELIST
  std::stringstream stream;
#endif
  for (int i = BlinkGC::kVector1ArenaIndex; i <= BlinkGC::kHashTableArenaIndex;
       ++i) {
    NormalPageArena* arena = static_cast<NormalPageArena*>(heap_->Arena(i));
    size_t arena_size = arena->ArenaSize();
    size_t free_list_size = arena->FreeListSize();
    total_arena_size += arena_size;
    total_free_list_size += free_list_size;
#if DEBUG_HEAP_FREELIST
    stream << i << ": [" << arena_size << ", " << free_list_size << "], ";
#endif
    // TODO: be more discriminating and consider arena
    // load factor, effectiveness of past compactions etc.
    if (!arena_size)
      continue;
    // Mark the arena as compactable.
    compactable_arenas_ |= 0x1u << i;
  }
#if DEBUG_HEAP_FREELIST
  LOG_HEAP_FREELIST() << "Arena residencies: {" << stream.str() << "}";
  LOG_HEAP_FREELIST() << "Total = " << total_arena_size
                      << ", Free = " << total_free_list_size;
#endif

  // TODO(sof): consider smoothing the reported sizes.
  free_list_size_ = total_free_list_size;
}

void HeapCompact::FinishedArenaCompaction(NormalPageArena* arena,
                                          size_t freed_pages,
                                          size_t freed_size) {
  if (!do_compact_)
    return;

  heap_->stats_collector()->IncreaseCompactionFreedPages(freed_pages);
  heap_->stats_collector()->IncreaseCompactionFreedSize(freed_size);
}

void HeapCompact::Relocate(Address from, Address to) {
  Fixups().Relocate(from, to);
}

void HeapCompact::StartThreadCompaction() {
  if (!do_compact_)
    return;

  // The mapping between the slots and the backing stores are created
  for (auto** slot : traced_slots_) {
    if (*slot)
      Fixups().Add(slot);
  }
  traced_slots_.clear();
}

void HeapCompact::FinishThreadCompaction() {
  if (!do_compact_)
    return;

#if DEBUG_HEAP_COMPACTION
  if (fixups_)
    fixups_->dumpDebugStats();
#endif
  fixups_.reset();
  do_compact_ = false;
}

void HeapCompact::AddCompactingPage(BasePage* page) {
  DCHECK(do_compact_);
  DCHECK(IsCompactingArena(page->Arena()->ArenaIndex()));
  Fixups().AddCompactingPage(page);
}

bool HeapCompact::ScheduleCompactionGCForTesting(bool value) {
  bool current = force_compaction_gc_;
  force_compaction_gc_ = value;
  return current;
}

}  // namespace blink

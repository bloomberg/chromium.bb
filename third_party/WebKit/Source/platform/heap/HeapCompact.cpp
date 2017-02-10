// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/HeapCompact.h"

#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Heap.h"
#include "platform/heap/SparseHeapBitmap.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

bool HeapCompact::s_forceCompactionGC = false;

// The real worker behind heap compaction, recording references to movable
// objects ("slots".) When the objects end up being compacted and moved,
// relocate() will adjust the slots to point to the new location of the
// object along with handling fixups for interior pointers.
//
// The "fixups" object is created and maintained for the lifetime of one
// heap compaction-enhanced GC.
class HeapCompact::MovableObjectFixups final {
 public:
  static std::unique_ptr<MovableObjectFixups> create() {
    return WTF::wrapUnique(new MovableObjectFixups);
  }

  ~MovableObjectFixups() {}

  // For the arenas being compacted, record all pages belonging to them.
  // This is needed to handle 'interior slots', pointers that themselves
  // can move (independently from the reference the slot points to.)
  void addCompactingPage(BasePage* page) {
    DCHECK(!page->isLargeObjectPage());
    m_relocatablePages.insert(page);
  }

  void addInteriorFixup(MovableReference* slot) {
    auto it = m_interiorFixups.find(slot);
    // Ephemeron fixpoint iterations may cause repeated registrations.
    if (UNLIKELY(it != m_interiorFixups.end())) {
      DCHECK(!it->value);
      return;
    }
    m_interiorFixups.insert(slot, nullptr);
    LOG_HEAP_COMPACTION("Interior slot: %p\n", slot);
    Address slotAddress = reinterpret_cast<Address>(slot);
    if (!m_interiors) {
      m_interiors = SparseHeapBitmap::create(slotAddress);
      return;
    }
    m_interiors->add(slotAddress);
  }

  void add(MovableReference* slot) {
    MovableReference reference = *slot;
    BasePage* refPage = pageFromObject(reference);
    // Nothing to compact on a large object's page.
    if (refPage->isLargeObjectPage())
      return;

#if DCHECK_IS_ON()
    DCHECK(HeapCompact::isCompactableArena(refPage->arena()->arenaIndex()));
    auto it = m_fixups.find(reference);
    DCHECK(it == m_fixups.end() || it->value == slot);
#endif

    // TODO: when updateHeapResidency() becomes more discriminating about
    // leaving out arenas that aren't worth compacting, a check for
    // isCompactingArena() would be appropriate here, leaving early if
    // |refPage|'s arena isn't in the set.

    m_fixups.insert(reference, slot);

    // Note: |slot| will reside outside the Oilpan heap if it is a
    // PersistentHeapCollectionBase. Hence pageFromObject() cannot be
    // used, as it sanity checks the |BasePage| it returns. Simply
    // derive the raw BasePage address here and check if it is a member
    // of the compactable and relocatable page address set.
    Address slotAddress = reinterpret_cast<Address>(slot);
    void* slotPageAddress = blinkPageAddress(slotAddress) + blinkGuardPageSize;
    if (LIKELY(!m_relocatablePages.contains(slotPageAddress)))
      return;
#if DCHECK_IS_ON()
    BasePage* slotPage = reinterpret_cast<BasePage*>(slotPageAddress);
    DCHECK(slotPage->contains(slotAddress));
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
    addInteriorFixup(slot);
  }

  void addFixupCallback(MovableReference reference,
                        MovingObjectCallback callback,
                        void* callbackData) {
    DCHECK(!m_fixupCallbacks.contains(reference));
    m_fixupCallbacks.insert(reference, std::pair<void*, MovingObjectCallback>(
                                           callbackData, callback));
  }

  void relocateInteriorFixups(Address from, Address to, size_t size) {
    SparseHeapBitmap* range = m_interiors->hasRange(from, size);
    if (LIKELY(!range))
      return;

    // Scan through the payload, looking for interior pointer slots
    // to adjust. If the backing store of such an interior slot hasn't
    // been moved already, update the slot -> real location mapping.
    // When the backing store is eventually moved, it'll use that location.
    //
    for (size_t offset = 0; offset < size; offset += sizeof(void*)) {
      if (!range->isSet(from + offset))
        continue;
      MovableReference* slot =
          reinterpret_cast<MovableReference*>(from + offset);
      auto it = m_interiorFixups.find(slot);
      if (it == m_interiorFixups.end())
        continue;

      // TODO: with the right sparse bitmap representation, it could be possible
      // to quickly determine if we've now stepped past the last address
      // that needed fixup in [address, address + size). Breaking out of this
      // loop might be worth doing for hash table backing stores with a very
      // low load factor. But interior fixups are rare.

      // If |slot|'s mapping is set, then the slot has been adjusted already.
      if (it->value)
        continue;
      Address fixup = to + offset;
      LOG_HEAP_COMPACTION("Range interior fixup: %p %p %p\n", from + offset,
                          it->value, fixup);
      // Fill in the relocated location of the original slot at |slot|.
      // when the backing store corresponding to |slot| is eventually
      // moved/compacted, it'll update |to + offset| with a pointer to the
      // moved backing store.
      m_interiorFixups.set(slot, fixup);
    }
  }

  void relocate(Address from, Address to) {
    auto it = m_fixups.find(from);
    DCHECK(it != m_fixups.end());
#if DCHECK_IS_ON()
    BasePage* fromPage = pageFromObject(from);
    DCHECK(m_relocatablePages.contains(fromPage));
#endif
    MovableReference* slot = reinterpret_cast<MovableReference*>(it->value);
    auto interior = m_interiorFixups.find(slot);
    if (interior != m_interiorFixups.end()) {
      MovableReference* slotLocation =
          reinterpret_cast<MovableReference*>(interior->value);
      if (!slotLocation) {
        m_interiorFixups.set(slot, to);
      } else {
        LOG_HEAP_COMPACTION("Redirected slot: %p => %p\n", slot, slotLocation);
        slot = slotLocation;
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
      LOG_HEAP_COMPACTION(
          "No relocation: slot = %p, *slot = %p, from = %p, to = %p\n", slot,
          *slot, from, to);
#if DCHECK_IS_ON()
      // Verify that the already updated slot is valid, meaning:
      //  - has been cleared.
      //  - has been updated & expanded with a large object backing store.
      //  - has been updated with a larger, freshly allocated backing store.
      //    (on a fresh page in a compactable arena that is not being
      //    compacted.)
      if (!*slot)
        return;
      BasePage* slotPage = pageFromObject(*slot);
      DCHECK(
          slotPage->isLargeObjectPage() ||
          (HeapCompact::isCompactableArena(slotPage->arena()->arenaIndex()) &&
           !m_relocatablePages.contains(slotPage)));
#endif
      return;
    }
    *slot = to;

    size_t size = 0;
    auto callback = m_fixupCallbacks.find(from);
    if (UNLIKELY(callback != m_fixupCallbacks.end())) {
      size = HeapObjectHeader::fromPayload(to)->payloadSize();
      callback->value.second(callback->value.first, from, to, size);
    }

    if (!m_interiors)
      return;

    if (!size)
      size = HeapObjectHeader::fromPayload(to)->payloadSize();
    relocateInteriorFixups(from, to, size);
  }

#if DEBUG_HEAP_COMPACTION
  void dumpDebugStats() {
    LOG_HEAP_COMPACTION(
        "Fixups: pages=%u objects=%u callbacks=%u interior-size=%zu"
        " interiors-f=%u\n",
        m_relocatablePages.size(), m_fixups.size(), m_fixupCallbacks.size(),
        m_interiors ? m_interiors->intervalCount() : 0,
        m_interiorFixups.size());
  }
#endif

 private:
  MovableObjectFixups() {}

  // Tracking movable and updatable references. For now, we keep a
  // map which for each movable object, recording the slot that
  // points to it. Upon moving the object, that slot needs to be
  // updated.
  //
  // (TODO: consider in-place updating schemes.)
  HashMap<MovableReference, MovableReference*> m_fixups;

  // Map from movable reference to callbacks that need to be invoked
  // when the object moves.
  HashMap<MovableReference, std::pair<void*, MovingObjectCallback>>
      m_fixupCallbacks;

  // Slot => relocated slot/final location.
  HashMap<MovableReference*, Address> m_interiorFixups;

  // All pages that are being compacted. The set keeps references to
  // BasePage instances. The void* type was selected to allow to check
  // arbitrary addresses.
  HashSet<void*> m_relocatablePages;

  std::unique_ptr<SparseHeapBitmap> m_interiors;
};

HeapCompact::HeapCompact()
    : m_doCompact(false),
      m_gcCountSinceLastCompaction(0),
      m_freeListSize(0),
      m_compactableArenas(0u),
      m_freedPages(0),
      m_freedSize(0),
      m_startCompactionTimeMS(0) {
  // The heap compaction implementation assumes the contiguous range,
  //
  //   [Vector1ArenaIndex, HashTableArenaIndex]
  //
  // in a few places. Use static asserts here to not have that assumption
  // be silently invalidated by ArenaIndices changes.
  static_assert(BlinkGC::Vector1ArenaIndex + 3 == BlinkGC::Vector4ArenaIndex,
                "unexpected ArenaIndices ordering");
  static_assert(
      BlinkGC::Vector4ArenaIndex + 1 == BlinkGC::InlineVectorArenaIndex,
      "unexpected ArenaIndices ordering");
  static_assert(
      BlinkGC::InlineVectorArenaIndex + 1 == BlinkGC::HashTableArenaIndex,
      "unexpected ArenaIndices ordering");
}

HeapCompact::~HeapCompact() {}

HeapCompact::MovableObjectFixups& HeapCompact::fixups() {
  if (!m_fixups)
    m_fixups = MovableObjectFixups::create();
  return *m_fixups;
}

bool HeapCompact::shouldCompact(ThreadState* state,
                                BlinkGC::GCType gcType,
                                BlinkGC::GCReason reason) {
#if !ENABLE_HEAP_COMPACTION
  return false;
#else
  if (!RuntimeEnabledFeatures::heapCompactionEnabled())
    return false;

  LOG_HEAP_COMPACTION("shouldCompact(): gc=%s count=%zu free=%zu\n",
                      ThreadState::gcReasonString(reason),
                      m_gcCountSinceLastCompaction, m_freeListSize);
  m_gcCountSinceLastCompaction++;
  // It is only safe to compact during non-conservative GCs.
  // TODO: for the main thread, limit this further to only idle GCs.
  if (reason != BlinkGC::IdleGC && reason != BlinkGC::PreciseGC &&
      reason != BlinkGC::ForcedGC)
    return false;

  // If the GCing thread requires a stack scan, do not compact.
  // Why? Should the stack contain an iterator pointing into its
  // associated backing store, its references wouldn't be
  // correctly relocated.
  if (state->stackState() == BlinkGC::HeapPointersOnStack)
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

  updateHeapResidency(state);

#if STRESS_TEST_HEAP_COMPACTION
  // Exercise the handling of object movement by compacting as
  // often as possible.
  return true;
#else
  return s_forceCompactionGC ||
         (m_gcCountSinceLastCompaction > kGCCountSinceLastCompactionThreshold &&
          m_freeListSize > kFreeListSizeThreshold);
#endif
#endif
}

void HeapCompact::initialize(ThreadState* state) {
  DCHECK(RuntimeEnabledFeatures::heapCompactionEnabled());
  LOG_HEAP_COMPACTION("Compacting: free=%zu\n", m_freeListSize);
  m_doCompact = true;
  m_freedPages = 0;
  m_freedSize = 0;
  m_fixups.reset();
  m_gcCountSinceLastCompaction = 0;
  s_forceCompactionGC = false;
}

void HeapCompact::registerMovingObjectReference(MovableReference* slot) {
  if (!m_doCompact)
    return;

  fixups().add(slot);
}

void HeapCompact::registerMovingObjectCallback(MovableReference reference,
                                               MovingObjectCallback callback,
                                               void* callbackData) {
  if (!m_doCompact)
    return;

  fixups().addFixupCallback(reference, callback, callbackData);
}

void HeapCompact::updateHeapResidency(ThreadState* threadState) {
  size_t totalArenaSize = 0;
  size_t totalFreeListSize = 0;

  m_compactableArenas = 0;
#if DEBUG_HEAP_FREELIST
  LOG_HEAP_FREELIST("Arena residencies: {");
#endif
  for (int i = BlinkGC::Vector1ArenaIndex; i <= BlinkGC::HashTableArenaIndex;
       ++i) {
    NormalPageArena* arena =
        static_cast<NormalPageArena*>(threadState->arena(i));
    size_t arenaSize = arena->arenaSize();
    size_t freeListSize = arena->freeListSize();
    totalArenaSize += arenaSize;
    totalFreeListSize += freeListSize;
    LOG_HEAP_FREELIST("%d: [%zu, %zu], ", i, arenaSize, freeListSize);
    // TODO: be more discriminating and consider arena
    // load factor, effectiveness of past compactions etc.
    if (!arenaSize)
      continue;
    // Mark the arena as compactable.
    m_compactableArenas |= (0x1u << (BlinkGC::Vector1ArenaIndex + i));
  }
  LOG_HEAP_FREELIST("}\nTotal = %zu, Free = %zu\n", totalArenaSize,
                    totalFreeListSize);

  // TODO(sof): consider smoothing the reported sizes.
  m_freeListSize = totalFreeListSize;
}

void HeapCompact::finishedArenaCompaction(NormalPageArena* arena,
                                          size_t freedPages,
                                          size_t freedSize) {
  if (!m_doCompact)
    return;

  m_freedPages += freedPages;
  m_freedSize += freedSize;
}

void HeapCompact::relocate(Address from, Address to) {
  DCHECK(m_fixups);
  m_fixups->relocate(from, to);
}

void HeapCompact::startThreadCompaction() {
  if (!m_doCompact)
    return;

  if (!m_startCompactionTimeMS)
    m_startCompactionTimeMS = WTF::currentTimeMS();
}

void HeapCompact::finishThreadCompaction() {
  if (!m_doCompact)
    return;

#if DEBUG_HEAP_COMPACTION
  if (m_fixups)
    m_fixups->dumpDebugStats();
#endif
  m_fixups.reset();
  m_doCompact = false;

  double timeForHeapCompaction = WTF::currentTimeMS() - m_startCompactionTimeMS;
  DEFINE_STATIC_LOCAL(CustomCountHistogram, timeForHeapCompactionHistogram,
                      ("BlinkGC.TimeForHeapCompaction", 1, 10 * 1000, 50));
  timeForHeapCompactionHistogram.count(timeForHeapCompaction);
  m_startCompactionTimeMS = 0;

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, objectSizeFreedByHeapCompaction,
      ("BlinkGC.ObjectSizeFreedByHeapCompaction", 1, 4 * 1024 * 1024, 50));
  objectSizeFreedByHeapCompaction.count(m_freedSize / 1024);

#if DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME
  LOG_HEAP_COMPACTION_INTERNAL(
      "Compaction stats: time=%gms, pages freed=%zu, size=%zu\n",
      timeForHeapCompaction, m_freedPages, m_freedSize);
#else
  LOG_HEAP_COMPACTION("Compaction stats: freed pages=%zu size=%zu\n",
                      m_freedPages, m_freedSize);
#endif
}

void HeapCompact::addCompactingPage(BasePage* page) {
  DCHECK(m_doCompact);
  DCHECK(isCompactingArena(page->arena()->arenaIndex()));
  fixups().addCompactingPage(page);
}

bool HeapCompact::scheduleCompactionGCForTesting(bool value) {
  bool current = s_forceCompactionGC;
  s_forceCompactionGC = value;
  return current;
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapCompact_h
#define HeapCompact_h

#include "platform/PlatformExport.h"
#include "platform/heap/BlinkGC.h"
#include "wtf/DataLog.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

#include <bitset>
#include <utility>

// Compaction-specific debug switches:

// Set to 0 to prevent compaction GCs, disabling the heap compaction feature.
#define ENABLE_HEAP_COMPACTION 1

// Emit debug info during compaction.
#define DEBUG_HEAP_COMPACTION 0

// Emit stats on freelist occupancy.
// 0 - disabled, 1 - minimal, 2 - verbose.
#define DEBUG_HEAP_FREELIST 0

// Log the amount of time spent compacting.
#define DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME 0

// Compact during all idle + precise GCs; for debugging.
#define STRESS_TEST_HEAP_COMPACTION 0

namespace blink {

class NormalPageArena;
class BasePage;
class ThreadState;

class PLATFORM_EXPORT HeapCompact final {
 public:
  static std::unique_ptr<HeapCompact> create() {
    return WTF::wrapUnique(new HeapCompact);
  }

  ~HeapCompact();

  // Determine if a GC for the given type and reason should also perform
  // additional heap compaction.
  //
  bool shouldCompact(ThreadState*, BlinkGC::GCType, BlinkGC::GCReason);

  // Compaction should be performed as part of the ongoing GC, initialize
  // the heap compaction pass. Returns the appropriate visitor type to
  // use when running the marking phase.
  void initialize(ThreadState*);

  // Returns true if the ongoing GC will perform compaction.
  bool isCompacting() const { return m_doCompact; }

  // Returns true if the ongoing GC will perform compaction for the given
  // heap arena.
  bool isCompactingArena(int arenaIndex) const {
    return m_doCompact && (m_compactableArenas & (0x1u << arenaIndex));
  }

  // Returns |true| if the ongoing GC may compact the given arena/sub-heap.
  static bool isCompactableArena(int arenaIndex) {
    return arenaIndex >= BlinkGC::Vector1ArenaIndex &&
           arenaIndex <= BlinkGC::HashTableArenaIndex;
  }

  // See |Heap::registerMovingObjectReference()| documentation.
  void registerMovingObjectReference(MovableReference* slot);

  // See |Heap::registerMovingObjectCallback()| documentation.
  void registerMovingObjectCallback(MovableReference,
                                    MovingObjectCallback,
                                    void* callbackData);

  // Thread signalling that a compaction pass is starting or has
  // completed.
  //
  // A thread participating in a heap GC will wait on the completion
  // of compaction across all threads. No thread can be allowed to
  // potentially access another thread's heap arenas while they're
  // still being compacted.
  void startThreadCompaction();
  void finishThreadCompaction();

  // Perform any relocation post-processing after having completed compacting
  // the given arena. The number of pages that were freed together with the
  // total size (in bytes) of freed heap storage, are passed in as arguments.
  void finishedArenaCompaction(NormalPageArena*,
                               size_t freedPages,
                               size_t freedSize);

  // Register the heap page as containing live objects that will all be
  // compacted. Registration happens as part of making the arenas ready
  // for a GC.
  void addCompactingPage(BasePage*);

  // Notify heap compaction that object at |from| has been relocated to.. |to|.
  // (Called by the sweep compaction pass.)
  void relocate(Address from, Address to);

  // For unit testing only: arrange for a compaction GC to be triggered
  // next time a non-conservative GC is run. Sets the compact-next flag
  // to the new value, returning old.
  static bool scheduleCompactionGCForTesting(bool);

 private:
  class MovableObjectFixups;

  HeapCompact();

  // Sample the amount of fragmentation and heap memory currently residing
  // on the freelists of the arenas we're able to compact. The computed
  // numbers will be subsequently used to determine if a heap compaction
  // is on order (shouldCompact().)
  void updateHeapResidency(ThreadState*);

  // Parameters controlling when compaction should be done:

  // Number of GCs that must have passed since last compaction GC.
  static const int kGCCountSinceLastCompactionThreshold = 10;

  // Freelist size threshold that must be exceeded before compaction
  // should be considered.
  static const size_t kFreeListSizeThreshold = 512 * 1024;

  MovableObjectFixups& fixups();

  std::unique_ptr<MovableObjectFixups> m_fixups;

  // Set to |true| when a compacting sweep will go ahead.
  bool m_doCompact;
  size_t m_gcCountSinceLastCompaction;

  // Last reported freelist size, across all compactable arenas.
  size_t m_freeListSize;

  // If compacting, i'th heap arena will be compacted
  // if corresponding bit is set. Indexes are in
  // the range of BlinkGC::ArenaIndices.
  unsigned m_compactableArenas;

  // Stats, number of (complete) pages freed/decommitted +
  // bytes freed (which will include partial pages.)
  size_t m_freedPages;
  size_t m_freedSize;

  double m_startCompactionTimeMS;

  static bool s_forceCompactionGC;
};

}  // namespace blink

// Logging macros activated by debug switches.

#define LOG_HEAP_COMPACTION_INTERNAL(msg, ...) dataLogF(msg, ##__VA_ARGS__)

#if DEBUG_HEAP_COMPACTION
#define LOG_HEAP_COMPACTION(msg, ...) \
  LOG_HEAP_COMPACTION_INTERNAL(msg, ##__VA_ARGS__)
#else
#define LOG_HEAP_COMPACTION(msg, ...) \
  do {                                \
  } while (0)
#endif

#if DEBUG_HEAP_FREELIST
#define LOG_HEAP_FREELIST(msg, ...) \
  LOG_HEAP_COMPACTION_INTERNAL(msg, ##__VA_ARGS__)
#else
#define LOG_HEAP_FREELIST(msg, ...) \
  do {                              \
  } while (0)
#endif

#if DEBUG_HEAP_FREELIST == 2
#define LOG_HEAP_FREELIST_VERBOSE(msg, ...) \
  LOG_HEAP_COMPACTION_INTERNAL(msg, ##__VA_ARGS__)
#else
#define LOG_HEAP_FREELIST_VERBOSE(msg, ...) \
  do {                                      \
  } while (0)
#endif

#endif  // HeapCompact_h

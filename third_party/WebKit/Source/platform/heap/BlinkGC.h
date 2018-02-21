// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkGC_h
#define BlinkGC_h

// BlinkGC.h is a file that defines common things used by Blink GC.

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"

#define PRINT_HEAP_STATS 0  // Enable this macro to print heap stats to stderr.

namespace blink {

class MarkingVisitor;
class Visitor;

using Address = uint8_t*;

using FinalizationCallback = void (*)(void*);
using VisitorCallback = void (*)(Visitor*, void*);
using MarkingVisitorCallback = void (*)(MarkingVisitor*, void*);
using TraceCallback = VisitorCallback;
using WeakCallback = VisitorCallback;
using EphemeronCallback = VisitorCallback;

// Simple alias to avoid heap compaction type signatures turning into
// a sea of generic |void*|s.
using MovableReference = void*;

// Heap compaction supports registering callbacks that are to be invoked
// when an object is moved during compaction. This is to support internal
// location fixups that need to happen as a result.
//
// i.e., when the object residing at |from| is moved to |to| by the compaction
// pass, invoke the callback to adjust any internal references that now need
// to be |to|-relative.
using MovingObjectCallback = void (*)(void* callback_data,
                                      MovableReference from,
                                      MovableReference to,
                                      size_t);

// List of typed arenas. The list is used to generate the implementation
// of typed arena related methods.
//
// To create a new typed arena add a H(<ClassName>) to the
// FOR_EACH_TYPED_ARENA macro below.
#define FOR_EACH_TYPED_ARENA(H) \
  H(Node)                       \
  H(CSSValue)

#define TypedArenaEnumName(Type) k##Type##ArenaIndex,

class PLATFORM_EXPORT BlinkGC final {
  STATIC_ONLY(BlinkGC);

 public:
  // When garbage collecting we need to know whether or not there
  // can be pointers to Blink GC managed objects on the stack for
  // each thread. When threads reach a safe point they record
  // whether or not they have pointers on the stack.
  enum StackState { kNoHeapPointersOnStack, kHeapPointersOnStack };

  enum GCType {
    // Both of the marking task and the sweeping task run in
    // ThreadHeap::collectGarbage().
    kGCWithSweep,
    // Only the marking task runs in ThreadHeap::collectGarbage().
    // The sweeping task is split into chunks and scheduled lazily.
    kGCWithoutSweep,
    // Only the marking task runs just to take a heap snapshot.
    // The sweeping task doesn't run. The marks added in the marking task
    // are just cleared.
    kTakeSnapshot,
  };

  enum GCReason {
    kIdleGC,
    kPreciseGC,
    kConservativeGC,
    kForcedGC,
    kMemoryPressureGC,
    kPageNavigationGC,
    kThreadTerminationGC,
    kLastGCReason = kThreadTerminationGC,
  };

  enum ArenaIndices {
    kEagerSweepArenaIndex = 0,
    kNormalPage1ArenaIndex,
    kNormalPage2ArenaIndex,
    kNormalPage3ArenaIndex,
    kNormalPage4ArenaIndex,
    kVector1ArenaIndex,
    kVector2ArenaIndex,
    kVector3ArenaIndex,
    kVector4ArenaIndex,
    kInlineVectorArenaIndex,
    kHashTableArenaIndex,
    FOR_EACH_TYPED_ARENA(TypedArenaEnumName) kLargeObjectArenaIndex,
    // Values used for iteration of heap segments.
    kNumberOfArenas,
  };

  enum V8GCType {
    kV8MinorGC,
    kV8MajorGC,
  };
};

}  // namespace blink

#endif  // BlinkGC_h

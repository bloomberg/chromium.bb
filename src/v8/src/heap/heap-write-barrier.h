// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_H_

#include "include/v8-internal.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Code;
class FixedArray;
class Heap;
class HeapObject;
class MaybeObject;
class Object;
class RelocInfo;
class EphemeronHashTable;

// Note: In general it is preferred to use the macros defined in
// object-macros.h.

// Write barrier for FixedArray elements.
#define FIXED_ARRAY_ELEMENTS_WRITE_BARRIER(heap, array, start, length) \
  do {                                                                 \
    GenerationalBarrierForElements(heap, array, start, length);        \
    MarkingBarrierForElements(heap, array);                            \
  } while (false)

// Combined write barriers.
void WriteBarrierForCode(Code host, RelocInfo* rinfo, Object value);
void WriteBarrierForCode(Code host);

// Generational write barrier.
void GenerationalBarrier(HeapObject object, ObjectSlot slot, Object value);
void GenerationalBarrier(HeapObject object, MaybeObjectSlot slot,
                         MaybeObject value);
void GenerationalEphemeronKeyBarrier(EphemeronHashTable table, ObjectSlot slot,
                                     Object value);
void GenerationalBarrierForElements(Heap* heap, FixedArray array, int offset,
                                    int length);
void GenerationalBarrierForCode(Code host, RelocInfo* rinfo, HeapObject object);

// Marking write barrier.
void MarkingBarrier(HeapObject object, ObjectSlot slot, Object value);
void MarkingBarrier(HeapObject object, MaybeObjectSlot slot, MaybeObject value);
void MarkingBarrierForElements(Heap* heap, FixedArray array);
void MarkingBarrierForCode(Code host, RelocInfo* rinfo, HeapObject object);

void MarkingBarrierForDescriptorArray(Heap* heap, HeapObject host,
                                      HeapObject descriptor_array,
                                      int number_of_own_descriptors);

Heap* GetHeapFromWritableObject(const HeapObject object);

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_H_

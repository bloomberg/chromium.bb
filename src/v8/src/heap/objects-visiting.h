// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECTS_VISITING_H_
#define V8_HEAP_OBJECTS_VISITING_H_

#include "src/allocation.h"
#include "src/layout-descriptor.h"
#include "src/objects-body-descriptors.h"
#include "src/objects.h"
#include "src/objects/hash-table.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/string.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

class BigInt;
class BytecodeArray;
class DataHandler;
class EmbedderDataArray;
class JSArrayBuffer;
class JSDataView;
class JSRegExp;
class JSTypedArray;
class JSWeakCell;
class JSWeakCollection;
class NativeContext;
class UncompiledDataWithoutPreParsedScope;
class UncompiledDataWithPreParsedScope;
class WasmInstanceObject;

#define TYPED_VISITOR_ID_LIST(V)                                               \
  V(AllocationSite, AllocationSite*)                                           \
  V(BigInt, BigInt*)                                                           \
  V(ByteArray, ByteArray)                                                      \
  V(BytecodeArray, BytecodeArray)                                              \
  V(Cell, Cell*)                                                               \
  V(Code, Code)                                                                \
  V(CodeDataContainer, CodeDataContainer*)                                     \
  V(ConsString, ConsString)                                                    \
  V(Context, Context)                                                          \
  V(DataHandler, DataHandler*)                                                 \
  V(DescriptorArray, DescriptorArray*)                                         \
  V(EmbedderDataArray, EmbedderDataArray)                                      \
  V(EphemeronHashTable, EphemeronHashTable)                                    \
  V(FeedbackCell, FeedbackCell*)                                               \
  V(FeedbackVector, FeedbackVector*)                                           \
  V(FixedArray, FixedArray)                                                    \
  V(FixedDoubleArray, FixedDoubleArray)                                        \
  V(FixedFloat64Array, FixedFloat64Array)                                      \
  V(FixedTypedArrayBase, FixedTypedArrayBase)                                  \
  V(JSArrayBuffer, JSArrayBuffer*)                                             \
  V(JSDataView, JSDataView*)                                                   \
  V(JSObject, JSObject*)                                                       \
  V(JSTypedArray, JSTypedArray*)                                               \
  V(JSWeakCollection, JSWeakCollection*)                                       \
  V(Map, Map)                                                                  \
  V(NativeContext, NativeContext)                                              \
  V(Oddball, Oddball*)                                                         \
  V(PreParsedScopeData, PreParsedScopeData*)                                   \
  V(PropertyArray, PropertyArray)                                              \
  V(PropertyCell, PropertyCell*)                                               \
  V(PrototypeInfo, PrototypeInfo*)                                             \
  V(SeqOneByteString, SeqOneByteString)                                        \
  V(SeqTwoByteString, SeqTwoByteString)                                        \
  V(SharedFunctionInfo, SharedFunctionInfo*)                                   \
  V(SlicedString, SlicedString)                                                \
  V(SmallOrderedHashMap, SmallOrderedHashMap)                                  \
  V(SmallOrderedHashSet, SmallOrderedHashSet)                                  \
  V(SmallOrderedNameDictionary, SmallOrderedNameDictionary)                    \
  V(Symbol, Symbol)                                                            \
  V(ThinString, ThinString)                                                    \
  V(TransitionArray, TransitionArray*)                                         \
  V(UncompiledDataWithoutPreParsedScope, UncompiledDataWithoutPreParsedScope*) \
  V(UncompiledDataWithPreParsedScope, UncompiledDataWithPreParsedScope*)       \
  V(WasmInstanceObject, WasmInstanceObject*)

// The base class for visitors that need to dispatch on object type. The default
// behavior of all visit functions is to iterate body of the given object using
// the BodyDescriptor of the object.
//
// The visit functions return the size of the object cast to ResultType.
//
// This class is intended to be used in the following way:
//
//   class SomeVisitor : public HeapVisitor<ResultType, SomeVisitor> {
//     ...
//   }
template <typename ResultType, typename ConcreteVisitor>
class HeapVisitor : public ObjectVisitor {
 public:
  V8_INLINE ResultType Visit(HeapObject* object);
  V8_INLINE ResultType Visit(Map map, HeapObject* object);

 protected:
  // A guard predicate for visiting the object.
  // If it returns false then the default implementations of the Visit*
  // functions bailout from iterating the object pointers.
  V8_INLINE bool ShouldVisit(HeapObject* object) { return true; }
  // Guard predicate for visiting the objects map pointer separately.
  V8_INLINE bool ShouldVisitMapPointer() { return true; }
  // A callback for visiting the map pointer in the object header.
  V8_INLINE void VisitMapPointer(HeapObject* host, ObjectSlot map);
  // If this predicate returns false, then the heap visitor will fail
  // in default Visit implemention for subclasses of JSObject.
  V8_INLINE bool AllowDefaultJSObjectVisit() { return true; }

#define VISIT(TypeName, Type) \
  V8_INLINE ResultType Visit##TypeName(Map map, Type object);
  TYPED_VISITOR_ID_LIST(VISIT)
#undef VISIT
  V8_INLINE ResultType VisitShortcutCandidate(Map map, ConsString object);
  V8_INLINE ResultType VisitDataObject(Map map, HeapObject* object);
  V8_INLINE ResultType VisitJSObjectFast(Map map, JSObject* object);
  V8_INLINE ResultType VisitJSApiObject(Map map, JSObject* object);
  V8_INLINE ResultType VisitStruct(Map map, HeapObject* object);
  V8_INLINE ResultType VisitFreeSpace(Map map, FreeSpace* object);
  V8_INLINE ResultType VisitWeakArray(Map map, HeapObject* object);

  template <typename T, typename = typename std::enable_if<
                            std::is_base_of<Object, T>::value>::type>
  static V8_INLINE T* Cast(HeapObject* object);

  template <typename T, typename = typename std::enable_if<
                            std::is_base_of<ObjectPtr, T>::value>::type>
  static V8_INLINE T Cast(HeapObject* object);
};

template <typename ConcreteVisitor>
class NewSpaceVisitor : public HeapVisitor<int, ConcreteVisitor> {
 public:
  V8_INLINE bool ShouldVisitMapPointer() { return false; }

  // Special cases for young generation.

  V8_INLINE int VisitNativeContext(Map map, NativeContext object);
  V8_INLINE int VisitJSApiObject(Map map, JSObject* object);

  int VisitBytecodeArray(Map map, BytecodeArray object) {
    UNREACHABLE();
    return 0;
  }

  int VisitSharedFunctionInfo(Map map, SharedFunctionInfo* object) {
    UNREACHABLE();
    return 0;
  }

  int VisitJSWeakCell(Map map, JSWeakCell* js_weak_cell) {
    UNREACHABLE();
    return 0;
  }
};

class WeakObjectRetainer;

// A weak list is single linked list where each element has a weak pointer to
// the next element. Given the head of the list, this function removes dead
// elements from the list and if requested records slots for next-element
// pointers. The template parameter T is a WeakListVisitor that defines how to
// access the next-element pointers.
template <class T>
Object* VisitWeakList(Heap* heap, Object* list, WeakObjectRetainer* retainer);
template <class T>
Object* VisitWeakList2(Heap* heap, Object* list, WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECTS_VISITING_H_

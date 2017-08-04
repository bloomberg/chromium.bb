// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperMember_h
#define TraceWrapperMember_h

#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

template <typename T>
class Member;

// TraceWrapperMember is used for Member fields that should participate in
// wrapper tracing, i.e., strongly hold a ScriptWrappable alive. All
// TraceWrapperMember fields must be traced in the class' |TraceWrappers|
// method.
template <class T>
class TraceWrapperMember : public Member<T> {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  // TODO(mlippautz): Remove constructor taking |parent|.
  TraceWrapperMember(void* parent, T* raw) : Member<T>(raw) {}

  TraceWrapperMember(T* raw) : Member<T>(raw) {
    // We don't require a write barrier here as TraceWrapperMember is used for
    // the following scenarios:
    // - Initial initialization: The write barrier will not fire as the parent
    //   is initially white.
    // - Wrapping when inserting into a container: The write barrier will fire
    //   upon establishing the move into the container.
    // - Assignment to a field: The regular assignment operator will fire the
    //   write barrier.
    // Note that support for black allocation would require a barrier here.
  }

  TraceWrapperMember(WTF::HashTableDeletedValueType x) : Member<T>(x) {}

  TraceWrapperMember(const TraceWrapperMember& other) { *this = other; }

  TraceWrapperMember& operator=(const TraceWrapperMember& other) {
    Member<T>::operator=(other);
    ScriptWrappableVisitor::WriteBarrier(other);
    return *this;
  }

  TraceWrapperMember& operator=(const Member<T>& other) {
    Member<T>::operator=(other);
    ScriptWrappableVisitor::WriteBarrier(other);
    return *this;
  }

  TraceWrapperMember& operator=(T* other) {
    Member<T>::operator=(other);
    ScriptWrappableVisitor::WriteBarrier(other);
    return *this;
  }

  TraceWrapperMember& operator=(std::nullptr_t) {
    // No need for a write barrier when assigning nullptr.
    Member<T>::operator=(nullptr);
    return *this;
  }
};

// Swaps two HeapVectors specialized for TraceWrapperMember. The custom swap
// function is required as TraceWrapperMember potentially requires emitting a
// write barrier.
template <typename T>
void swap(HeapVector<TraceWrapperMember<T>>& a,
          HeapVector<TraceWrapperMember<T>>& b) {
  // HeapVector<Member<T>> and HeapVector<TraceWrapperMember<T>> have the
  // same size and semantics.
  HeapVector<Member<T>>& a_ = reinterpret_cast<HeapVector<Member<T>>&>(a);
  HeapVector<Member<T>>& b_ = reinterpret_cast<HeapVector<Member<T>>&>(b);
  a_.swap(b_);
  if (ThreadState::Current()->WrapperTracingInProgress()) {
    // If incremental marking is enabled we need to emit the write barrier since
    // the swap was performed on HeapVector<Member<T>>.
    for (auto item : a) {
      ScriptWrappableVisitor::WriteBarrier(item.Get());
    }
    for (auto item : b) {
      ScriptWrappableVisitor::WriteBarrier(item.Get());
    }
  }
}

// Swaps two HeapVectors, one containing TraceWrapperMember and one with
// regular Members. The custom swap function is required as TraceWrapperMember
// potentially requires emitting a write barrier.
template <typename T>
void swap(HeapVector<TraceWrapperMember<T>>& a, HeapVector<Member<T>>& b) {
  // HeapVector<Member<T>> and HeapVector<TraceWrapperMember<T>> have the
  // same size and semantics.
  HeapVector<Member<T>>& a_ = reinterpret_cast<HeapVector<Member<T>>&>(a);
  a_.swap(b);
  if (ThreadState::Current()->WrapperTracingInProgress()) {
    // If incremental marking is enabled we need to emit the write barrier since
    // the swap was performed on HeapVector<Member<T>>.
    for (auto item : a) {
      ScriptWrappableVisitor::WriteBarrier(item.Get());
    }
  }
}

}  // namespace blink

#endif  // TraceWrapperMember_h

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
  TraceWrapperMember() : Member<T>(nullptr) {}

  TraceWrapperMember(T* raw) : Member<T>(raw) {
    // We have to use a write barrier here because of in-place construction
    // in containers, such as HeapVector::push_back.
    ScriptWrappableVisitor::WriteBarrier(raw);
  }

  TraceWrapperMember(WTF::HashTableDeletedValueType x) : Member<T>(x) {}

  TraceWrapperMember(const TraceWrapperMember& other) { *this = other; }

  TraceWrapperMember& operator=(const TraceWrapperMember& other) {
    Member<T>::operator=(other);
    DCHECK_EQ(other.Get(), this->Get());
    ScriptWrappableVisitor::WriteBarrier(this->Get());
    return *this;
  }

  TraceWrapperMember& operator=(const Member<T>& other) {
    Member<T>::operator=(other);
    DCHECK_EQ(other.Get(), this->Get());
    ScriptWrappableVisitor::WriteBarrier(this->Get());
    return *this;
  }

  TraceWrapperMember& operator=(T* other) {
    Member<T>::operator=(other);
    DCHECK_EQ(other, this->Get());
    ScriptWrappableVisitor::WriteBarrier(this->Get());
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

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperMember_h
#define TraceWrapperMember_h

#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class HeapObjectHeader;
template <typename T>
class Member;

/**
 * TraceWrapperMember is used for Member fields that should participate in
 * wrapper tracing, i.e., strongly hold a ScriptWrappable alive. All
 * TraceWrapperMember fields must be traced in the class' traceWrappers method.
 */
template <class T>
class TraceWrapperMember : public Member<T> {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  TraceWrapperMember(void* parent, T* raw) : Member<T>(raw), parent_(parent) {
#if DCHECK_IS_ON()
    if (parent_) {
      HeapObjectHeader::CheckFromPayload(parent_);
    }
#endif
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
  TraceWrapperMember(WTF::HashTableDeletedValueType x)
      : Member<T>(x), parent_(nullptr) {}

  /**
   * Copying a TraceWrapperMember means that its backpointer will also be
   * copied.
   */
  TraceWrapperMember(const TraceWrapperMember& other) { *this = other; }

  TraceWrapperMember& operator=(const TraceWrapperMember& other) {
    DCHECK(!other.raw_ || other.parent_);
    parent_ = other.parent_;
    Member<T>::operator=(other);
    ScriptWrappableVisitor::WriteBarrier(parent_, other);
    return *this;
  }

  TraceWrapperMember& operator=(const Member<T>& other) {
    DCHECK(!TraceWrapperMemberIsNotInitialized());
    Member<T>::operator=(other);
    ScriptWrappableVisitor::WriteBarrier(parent_, other);
    return *this;
  }

  TraceWrapperMember& operator=(T* other) {
    DCHECK(!TraceWrapperMemberIsNotInitialized());
    Member<T>::operator=(other);
    ScriptWrappableVisitor::WriteBarrier(parent_, other);
    return *this;
  }

  TraceWrapperMember& operator=(std::nullptr_t) {
    // No need for a write barrier when assigning nullptr.
    Member<T>::operator=(nullptr);
    return *this;
  }

  void* Parent() { return parent_; }

 private:
  bool TraceWrapperMemberIsNotInitialized() { return !parent_; }

  /**
   * The parent object holding strongly onto the actual Member.
   */
  void* parent_;
};

/**
 * Swaps two HeapVectors specialized for TraceWrapperMember. The custom swap
 * function is required as TraceWrapperMember contains ownership information
 * which is not copyable but has to be explicitly specified.
 */
template <typename T>
void swap(HeapVector<TraceWrapperMember<T>>& a,
          HeapVector<TraceWrapperMember<T>>& b,
          void* parent_for_a,
          void* parent_for_b) {
  HeapVector<TraceWrapperMember<T>> temp;
  temp.ReserveCapacity(a.size());
  for (auto item : a) {
    temp.push_back(TraceWrapperMember<T>(parent_for_b, item.Get()));
  }
  a.Clear();
  a.ReserveCapacity(b.size());
  for (auto item : b) {
    a.push_back(TraceWrapperMember<T>(parent_for_a, item.Get()));
  }
  b.Clear();
  b.ReserveCapacity(temp.size());
  for (auto item : temp) {
    b.push_back(TraceWrapperMember<T>(parent_for_b, item.Get()));
  }
}

/**
 * Swaps two HeapVectors, one containing TraceWrapperMember and one with
 * regular Members. The custom swap function is required as
 * TraceWrapperMember contains ownership information which is not copyable
 * but has to be explicitly specified.
 */
template <typename T>
void swap(HeapVector<TraceWrapperMember<T>>& a,
          HeapVector<Member<T>>& b,
          void* parent_for_a) {
  HeapVector<TraceWrapperMember<T>> temp;
  temp.ReserveCapacity(a.size());
  for (auto item : a) {
    temp.push_back(TraceWrapperMember<T>(item.Parent(), item.Get()));
  }
  a.Clear();
  a.ReserveCapacity(b.size());
  for (auto item : b) {
    a.push_back(TraceWrapperMember<T>(parent_for_a, item.Get()));
  }
  b.Clear();
  b.ReserveCapacity(temp.size());
  for (auto item : temp) {
    b.push_back(item.Get());
  }
}

}  // namespace blink

#endif  // TraceWrapperMember_h

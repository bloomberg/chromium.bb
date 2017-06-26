// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapTerminatedArray_h
#define HeapTerminatedArray_h

#include "platform/heap/Heap.h"
#include "platform/wtf/TerminatedArray.h"
#include "platform/wtf/TerminatedArrayBuilder.h"
#include "platform/wtf/allocator/Partitions.h"

namespace blink {

template <typename T>
class HeapTerminatedArray : public TerminatedArray<T> {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  using TerminatedArray<T>::begin;
  using TerminatedArray<T>::end;

  DEFINE_INLINE_TRACE() {
    for (typename TerminatedArray<T>::iterator it = begin(); it != end(); ++it)
      visitor->Trace(*it);
  }

 private:
  // Allocator describes how HeapTerminatedArrayBuilder should create new
  // instances of HeapTerminatedArray and manage their lifetimes.
  struct Allocator final {
    STATIC_ONLY(Allocator);
    using PassPtr = HeapTerminatedArray*;
    using Ptr = Member<HeapTerminatedArray>;

    static PassPtr Release(Ptr& ptr) { return ptr; }

    static PassPtr Create(size_t capacity) {
      return reinterpret_cast<HeapTerminatedArray*>(
          ThreadHeap::Allocate<HeapTerminatedArray>(
              WTF::Partitions::ComputeAllocationSize(capacity, sizeof(T)),
              IsEagerlyFinalizedType<T>::value));
    }

    static PassPtr Resize(PassPtr ptr, size_t capacity) {
      return reinterpret_cast<HeapTerminatedArray*>(
          ThreadHeap::Reallocate<HeapTerminatedArray>(
              ptr,
              WTF::Partitions::ComputeAllocationSize(capacity, sizeof(T))));
    }
  };

  // Prohibit construction. Allocator makes HeapTerminatedArray instances for
  // HeapTerminatedArrayBuilder by pointer casting.
  HeapTerminatedArray();

  template <typename U, template <typename> class>
  friend class WTF::TerminatedArrayBuilder;
};

template <typename T>
class TraceEagerlyTrait<HeapTerminatedArray<T>> {
 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

}  // namespace blink

#endif  // HeapTerminatedArray_h

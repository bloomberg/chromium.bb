// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ProcessHeap_h
#define ProcessHeap_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Atomics.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace blink {

class CrossThreadPersistentRegion;

class PLATFORM_EXPORT ProcessHeap {
  STATIC_ONLY(ProcessHeap);

 public:
  static void Init();

  static CrossThreadPersistentRegion& GetCrossThreadPersistentRegion();
  static CrossThreadPersistentRegion& GetCrossThreadWeakPersistentRegion();

  // Recursive as prepareForThreadStateTermination() clears a PersistentNode's
  // associated Persistent<> -- it in turn freeing the PersistentNode. And both
  // CrossThreadPersistentRegion operations need a lock on the region before
  // mutating.
  static RecursiveMutex& CrossThreadPersistentMutex();

  static void IncreaseTotalAllocatedObjectSize(size_t delta) {
    AtomicAdd(&total_allocated_object_size_, static_cast<long>(delta));
  }
  static void DecreaseTotalAllocatedObjectSize(size_t delta) {
    AtomicSubtract(&total_allocated_object_size_, static_cast<long>(delta));
  }
  static size_t TotalAllocatedObjectSize() {
    return AcquireLoad(&total_allocated_object_size_);
  }
  static void IncreaseTotalMarkedObjectSize(size_t delta) {
    AtomicAdd(&total_marked_object_size_, static_cast<long>(delta));
  }
  static void DecreaseTotalMarkedObjectSize(size_t delta) {
    AtomicSubtract(&total_marked_object_size_, static_cast<long>(delta));
  }
  static size_t TotalMarkedObjectSize() {
    return AcquireLoad(&total_marked_object_size_);
  }
  static void IncreaseTotalAllocatedSpace(size_t delta) {
    AtomicAdd(&total_allocated_space_, static_cast<long>(delta));
  }
  static void DecreaseTotalAllocatedSpace(size_t delta) {
    AtomicSubtract(&total_allocated_space_, static_cast<long>(delta));
  }
  static size_t TotalAllocatedSpace() {
    return AcquireLoad(&total_allocated_space_);
  }
  static void ResetHeapCounters();

 private:
  static size_t total_allocated_space_;
  static size_t total_allocated_object_size_;
  static size_t total_marked_object_size_;

  friend class ThreadState;
};

}  // namespace blink

#endif

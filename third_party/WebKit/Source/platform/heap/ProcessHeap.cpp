// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/ProcessHeap.h"

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/Heap.h"
#include "platform/heap/PersistentNode.h"
#include "platform/wtf/Assertions.h"

namespace blink {

namespace {

void BlinkGCAllocHook(uint8_t* address, size_t size, const char*) {
  base::SamplingHeapProfiler::RecordAlloc(address, size);
}

void BlinkGCFreeHook(uint8_t* address) {
  base::SamplingHeapProfiler::RecordFree(address);
}

}  // namespace

void ProcessHeap::Init() {
  total_allocated_space_ = 0;
  total_allocated_object_size_ = 0;
  total_marked_object_size_ = 0;

  GCInfoTable::Init();

  base::SamplingHeapProfiler::SetHooksInstallCallback([]() {
    HeapAllocHooks::SetAllocationHook(&BlinkGCAllocHook);
    HeapAllocHooks::SetFreeHook(&BlinkGCFreeHook);
  });
}

void ProcessHeap::ResetHeapCounters() {
  total_allocated_object_size_ = 0;
  total_marked_object_size_ = 0;
}

CrossThreadPersistentRegion& ProcessHeap::GetCrossThreadPersistentRegion() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion,
                                  persistent_region, ());
  return persistent_region;
}

CrossThreadPersistentRegion& ProcessHeap::GetCrossThreadWeakPersistentRegion() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion,
                                  persistent_region, ());
  return persistent_region;
}

RecursiveMutex& ProcessHeap::CrossThreadPersistentMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(RecursiveMutex, mutex, ());
  return mutex;
}

size_t ProcessHeap::total_allocated_space_ = 0;
size_t ProcessHeap::total_allocated_object_size_ = 0;
size_t ProcessHeap::total_marked_object_size_ = 0;

}  // namespace blink

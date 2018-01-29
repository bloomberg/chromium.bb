// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/ProcessHeap.h"

#include "platform/heap/CallbackStack.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/PersistentNode.h"
#include "platform/heap/VisitorImpl.h"
#include "platform/wtf/Assertions.h"

namespace blink {

void ProcessHeap::Init() {
  total_allocated_space_ = 0;
  total_allocated_object_size_ = 0;
  total_marked_object_size_ = 0;

  GCInfoTable::Init();
  CallbackStackMemoryPool::Instance().Initialize();
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

size_t ProcessHeap::total_allocated_space_ = 0;
size_t ProcessHeap::total_allocated_object_size_ = 0;
size_t ProcessHeap::total_marked_object_size_ = 0;

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMemoryStatistics.h"

#include "platform/heap/Handle.h"
#include "platform/wtf/allocator/Partitions.h"

namespace blink {

WebMemoryStatistics WebMemoryStatistics::Get() {
  WebMemoryStatistics statistics;
  statistics.partition_alloc_total_allocated_bytes =
      WTF::Partitions::TotalActiveBytes();
  statistics.blink_gc_total_allocated_bytes =
      ProcessHeap::TotalAllocatedObjectSize() +
      ProcessHeap::TotalMarkedObjectSize();
  return statistics;
}

}  // namespace blink

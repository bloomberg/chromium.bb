// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PARTITION_ALLOC_MEMORY_DUMP_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PARTITION_ALLOC_MEMORY_DUMP_PROVIDER_H_

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/sharded_allocation_register.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace blink {

class BLINK_PLATFORM_EXPORT PartitionAllocMemoryDumpProvider final
    : public base::trace_event::MemoryDumpProvider {
  // TODO(tasak): PartitionAllocMemoryDumpProvider should be
  // USING_FAST_MALLOC. c.f. crbug.com/584196
  WTF_MAKE_NONCOPYABLE(PartitionAllocMemoryDumpProvider);

 public:
  static PartitionAllocMemoryDumpProvider* Instance();
  ~PartitionAllocMemoryDumpProvider() override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;
  void OnHeapProfilingEnabled(bool) override;

  // These methods are called only from PartitionAllocHooks' callbacks.
  void insert(void*, size_t, const char*);
  void Remove(void*);

 private:
  PartitionAllocMemoryDumpProvider();

  base::trace_event::ShardedAllocationRegister allocation_register_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PARTITION_ALLOC_MEMORY_DUMP_PROVIDER_H_

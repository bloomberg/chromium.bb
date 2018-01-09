// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstanceCountersMemoryDumpProvider_h
#define InstanceCountersMemoryDumpProvider_h

#include "base/trace_event/memory_dump_provider.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT InstanceCountersMemoryDumpProvider final
    : public base::trace_event::MemoryDumpProvider {
  USING_FAST_MALLOC(InstanceCountersMemoryDumpProvider);

 public:
  static InstanceCountersMemoryDumpProvider* Instance();
  ~InstanceCountersMemoryDumpProvider() override = default;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;

 private:
  InstanceCountersMemoryDumpProvider() = default;

  WTF_MAKE_NONCOPYABLE(InstanceCountersMemoryDumpProvider);
};

}  // namespace blink

#endif  // InstanceCountersMemoryDumpProvider_h

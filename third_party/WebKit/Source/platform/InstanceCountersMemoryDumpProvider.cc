// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/InstanceCountersMemoryDumpProvider.h"

#include "base/trace_event/process_memory_dump.h"
#include "platform/InstanceCounters.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

InstanceCountersMemoryDumpProvider*
InstanceCountersMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(InstanceCountersMemoryDumpProvider, instance, ());
  return &instance;
}

bool InstanceCountersMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  using base::trace_event::MemoryAllocatorDump;
#define DUMP_COUNTER(CounterType)                                     \
  memory_dump->CreateAllocatorDump("counter/" #CounterType)           \
      ->AddScalar("object_count", MemoryAllocatorDump::kUnitsObjects, \
                  InstanceCounters::CounterValue(                     \
                      InstanceCounters::k##CounterType##Counter));
  INSTANCE_COUNTERS_LIST(DUMP_COUNTER)
  return true;
}

}  // namespace blink

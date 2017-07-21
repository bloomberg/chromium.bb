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
#define DUMP_COUNTER(CounterType)                                            \
  {                                                                          \
    MemoryAllocatorDump* counter_dump =                                      \
        memory_dump->CreateAllocatorDump("counter/" #CounterType);           \
    counter_dump->AddScalar("object_count",                                  \
                            MemoryAllocatorDump::kUnitsObjects,              \
                            InstanceCounters::CounterValue(                  \
                                InstanceCounters::k##CounterType##Counter)); \
    break;                                                                   \
  }

  for (int i = 0; i < InstanceCounters::kCounterTypeLength; ++i) {
    InstanceCounters::CounterType type =
        static_cast<InstanceCounters::CounterType>(i);
    switch (type) {
      case InstanceCounters::kAudioHandlerCounter:
        DUMP_COUNTER(AudioHandler)
      case InstanceCounters::kDocumentCounter:
        DUMP_COUNTER(Document)
      case InstanceCounters::kFrameCounter:
        DUMP_COUNTER(Frame)
      case InstanceCounters::kJSEventListenerCounter:
        DUMP_COUNTER(JSEventListener)
      case InstanceCounters::kLayoutObjectCounter:
        DUMP_COUNTER(LayoutObject)
      case InstanceCounters::kMediaKeySessionCounter:
        DUMP_COUNTER(MediaKeySession)
      case InstanceCounters::kMediaKeysCounter:
        DUMP_COUNTER(MediaKeys)
      case InstanceCounters::kNodeCounter:
        DUMP_COUNTER(Node)
      case InstanceCounters::kResourceCounter:
        DUMP_COUNTER(Resource)
      case InstanceCounters::kScriptPromiseCounter:
        DUMP_COUNTER(ScriptPromise)
      case InstanceCounters::kSuspendableObjectCounter:
        DUMP_COUNTER(SuspendableObject)
      case InstanceCounters::kV8PerContextDataCounter:
        DUMP_COUNTER(V8PerContextData)
      case InstanceCounters::kWorkerGlobalScopeCounter:
        DUMP_COUNTER(WorkerGlobalScope)
      case InstanceCounters::kCounterTypeLength:
        break;
    }
  }
  return true;
}

}  // namespace blink

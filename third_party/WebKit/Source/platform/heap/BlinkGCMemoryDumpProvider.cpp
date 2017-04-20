// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/BlinkGCMemoryDumpProvider.h"

#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Threading.h"
#include "public/platform/Platform.h"

namespace blink {
namespace {

void DumpMemoryTotals(base::trace_event::ProcessMemoryDump* memory_dump) {
  base::trace_event::MemoryAllocatorDump* allocator_dump =
      memory_dump->CreateAllocatorDump("blink_gc");
  allocator_dump->AddScalar("size", "bytes",
                            ProcessHeap::TotalAllocatedSpace());

  base::trace_event::MemoryAllocatorDump* objects_dump =
      memory_dump->CreateAllocatorDump("blink_gc/allocated_objects");

  // ThreadHeap::markedObjectSize() can be underestimated if we're still in the
  // process of lazy sweeping.
  objects_dump->AddScalar("size", "bytes",
                          ProcessHeap::TotalAllocatedObjectSize() +
                              ProcessHeap::TotalMarkedObjectSize());
}

void ReportAllocation(Address address, size_t size, const char* type_name) {
  BlinkGCMemoryDumpProvider::Instance()->insert(address, size, type_name);
}

void ReportFree(Address address) {
  BlinkGCMemoryDumpProvider::Instance()->Remove(address);
}

}  // namespace

BlinkGCMemoryDumpProvider* BlinkGCMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(BlinkGCMemoryDumpProvider, instance, ());
  return &instance;
}

BlinkGCMemoryDumpProvider::~BlinkGCMemoryDumpProvider() {}

bool BlinkGCMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  using base::trace_event::MemoryDumpLevelOfDetail;
  MemoryDumpLevelOfDetail level_of_detail = args.level_of_detail;
  // In the case of a detailed dump perform a mark-only GC pass to collect
  // more detailed stats.
  if (level_of_detail == MemoryDumpLevelOfDetail::DETAILED)
    ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                           BlinkGC::kTakeSnapshot,
                                           BlinkGC::kForcedGC);
  DumpMemoryTotals(memory_dump);

  if (is_heap_profiling_enabled_) {
    // Overhead should always be reported, regardless of light vs. heavy.
    base::trace_event::TraceEventMemoryOverhead overhead;
    base::hash_map<base::trace_event::AllocationContext,
                   base::trace_event::AllocationMetrics>
        metrics_by_context;
    {
      MutexLocker locker(allocation_register_mutex_);
      if (level_of_detail == MemoryDumpLevelOfDetail::DETAILED) {
        for (const auto& alloc_size : *allocation_register_) {
          base::trace_event::AllocationMetrics& metrics =
              metrics_by_context[alloc_size.context];
          metrics.size += alloc_size.size;
          metrics.count++;
        }
      }
      allocation_register_->EstimateTraceMemoryOverhead(&overhead);
    }
    memory_dump->DumpHeapUsage(metrics_by_context, overhead, "blink_gc");
  }

  // Merge all dumps collected by ThreadHeap::collectGarbage.
  if (level_of_detail == MemoryDumpLevelOfDetail::DETAILED)
    memory_dump->TakeAllDumpsFrom(current_process_memory_dump_.get());
  return true;
}

void BlinkGCMemoryDumpProvider::OnHeapProfilingEnabled(bool enabled) {
  if (enabled) {
    {
      MutexLocker locker(allocation_register_mutex_);
      if (!allocation_register_)
        allocation_register_.reset(new base::trace_event::AllocationRegister());
    }
    HeapAllocHooks::SetAllocationHook(ReportAllocation);
    HeapAllocHooks::SetFreeHook(ReportFree);
  } else {
    HeapAllocHooks::SetAllocationHook(nullptr);
    HeapAllocHooks::SetFreeHook(nullptr);
  }
  is_heap_profiling_enabled_ = enabled;
}

base::trace_event::MemoryAllocatorDump*
BlinkGCMemoryDumpProvider::CreateMemoryAllocatorDumpForCurrentGC(
    const String& absolute_name) {
  // TODO(bashi): Change type name of |absoluteName|.
  return current_process_memory_dump_->CreateAllocatorDump(
      absolute_name.Utf8().data());
}

void BlinkGCMemoryDumpProvider::ClearProcessDumpForCurrentGC() {
  current_process_memory_dump_->Clear();
}

BlinkGCMemoryDumpProvider::BlinkGCMemoryDumpProvider()
    : current_process_memory_dump_(new base::trace_event::ProcessMemoryDump(
          nullptr,
          {base::trace_event::MemoryDumpLevelOfDetail::DETAILED})),
      is_heap_profiling_enabled_(false) {}

void BlinkGCMemoryDumpProvider::insert(Address address,
                                       size_t size,
                                       const char* type_name) {
  base::trace_event::AllocationContext context;
  if (!base::trace_event::AllocationContextTracker::
           GetInstanceForCurrentThread()
               ->GetContextSnapshot(&context))
    return;
  context.type_name = type_name;
  MutexLocker locker(allocation_register_mutex_);
  if (allocation_register_)
    allocation_register_->Insert(address, size, context);
}

void BlinkGCMemoryDumpProvider::Remove(Address address) {
  MutexLocker locker(allocation_register_mutex_);
  if (allocation_register_)
    allocation_register_->Remove(address);
}

}  // namespace blink

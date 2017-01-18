// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/PartitionAllocMemoryDumpProvider.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/heap_profiler_allocation_context.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

using namespace WTF;

void reportAllocation(void* address, size_t size, const char* typeName) {
  PartitionAllocMemoryDumpProvider::instance()->insert(address, size, typeName);
}

void reportFree(void* address) {
  PartitionAllocMemoryDumpProvider::instance()->remove(address);
}

const char kPartitionAllocDumpName[] = "partition_alloc";
const char kPartitionsDumpName[] = "partitions";

std::string getPartitionDumpName(const char* partitionName) {
  return base::StringPrintf("%s/%s/%s", kPartitionAllocDumpName,
                            kPartitionsDumpName, partitionName);
}

// This class is used to invert the dependency of PartitionAlloc on the
// PartitionAllocMemoryDumpProvider. This implements an interface that will
// be called with memory statistics for each bucket in the allocator.
class PartitionStatsDumperImpl final : public WTF::PartitionStatsDumper {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(PartitionStatsDumperImpl);

 public:
  PartitionStatsDumperImpl(
      base::trace_event::ProcessMemoryDump* memoryDump,
      base::trace_event::MemoryDumpLevelOfDetail levelOfDetail)
      : m_memoryDump(memoryDump), m_uid(0), m_totalActiveBytes(0) {}

  // PartitionStatsDumper implementation.
  void PartitionDumpTotals(const char* partitionName,
                           const WTF::PartitionMemoryStats*) override;
  void PartitionsDumpBucketStats(
      const char* partitionName,
      const WTF::PartitionBucketMemoryStats*) override;

  size_t TotalActiveBytes() const { return m_totalActiveBytes; }

 private:
  base::trace_event::ProcessMemoryDump* m_memoryDump;
  unsigned long m_uid;
  size_t m_totalActiveBytes;
};

void PartitionStatsDumperImpl::PartitionDumpTotals(
    const char* partitionName,
    const WTF::PartitionMemoryStats* memoryStats) {
  m_totalActiveBytes += memoryStats->total_active_bytes;
  std::string dumpName = getPartitionDumpName(partitionName);
  base::trace_event::MemoryAllocatorDump* allocatorDump =
      m_memoryDump->CreateAllocatorDump(dumpName);
  allocatorDump->AddScalar("size", "bytes", memoryStats->total_resident_bytes);
  allocatorDump->AddScalar("allocated_objects_size", "bytes",
                           memoryStats->total_active_bytes);
  allocatorDump->AddScalar("virtual_size", "bytes",
                           memoryStats->total_mmapped_bytes);
  allocatorDump->AddScalar("virtual_committed_size", "bytes",
                           memoryStats->total_committed_bytes);
  allocatorDump->AddScalar("decommittable_size", "bytes",
                           memoryStats->total_decommittable_bytes);
  allocatorDump->AddScalar("discardable_size", "bytes",
                           memoryStats->total_discardable_bytes);
}

void PartitionStatsDumperImpl::PartitionsDumpBucketStats(
    const char* partitionName,
    const WTF::PartitionBucketMemoryStats* memoryStats) {
  DCHECK(memoryStats->is_valid);
  std::string dumpName = getPartitionDumpName(partitionName);
  if (memoryStats->is_direct_map) {
    dumpName.append(base::StringPrintf("/directMap_%lu", ++m_uid));
  } else {
    dumpName.append(base::StringPrintf(
        "/bucket_%u", static_cast<unsigned>(memoryStats->bucket_slot_size)));
  }

  base::trace_event::MemoryAllocatorDump* allocatorDump =
      m_memoryDump->CreateAllocatorDump(dumpName);
  allocatorDump->AddScalar("size", "bytes", memoryStats->resident_bytes);
  allocatorDump->AddScalar("allocated_objects_size", "bytes",
                           memoryStats->active_bytes);
  allocatorDump->AddScalar("slot_size", "bytes", memoryStats->bucket_slot_size);
  allocatorDump->AddScalar("decommittable_size", "bytes",
                           memoryStats->decommittable_bytes);
  allocatorDump->AddScalar("discardable_size", "bytes",
                           memoryStats->discardable_bytes);
  allocatorDump->AddScalar("total_pages_size", "bytes",
                           memoryStats->allocated_page_size);
  allocatorDump->AddScalar("active_pages", "objects",
                           memoryStats->num_active_pages);
  allocatorDump->AddScalar("full_pages", "objects",
                           memoryStats->num_full_pages);
  allocatorDump->AddScalar("empty_pages", "objects",
                           memoryStats->num_empty_pages);
  allocatorDump->AddScalar("decommitted_pages", "objects",
                           memoryStats->num_decommitted_pages);
}

}  // namespace

PartitionAllocMemoryDumpProvider* PartitionAllocMemoryDumpProvider::instance() {
  DEFINE_STATIC_LOCAL(PartitionAllocMemoryDumpProvider, instance, ());
  return &instance;
}

bool PartitionAllocMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memoryDump) {
  using base::trace_event::MemoryDumpLevelOfDetail;

  MemoryDumpLevelOfDetail levelOfDetail = args.level_of_detail;
  if (m_isHeapProfilingEnabled) {
    // Overhead should always be reported, regardless of light vs. heavy.
    base::trace_event::TraceEventMemoryOverhead overhead;
    base::hash_map<base::trace_event::AllocationContext,
                   base::trace_event::AllocationMetrics>
        metricsByContext;
    {
      MutexLocker locker(m_allocationRegisterMutex);
      // Dump only the overhead estimation in non-detailed dumps.
      if (levelOfDetail == MemoryDumpLevelOfDetail::DETAILED) {
        for (const auto& allocSize : *m_allocationRegister) {
          base::trace_event::AllocationMetrics& metrics =
              metricsByContext[allocSize.context];
          metrics.size += allocSize.size;
          metrics.count++;
        }
      }
      m_allocationRegister->EstimateTraceMemoryOverhead(&overhead);
    }
    memoryDump->DumpHeapUsage(metricsByContext, overhead, "partition_alloc");
  }

  PartitionStatsDumperImpl partitionStatsDumper(memoryDump, levelOfDetail);

  base::trace_event::MemoryAllocatorDump* partitionsDump =
      memoryDump->CreateAllocatorDump(base::StringPrintf(
          "%s/%s", kPartitionAllocDumpName, kPartitionsDumpName));

  // This method calls memoryStats.partitionsDumpBucketStats with memory
  // statistics.
  WTF::Partitions::dumpMemoryStats(
      levelOfDetail != MemoryDumpLevelOfDetail::DETAILED,
      &partitionStatsDumper);

  base::trace_event::MemoryAllocatorDump* allocatedObjectsDump =
      memoryDump->CreateAllocatorDump(Partitions::kAllocatedObjectPoolName);
  allocatedObjectsDump->AddScalar("size", "bytes",
                                  partitionStatsDumper.TotalActiveBytes());
  memoryDump->AddOwnershipEdge(allocatedObjectsDump->guid(),
                               partitionsDump->guid());

  return true;
}

// |m_allocationRegister| should be initialized only when necessary to avoid
// waste of memory.
PartitionAllocMemoryDumpProvider::PartitionAllocMemoryDumpProvider()
    : m_allocationRegister(nullptr), m_isHeapProfilingEnabled(false) {}

PartitionAllocMemoryDumpProvider::~PartitionAllocMemoryDumpProvider() {}

void PartitionAllocMemoryDumpProvider::OnHeapProfilingEnabled(bool enabled) {
  if (enabled) {
    {
      MutexLocker locker(m_allocationRegisterMutex);
      if (!m_allocationRegister)
        m_allocationRegister.reset(new base::trace_event::AllocationRegister());
    }
    WTF::PartitionAllocHooks::SetAllocationHook(reportAllocation);
    WTF::PartitionAllocHooks::SetFreeHook(reportFree);
  } else {
    WTF::PartitionAllocHooks::SetAllocationHook(nullptr);
    WTF::PartitionAllocHooks::SetFreeHook(nullptr);
  }
  m_isHeapProfilingEnabled = enabled;
}

void PartitionAllocMemoryDumpProvider::insert(void* address,
                                              size_t size,
                                              const char* typeName) {
  base::trace_event::AllocationContext context;
  if (!base::trace_event::AllocationContextTracker::
           GetInstanceForCurrentThread()
               ->GetContextSnapshot(&context))
    return;

  context.type_name = typeName;
  MutexLocker locker(m_allocationRegisterMutex);
  if (m_allocationRegister)
    m_allocationRegister->Insert(address, size, context);
}

void PartitionAllocMemoryDumpProvider::remove(void* address) {
  MutexLocker locker(m_allocationRegisterMutex);
  if (m_allocationRegister)
    m_allocationRegister->Remove(address);
}

}  // namespace blink

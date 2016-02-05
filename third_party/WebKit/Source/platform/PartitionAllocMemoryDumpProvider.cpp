// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/PartitionAllocMemoryDumpProvider.h"

#include "base/trace_event/heap_profiler_allocation_context.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/Partitions.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

using namespace WTF;

void reportAllocation(void* address, size_t size, const char* typeName)
{
    PartitionAllocMemoryDumpProvider::instance()->insert(address, size, typeName);
}

void reportFree(void* address)
{
    PartitionAllocMemoryDumpProvider::instance()->remove(address);
}

const char kPartitionAllocDumpName[] = "partition_alloc";
const char kPartitionsDumpName[] = "partitions";

String getPartitionDumpName(const char* partitionName)
{
    return String::format("%s/%s/%s", kPartitionAllocDumpName, kPartitionsDumpName, partitionName);
}

// This class is used to invert the dependency of PartitionAlloc on the
// PartitionAllocMemoryDumpProvider. This implements an interface that will
// be called with memory statistics for each bucket in the allocator.
class PartitionStatsDumperImpl final : public PartitionStatsDumper {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(PartitionStatsDumperImpl);
public:
    PartitionStatsDumperImpl(WebProcessMemoryDump* memoryDump, WebMemoryDumpLevelOfDetail levelOfDetail)
        : m_memoryDump(memoryDump)
        , m_uid(0)
        , m_totalActiveBytes(0)
    {
    }

    // PartitionStatsDumper implementation.
    void partitionDumpTotals(const char* partitionName, const PartitionMemoryStats*) override;
    void partitionsDumpBucketStats(const char* partitionName, const PartitionBucketMemoryStats*) override;

    size_t totalActiveBytes() const { return m_totalActiveBytes; }

private:
    WebProcessMemoryDump* m_memoryDump;
    unsigned long m_uid;
    size_t m_totalActiveBytes;
};

void PartitionStatsDumperImpl::partitionDumpTotals(const char* partitionName, const PartitionMemoryStats* memoryStats)
{
    m_totalActiveBytes += memoryStats->totalActiveBytes;
    String dumpName = getPartitionDumpName(partitionName);
    WebMemoryAllocatorDump* allocatorDump = m_memoryDump->createMemoryAllocatorDump(dumpName);
    allocatorDump->addScalar("size", "bytes", memoryStats->totalResidentBytes);
    allocatorDump->addScalar("allocated_objects_size", "bytes", memoryStats->totalActiveBytes);
    allocatorDump->addScalar("virtual_size", "bytes", memoryStats->totalMmappedBytes);
    allocatorDump->addScalar("virtual_committed_size", "bytes", memoryStats->totalCommittedBytes);
    allocatorDump->addScalar("decommittable_size", "bytes", memoryStats->totalDecommittableBytes);
    allocatorDump->addScalar("discardable_size", "bytes", memoryStats->totalDiscardableBytes);
}

void PartitionStatsDumperImpl::partitionsDumpBucketStats(const char* partitionName, const PartitionBucketMemoryStats* memoryStats)
{
    ASSERT(memoryStats->isValid);
    String dumpName = getPartitionDumpName(partitionName);
    if (memoryStats->isDirectMap)
        dumpName.append(String::format("/directMap_%lu", ++m_uid));
    else
        dumpName.append(String::format("/bucket_%u", static_cast<unsigned>(memoryStats->bucketSlotSize)));

    WebMemoryAllocatorDump* allocatorDump = m_memoryDump->createMemoryAllocatorDump(dumpName);
    allocatorDump->addScalar("size", "bytes", memoryStats->residentBytes);
    allocatorDump->addScalar("allocated_objects_size", "bytes", memoryStats->activeBytes);
    allocatorDump->addScalar("slot_size", "bytes", memoryStats->bucketSlotSize);
    allocatorDump->addScalar("decommittable_size", "bytes", memoryStats->decommittableBytes);
    allocatorDump->addScalar("discardable_size", "bytes", memoryStats->discardableBytes);
    allocatorDump->addScalar("total_pages_size", "bytes", memoryStats->allocatedPageSize);
    allocatorDump->addScalar("active_pages", "objects", memoryStats->numActivePages);
    allocatorDump->addScalar("full_pages", "objects", memoryStats->numFullPages);
    allocatorDump->addScalar("empty_pages", "objects", memoryStats->numEmptyPages);
    allocatorDump->addScalar("decommitted_pages", "objects", memoryStats->numDecommittedPages);
}

} // namespace

PartitionAllocMemoryDumpProvider* PartitionAllocMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(PartitionAllocMemoryDumpProvider, instance, ());
    return &instance;
}

bool PartitionAllocMemoryDumpProvider::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail, WebProcessMemoryDump* memoryDump)
{
    if (levelOfDetail == WebMemoryDumpLevelOfDetail::Detailed && m_isHeapProfilingEnabled) {
        base::trace_event::TraceEventMemoryOverhead overhead;
        base::hash_map<base::trace_event::AllocationContext, size_t> bytesByContext;
        {
            MutexLocker locker(m_allocationRegisterMutex);
            for (const auto& allocSize : *m_allocationRegister)
                bytesByContext[allocSize.context] += allocSize.size;

            m_allocationRegister->EstimateTraceMemoryOverhead(&overhead);
        }
        memoryDump->dumpHeapUsage(bytesByContext, overhead, "partition_alloc");
    }

    PartitionStatsDumperImpl partitionStatsDumper(memoryDump, levelOfDetail);

    WebMemoryAllocatorDump* partitionsDump = memoryDump->createMemoryAllocatorDump(
        String::format("%s/%s", kPartitionAllocDumpName, kPartitionsDumpName));

    // This method calls memoryStats.partitionsDumpBucketStats with memory statistics.
    WTF::Partitions::dumpMemoryStats(levelOfDetail == WebMemoryDumpLevelOfDetail::Light, &partitionStatsDumper);

    WebMemoryAllocatorDump* allocatedObjectsDump = memoryDump->createMemoryAllocatorDump(String(Partitions::kAllocatedObjectPoolName));
    allocatedObjectsDump->addScalar("size", "bytes", partitionStatsDumper.totalActiveBytes());
    memoryDump->addOwnershipEdge(allocatedObjectsDump->guid(), partitionsDump->guid());

    return true;
}

// |m_allocationRegister| should be initialized only when necessary to avoid waste of memory.
PartitionAllocMemoryDumpProvider::PartitionAllocMemoryDumpProvider()
    : m_allocationRegister(nullptr)
    , m_isHeapProfilingEnabled(false)
{
}

PartitionAllocMemoryDumpProvider::~PartitionAllocMemoryDumpProvider()
{
}

void PartitionAllocMemoryDumpProvider::onHeapProfilingEnabled(bool enabled)
{
    if (enabled) {
        {
            MutexLocker locker(m_allocationRegisterMutex);
            if (!m_allocationRegister)
                m_allocationRegister = adoptPtr(new base::trace_event::AllocationRegister());
        }
        PartitionAllocHooks::setAllocationHook(reportAllocation);
        PartitionAllocHooks::setFreeHook(reportFree);
    } else {
        PartitionAllocHooks::setAllocationHook(nullptr);
        PartitionAllocHooks::setFreeHook(nullptr);
    }
    m_isHeapProfilingEnabled = enabled;
}

void PartitionAllocMemoryDumpProvider::insert(void* address, size_t size, const char* typeName)
{
    base::trace_event::AllocationContext context = base::trace_event::AllocationContextTracker::GetContextSnapshot();
    context.type_name = typeName;
    MutexLocker locker(m_allocationRegisterMutex);
    if (m_allocationRegister)
        m_allocationRegister->Insert(address, size, context);
}

void PartitionAllocMemoryDumpProvider::remove(void* address)
{
    MutexLocker locker(m_allocationRegisterMutex);
    if (m_allocationRegister)
        m_allocationRegister->Remove(address);
}

} // namespace blink

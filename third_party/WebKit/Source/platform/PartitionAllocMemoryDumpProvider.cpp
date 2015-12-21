// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/PartitionAllocMemoryDumpProvider.h"

#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/Partitions.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

using namespace WTF;

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

PartitionAllocMemoryDumpProvider::PartitionAllocMemoryDumpProvider()
{
}

PartitionAllocMemoryDumpProvider::~PartitionAllocMemoryDumpProvider()
{
}

void PartitionAllocMemoryDumpProvider::onHeapProfilingEnabled(AllocationHook* allocationHook, FreeHook* freeHook)
{
    // Make PartitionAlloc call |allocationHook| and |freeHook| for every
    // subsequent allocation and free (or not if the pointers are null).
    PartitionAllocHooks::setAllocationHook(allocationHook);
    PartitionAllocHooks::setFreeHook(freeHook);
}

} // namespace blink

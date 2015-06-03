// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Source/platform/PartitionAllocMemoryDumpProvider.h"

#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/Partitions.h"

namespace blink {

namespace {

using namespace WTF;

// This class is used to invert the dependency of PartitionAlloc on the
// PartitionAllocMemoryDumpProvider. This implements an interface that will
// be called with memory statistics for each bucket in the allocator.
class PartitionStatsDumperImpl final : public PartitionStatsDumper {
public:
    explicit PartitionStatsDumperImpl(WebProcessMemoryDump* memoryDump)
        : m_memoryDump(memoryDump), m_uid(0) { }

    // PartitionStatsDumper implementation.
    void partitionsDumpBucketStats(const char* partitionName, const PartitionBucketMemoryStats*) override;

private:
    WebProcessMemoryDump* m_memoryDump;
    size_t m_uid;
};

void PartitionStatsDumperImpl::partitionsDumpBucketStats(const char* partitionName, const PartitionBucketMemoryStats* memoryStats)
{
    ASSERT(memoryStats->isValid);
    String dumpName;
    if (memoryStats->isDirectMap)
        dumpName = String::format("partition_alloc/%s/directMap_%zu", partitionName, ++m_uid);
    else
        dumpName = String::format("partition_alloc/%s/bucket_%zu", partitionName, static_cast<size_t>(memoryStats->bucketSlotSize));

    WebMemoryAllocatorDump* allocatorDump = m_memoryDump->createMemoryAllocatorDump(dumpName);
    allocatorDump->AddScalar("size", "bytes", memoryStats->residentBytes);
    allocatorDump->AddScalar("waste_size", "bytes", memoryStats->pageWasteSize);
    allocatorDump->AddScalar("partition_page_size", "bytes", memoryStats->allocatedPageSize);
    allocatorDump->AddScalar("freeable_size", "bytes", memoryStats->freeableBytes);
    allocatorDump->AddScalar("full_partition_pages", "objects", memoryStats->numFullPages);
    allocatorDump->AddScalar("active_partition_pages", "objects", memoryStats->numActivePages);
    allocatorDump->AddScalar("empty_partition_pages", "objects", memoryStats->numEmptyPages);
    allocatorDump->AddScalar("decommitted_partition_pages", "objects", memoryStats->numDecommittedPages);

    dumpName = dumpName + "/allocated_objects";
    WebMemoryAllocatorDump* objectsDump = m_memoryDump->createMemoryAllocatorDump(dumpName);
    objectsDump->AddScalar("size", "bytes", memoryStats->activeBytes);
}

} // namespace

PartitionAllocMemoryDumpProvider* PartitionAllocMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(PartitionAllocMemoryDumpProvider, instance, ());
    return &instance;
}

bool PartitionAllocMemoryDumpProvider::onMemoryDump(blink::WebProcessMemoryDump* memoryDump)
{
    PartitionStatsDumperImpl partitionStatsDumper(memoryDump);

    // This method calls memoryStats.partitionsDumpBucketStats with memory statistics.
    WTF::Partitions::dumpMemoryStats(&partitionStatsDumper);
    return true;
}

PartitionAllocMemoryDumpProvider::PartitionAllocMemoryDumpProvider()
{
}

PartitionAllocMemoryDumpProvider::~PartitionAllocMemoryDumpProvider()
{
}

} // namespace blink

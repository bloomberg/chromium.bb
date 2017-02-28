/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Partitions_h
#define Partitions_h

#include <string.h>
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/spin_lock.h"
#include "wtf/Assertions.h"
#include "wtf/WTF.h"
#include "wtf/WTFExport.h"

namespace WTF {

class WTF_EXPORT Partitions {
 public:
  typedef void (*ReportPartitionAllocSizeFunction)(size_t);

  // Name of allocator used by tracing for marking sub-allocations while take
  // memory snapshots.
  static const char* const kAllocatedObjectPoolName;

  static void initialize(ReportPartitionAllocSizeFunction);
  ALWAYS_INLINE static base::PartitionRootGeneric* arrayBufferPartition() {
    DCHECK(s_initialized);
    return m_arrayBufferAllocator.root();
  }

  ALWAYS_INLINE static base::PartitionRootGeneric* bufferPartition() {
    DCHECK(s_initialized);
    return m_bufferAllocator.root();
  }

  ALWAYS_INLINE static base::PartitionRootGeneric* fastMallocPartition() {
    DCHECK(s_initialized);
    return m_fastMallocAllocator.root();
  }

  ALWAYS_INLINE static base::PartitionRoot* nodePartition() {
    NOTREACHED();
    return nullptr;
  }
  ALWAYS_INLINE static base::PartitionRoot* layoutPartition() {
    DCHECK(s_initialized);
    return m_layoutAllocator.root();
  }

  static size_t currentDOMMemoryUsage() {
    NOTREACHED();
    return 0;
  }

  static size_t totalSizeOfCommittedPages() {
    size_t totalSize = 0;
    totalSize += m_fastMallocAllocator.root()->total_size_of_committed_pages;
    totalSize += m_arrayBufferAllocator.root()->total_size_of_committed_pages;
    totalSize += m_bufferAllocator.root()->total_size_of_committed_pages;
    totalSize += m_layoutAllocator.root()->total_size_of_committed_pages;
    return totalSize;
  }

  static void decommitFreeableMemory();

  static void reportMemoryUsageHistogram();

  static void dumpMemoryStats(bool isLightDump, base::PartitionStatsDumper*);

  ALWAYS_INLINE static void* bufferMalloc(size_t n, const char* typeName) {
    return PartitionAllocGeneric(bufferPartition(), n, typeName);
  }
  ALWAYS_INLINE static void* bufferRealloc(void* p,
                                           size_t n,
                                           const char* typeName) {
    return PartitionReallocGeneric(bufferPartition(), p, n, typeName);
  }
  ALWAYS_INLINE static void bufferFree(void* p) {
    PartitionFreeGeneric(bufferPartition(), p);
  }
  ALWAYS_INLINE static size_t bufferActualSize(size_t n) {
    return PartitionAllocActualSize(bufferPartition(), n);
  }
  static void* fastMalloc(size_t n, const char* typeName) {
    return PartitionAllocGeneric(Partitions::fastMallocPartition(), n,
                                 typeName);
  }
  static void* fastZeroedMalloc(size_t n, const char* typeName) {
    void* result = fastMalloc(n, typeName);
    memset(result, 0, n);
    return result;
  }
  static void* fastRealloc(void* p, size_t n, const char* typeName) {
    return PartitionReallocGeneric(Partitions::fastMallocPartition(), p, n,
                                   typeName);
  }
  static void fastFree(void* p) {
    PartitionFreeGeneric(Partitions::fastMallocPartition(), p);
  }

  static void handleOutOfMemory();

 private:
  static base::subtle::SpinLock s_initializationLock;
  static bool s_initialized;

  // We have the following four partitions.
  //   - LayoutObject partition: A partition to allocate LayoutObjects.
  //     We prepare a dedicated partition for LayoutObjects because they
  //     are likely to be a source of use-after-frees. Another reason
  //     is for performance: As LayoutObjects are guaranteed to only be used
  //     by the main thread, we can bypass acquiring a lock. Also we can
  //     improve memory locality by putting LayoutObjects together.
  //   - ArrayBuffer partition: A partition to allocate array buffers.
  //   - Buffer partition: A partition to allocate other buffers that have
  //     a strong risk where the length and/or the contents are exploited from
  //     user scripts. Vectors, HashTables and Strings are allocated in the
  //      buffer partition.
  //   - Fast malloc partition: A partition to allocate all other objects.
  static base::PartitionAllocatorGeneric m_fastMallocAllocator;
  static base::PartitionAllocatorGeneric m_arrayBufferAllocator;
  static base::PartitionAllocatorGeneric m_bufferAllocator;
  static base::SizeSpecificPartitionAllocator<1024> m_layoutAllocator;
  static ReportPartitionAllocSizeFunction m_reportSizeFunction;
};

using base::kGenericMaxDirectMapped;
using base::kPageAllocationGranularity;
using base::kPageAllocationGranularityBaseMask;
using base::kPageAllocationGranularityOffsetMask;
using base::kSystemPageSize;

using base::AllocPages;
using base::DecommitSystemPages;
using base::DiscardSystemPages;
using base::PartitionFree;
using base::FreePages;
using base::GetAllocPageErrorCode;
using base::RecommitSystemPages;
using base::RoundDownToSystemPage;
using base::RoundUpToSystemPage;
using base::SetSystemPagesAccessible;
using base::SetSystemPagesInaccessible;

using base::PageAccessible;
using base::PageInaccessible;
using base::PartitionStatsDumper;
using base::PartitionMemoryStats;
using base::PartitionBucketMemoryStats;
using base::PartitionAllocHooks;

}  // namespace WTF

#endif  // Partitions_h

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

#ifndef WTF_PartitionAlloc_h
#define WTF_PartitionAlloc_h

// DESCRIPTION
// partitionAlloc() and partitionFree() are approximately analagous
// to malloc() and free().
// The main difference is that a PartitionRoot object must be supplied to
// partitionAlloc(), representing a specific "heap partition" that will
// be used to satisfy the allocation. Different partitions are guaranteed to
// exist in separate address spaces, including being separate from the main
// system heap. If the contained objects are all freed, physical memory is
// returned to the system but the address space remains reserved.
//
// Allocations using this API are only supported from the Blink main thread.
// This is ASSERT()ed in Debug builds.
//
// This allocator is designed to be extremely fast, thanks to the following
// properties and design:
// - Just a single (reasonably predicatable) branch in the hot / fast path for
// both allocating and (significantly) freeing.
// - A minimal number of operations in the hot / fast path, with the slow paths
// in separate functions, leading to the possibility of inlining.
// - Each partition page (which is usually multiple physical pages) has a header
// structure which allows fast mapping of free() address to an underlying
// bucket.
// - No support for threading yet, leading to simpler design.
// - The freelist for a given bucket is split across a number of partition
// pages, enabling various simple tricks to try and minimize fragmentation.
// - Fine-grained bucket sizes leading to less waste and better packing.
//
// The following security properties are provided at this time:
// - Linear overflows cannot corrupt into the partition.
// - Linear overflows cannot corrupt out of the partition.
// - Freed pages will only be re-used within the partition.
// - Freed pages will only hold same-sized objects when re-used.
// - Dereference of freelist pointer will fault.
//
// The following security properties could be investigated in the future:
// - No double-free detection (tcmalloc has some but it may be only a detection
// and not a defense).
// - No randomness in freelist pointers.
// - Per-object bucketing (instead of per-size) is mostly available at the API,
// but not used yet.
// - No randomness of freelist entries or bucket position.

#include "wtf/Assertions.h"
#include "wtf/MainThread.h"

#include <stdlib.h>

namespace WTF {

// Allocation granularity of sizeof(void*) bytes.
static const size_t kBucketShift = (sizeof(void*) == 8) ? 3 : 2;
// Support allocations up to kMaxAllocation bytes.
static const size_t kMaxAllocation = 4096;
static const size_t kNumBuckets = kMaxAllocation / (1 << kBucketShift);
// Underlying partition storage pages are a power-of-two size. It is typical
// for a partition page to be based on multiple system pages. We rarely deal
// with system pages. Most references to "page" refer to partition pages. We
// do also have the concept of "super pages" -- these are the underlying
// system allocations we make. Super pages can typically fit multiple
// partition pages inside them. See PageAllocator.h for more details on
// super pages.
static const size_t kPartitionPageSize = 1 << 14; // 16KB
static const size_t kPartitionPageOffsetMask = kPartitionPageSize - 1;
static const size_t kPartitionPageBaseMask = ~kPartitionPageOffsetMask;
// Special bucket id for free page metadata.
static const size_t kFreePageBucket = 0;

struct PartitionRoot;
struct PartitionBucket;

struct PartitionFreelistEntry {
    PartitionFreelistEntry* next;
};

struct PartitionPageHeader {
    int numAllocatedSlots; // Deliberately signed.
    PartitionBucket* bucket;
    PartitionFreelistEntry* freelistHead;
    PartitionPageHeader* next;
    PartitionPageHeader* prev;
};

struct PartitionFreepagelistEntry {
    PartitionPageHeader* page;
    PartitionFreepagelistEntry* next;
};

struct PartitionBucket {
    PartitionRoot* root;
    PartitionPageHeader* currPage;
    PartitionFreepagelistEntry* freePages;
    size_t numFullPages;
};

struct PartitionRoot {
    PartitionPageHeader seedPage;
    PartitionBucket seedBucket;
    PartitionBucket buckets[kNumBuckets];
    char* nextSuperPage;
    char* nextPartitionPage;
    char* nextPartitionPageEnd;
    bool initialized;
};

WTF_EXPORT void partitionAllocInit(PartitionRoot*);
WTF_EXPORT void partitionAllocShutdown(PartitionRoot*);

WTF_EXPORT NEVER_INLINE void* partitionAllocSlowPath(PartitionBucket*);
WTF_EXPORT NEVER_INLINE void partitionFreeSlowPath(PartitionPageHeader*);

ALWAYS_INLINE PartitionFreelistEntry* partitionFreelistMask(PartitionFreelistEntry* ptr)
{
    // For now, use a simple / fast mask that guarantees an invalid pointer in
    // case it gets used as a vtable pointer.
    // The one attack we're fully mitigating is where an object is freed and its
    // vtable used where the attacker doesn't get the chance to run allocations
    // between the free and use.
    // We're deliberately not trying to defend against OOB reads or writes.
    uintptr_t masked = ~reinterpret_cast<uintptr_t>(ptr);
    return reinterpret_cast<PartitionFreelistEntry*>(masked);
}

ALWAYS_INLINE size_t partitionBucketSize(const PartitionBucket* bucket)
{
    PartitionRoot* root = bucket->root;
    size_t index = bucket - &root->buckets[0];
    size_t size;
    if (UNLIKELY(index == kFreePageBucket))
        size = sizeof(PartitionFreepagelistEntry);
    else
        size = index << kBucketShift;
    return size;
}

ALWAYS_INLINE void* partitionBucketAlloc(PartitionBucket* bucket)
{
    PartitionPageHeader* page = bucket->currPage;
    PartitionFreelistEntry* ret = page->freelistHead;
    if (LIKELY(ret != 0)) {
        page->freelistHead = partitionFreelistMask(ret->next);
        page->numAllocatedSlots++;
        return ret;
    }
    return partitionAllocSlowPath(bucket);
}

ALWAYS_INLINE void* partitionAlloc(PartitionRoot* root, size_t size)
{
    ASSERT(isMainThread());
    ASSERT(size);
    size_t index = size >> kBucketShift;
    ASSERT(index < kNumBuckets);
    ASSERT(size == index << kBucketShift);
#if defined(ADDRESS_SANITIZER) || (!defined(NDEBUG) && !defined(DEBUG_PARTITION_ALLOC))
    return malloc(size);
#else
    PartitionBucket* bucket = &root->buckets[index];
    return partitionBucketAlloc(bucket);
#endif
}

ALWAYS_INLINE void partitionFree(void* ptr)
{
    ASSERT(isMainThread());
#if defined(ADDRESS_SANITIZER) || (!defined(NDEBUG) && !defined(DEBUG_PARTITION_ALLOC))
    free(ptr);
#else
    uintptr_t pointerAsUint = reinterpret_cast<uintptr_t>(ptr);
    // Checks that the pointer is after the page header. You can't free the
    // page header!
    ASSERT((pointerAsUint & kPartitionPageOffsetMask) >= sizeof(PartitionPageHeader));
    PartitionPageHeader* page = reinterpret_cast<PartitionPageHeader*>(pointerAsUint & kPartitionPageBaseMask);
    // Checks that the pointer is a multiple of bucket size.
    ASSERT(!(((pointerAsUint & kPartitionPageOffsetMask) - sizeof(PartitionPageHeader)) % partitionBucketSize(page->bucket)));
    PartitionFreelistEntry* entry = static_cast<PartitionFreelistEntry*>(ptr);
    entry->next = partitionFreelistMask(page->freelistHead);
    page->freelistHead = entry;
    --page->numAllocatedSlots;
    if (UNLIKELY(page->numAllocatedSlots <= 0))
        partitionFreeSlowPath(page);
#endif
}

} // namespace WTF

using WTF::PartitionRoot;
using WTF::partitionAllocInit;
using WTF::partitionAllocShutdown;
using WTF::partitionAlloc;
using WTF::partitionFree;

#endif // WTF_PartitionAlloc_h

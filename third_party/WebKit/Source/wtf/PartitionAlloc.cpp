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

#include "config.h"
#include "wtf/PartitionAlloc.h"

#include "wtf/CryptographicallyRandomNumber.h"

#if OS(UNIX)
#include <sys/mman.h>

#ifndef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif // OS(UNIX)

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WTF {

void partitionAllocInit(PartitionRoot* root)
{
    ASSERT(!root->pageBase);
    size_t i;
    for (i = 0; i < kNumBuckets; ++i) {
        PartitionBucket* bucket = &root->buckets[i];
        bucket->root = root;
        bucket->currPage = &root->seedPage;
        bucket->freePages = 0;
        bucket->numFullPages = 0;
    }
    uintptr_t random;
#if OS(UNIX)
    random = static_cast<uintptr_t>(cryptographicallyRandomNumber());
#if CPU(X86_64)
    random <<= 32UL;
    random |= static_cast<uintptr_t>(cryptographicallyRandomNumber());
    // This address mask gives a low liklihood of address space collisions.
    // We handle the situation gracefully if there is a collision.
    random &= (0x3ffffffff000UL & kPageMask);
#else
    random &= (0x3ffff000 & kPageMask);
    random += 0x20000000;
#endif // CPU(X86_64)
#else
    random = 0;
#endif // OS(UNIX)

    root->pageBase = reinterpret_cast<char*>(random);
    root->seedPage.numAllocatedSlots = 0;
    root->seedPage.bucket = &root->seedBucket;
    root->seedPage.freelistHead = 0;
    root->seedPage.next = &root->seedPage;
    root->seedPage.prev = &root->seedPage;

    root->seedBucket.root = root;
    root->seedBucket.currPage = 0;
    root->seedBucket.freePages = 0;
    root->seedBucket.numFullPages = 0;
}

static ALWAYS_INLINE void partitionFreePage(PartitionPageHeader* page)
{
#if OS(UNIX)
    int ret = munmap(page, kPageSize);
    ASSERT(!ret);
#endif
}

static void partitionAllocShutdownBucket(PartitionBucket* bucket)
{
    // Failure here indicates a memory leak.
    ASSERT(!bucket->numFullPages);
    PartitionFreepagelistEntry* freePage = bucket->freePages;
    while (freePage) {
        PartitionFreepagelistEntry* next = freePage->next;
        partitionFreePage(freePage->page);
        partitionFree(freePage);
        freePage = next;
    }
    PartitionPageHeader* page = bucket->currPage;
    do {
        // Failure here indicates a memory leak.
        ASSERT(bucket == &bucket->root->buckets[kFreePageBucket] || !page->numAllocatedSlots);
        PartitionPageHeader* next = page->next;
        if (page != &bucket->root->seedPage)
            partitionFreePage(page);
        page = next;
    } while (page != bucket->currPage);
}

void partitionAllocShutdown(PartitionRoot* root)
{
    ASSERT(root->pageBase);
    size_t i;
    // First, free the non-freepage buckets. Freeing the free pages in these
    // buckets will depend on the freepage bucket.
    for (i = 0; i < kNumBuckets; ++i) {
        if (i != kFreePageBucket) {
            PartitionBucket* bucket = &root->buckets[i];
            partitionAllocShutdownBucket(bucket);
        }
    }
    // Finally, free the freepage bucket.
    partitionAllocShutdownBucket(&root->buckets[kFreePageBucket]);
    root->pageBase = 0;
}

static ALWAYS_INLINE PartitionPageHeader* partitionAllocPage(char** pageBasePointer)
{
// TODO(cevans): When porting more generically, the probable approach
// is to use the underlying real malloc() as the page source.
#if OS(UNIX)
    char* ptr = reinterpret_cast<char*>(mmap(*pageBasePointer, kPageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    RELEASE_ASSERT(ptr != MAP_FAILED);
    // If our requested address collided with another mapping, there's a
    // chance we'll get back an unaligned address. We fix this by attempting
    // the allocation again, but with enough slack pages that we can find
    // correct alignment within the allocation.
    if (UNLIKELY(reinterpret_cast<uintptr_t>(ptr) & ~kPageMask)) {
        int ret = munmap(ptr, kPageSize);
        ASSERT(!ret);
        ptr = reinterpret_cast<char*>(mmap(*pageBasePointer, (kPageSize * 2) - kSystemPageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
        RELEASE_ASSERT(ptr != MAP_FAILED);
        int numSystemPages = kPageSize / kSystemPageSize;
        ASSERT(numSystemPages > 1);
        int numSystemPagesToUnmap = numSystemPages - 1;
        int numSystemPagesBefore = (numSystemPages - ((reinterpret_cast<uintptr_t>(ptr) & ~kPageMask) / kSystemPageSize)) % numSystemPages;
        ASSERT(numSystemPagesBefore <= numSystemPagesToUnmap);
        int numSystemPagesAfter = numSystemPagesToUnmap - numSystemPagesBefore;
        if (numSystemPagesBefore) {
            uintptr_t beforeSize = kSystemPageSize * numSystemPagesBefore;
            ret = munmap(ptr, beforeSize);
            ASSERT(!ret);
            ptr += beforeSize;
        }
        if (numSystemPagesAfter) {
            ret = munmap(ptr + kPageSize, kSystemPageSize * numSystemPagesAfter);
            ASSERT(!ret);
        }
    }
#else
    char* ptr = 0;
    CRASH();
#endif
    *pageBasePointer += kPageSize;
    return reinterpret_cast<PartitionPageHeader*>(ptr);
}

static ALWAYS_INLINE void partitionUnusePage(PartitionPageHeader* page)
{
#if OS(UNIX)
    int ret = madvise(page, kPageSize, MADV_FREE);
    ASSERT(!ret);
#endif
}

static ALWAYS_INLINE size_t partitionBucketSlots(const PartitionBucket* bucket)
{
    ASSERT(!(sizeof(PartitionPageHeader) % sizeof(void*)));
    return (kPageSize - sizeof(PartitionPageHeader)) / partitionBucketSize(bucket);
}

static ALWAYS_INLINE void partitionPageInit(PartitionPageHeader* page, PartitionBucket* bucket)
{
    page->numAllocatedSlots = 1;
    page->bucket = bucket;
    size_t size = partitionBucketSize(bucket);
    size_t numSlots = partitionBucketSlots(bucket);
    RELEASE_ASSERT(numSlots > 1);
    page->freelistHead = reinterpret_cast<PartitionFreelistEntry*>((reinterpret_cast<char*>(page) + sizeof(PartitionPageHeader) + size));
    PartitionFreelistEntry* freelist = page->freelistHead;
    // Account for the slot we've handed out right away as a return value.
    --numSlots;
    // This loop sets up the initial chain of freelist pointers in the new page.
    while (--numSlots) {
        PartitionFreelistEntry* next = reinterpret_cast<PartitionFreelistEntry*>(reinterpret_cast<char*>(freelist) + size);
        freelist->next = partitionFreelistMask(next);
        freelist = next;
    }
    freelist->next = partitionFreelistMask(0);
    // Artifically elevate the allocation count on free page metadata bucket
    // pages, so they never become candidates for being freed. It's a
    // re-entrancy headache.
    if (bucket == &bucket->root->buckets[kFreePageBucket])
        ++page->numAllocatedSlots;
}

static ALWAYS_INLINE void partitionUnlinkPage(PartitionPageHeader* page)
{
    ASSERT(page->prev->next == page);
    ASSERT(page->next->prev == page);

    page->next->prev = page->prev;
    page->prev->next = page->next;
}

static ALWAYS_INLINE void partitionLinkPage(PartitionPageHeader* newPage, PartitionPageHeader* prevPage)
{
    ASSERT(prevPage->prev->next == prevPage);
    ASSERT(prevPage->next->prev == prevPage);

    newPage->prev = prevPage;
    newPage->next = prevPage->next;

    prevPage->next->prev = newPage;
    prevPage->next = newPage;
}

void* partitionAllocSlowPath(PartitionBucket* bucket)
{
    // Slow path. First look for a page in our linked ring list of non-full
    // pages.
    PartitionPageHeader* page = bucket->currPage;
    PartitionPageHeader* next = page->next;
    ASSERT(page == &bucket->root->seedPage || (page->bucket == bucket && next->bucket == bucket));

    while (LIKELY(next != page)) {
        ASSERT(next->bucket == bucket);
        ASSERT(next->next->prev == next);
        ASSERT(next->prev->next == next);
        ASSERT(next != &bucket->root->seedPage);
        if (LIKELY(next->freelistHead != 0)) {
            bucket->currPage = next;
            PartitionFreelistEntry* ret = next->freelistHead;
            next->freelistHead = partitionFreelistMask(ret->next);
            next->numAllocatedSlots++;
            return ret;
        }
        // Pull this page out of the non-full page list, since it has no free
        // slots.
        // This tags the page as full so that free'ing can tell, and move
        // the page back into the non-full page list when appropriate.
        ASSERT(next->numAllocatedSlots == partitionBucketSlots(bucket));
        next->numAllocatedSlots = -next->numAllocatedSlots;
        partitionUnlinkPage(next);
        ++bucket->numFullPages;

        next = next->next;
    }

    // Second, look in our list of freed but reserved pages.
    PartitionPageHeader* newPage;
    PartitionFreepagelistEntry* pagelist = bucket->freePages;
    if (LIKELY(pagelist != 0)) {
        newPage = pagelist->page;
        bucket->freePages = pagelist->next;
        partitionFree(pagelist);
        ASSERT(page != &bucket->root->seedPage);
        partitionLinkPage(newPage, page);
    } else {
        // Third. If we get here, we need a brand new page.
        newPage = partitionAllocPage(&bucket->root->pageBase);
        if (UNLIKELY(page == &bucket->root->seedPage)) {
            // If this is the first page allocation to this bucket, then
            // fully replace the seed page. This avoids pointlessly iterating
            // over it.
            newPage->prev = newPage;
            newPage->next = newPage;
        } else {
            partitionLinkPage(newPage, page);
        }
    }
    partitionPageInit(newPage, bucket);
    bucket->currPage = newPage;

    return reinterpret_cast<char*>(newPage) + sizeof(PartitionPageHeader);
}

void partitionFreeSlowPath(PartitionPageHeader* page)
{
    PartitionBucket* bucket = page->bucket;
    if (LIKELY(page->numAllocatedSlots == 0)) {
        // Page became fully unused.
        // If it's the current page, leave it be so that we don't bounce a page
        // onto the free page list and immediately back out again.
        if (LIKELY(page == bucket->currPage))
            return;

        partitionUnlinkPage(page);
        partitionUnusePage(page);
        PartitionFreepagelistEntry* entry = static_cast<PartitionFreepagelistEntry*>(partitionBucketAlloc(&bucket->root->buckets[kFreePageBucket]));
        entry->page = page;
        entry->next = bucket->freePages;
        bucket->freePages = entry;
    } else {
        // Fully used page became partially used. It must be put back on the
        // non-full page list.
        partitionLinkPage(page, bucket->currPage);
        page->numAllocatedSlots = -page->numAllocatedSlots - 2;
        ASSERT(page->numAllocatedSlots == partitionBucketSlots(bucket) - 1);
        --bucket->numFullPages;
    }
}

#ifndef NDEBUG

void partitionDumpStats(const PartitionRoot& root)
{
    size_t i;
    for (i = 0; i < kNumBuckets; ++i) {
        const PartitionBucket& bucket = root.buckets[i];
        if (bucket.currPage == &bucket.root->seedPage && !bucket.freePages) {
            // Empty bucket with no freelist pages. Skip reporting it.
            continue;
        }
        size_t numFreePages = 0;
        const PartitionFreepagelistEntry* freePages = bucket.freePages;
        while (freePages) {
            ++numFreePages;
            freePages = freePages->next;
        }
        size_t bucketSlotSize = partitionBucketSize(&bucket);
        size_t bucketNumSlots = partitionBucketSlots(&bucket);
        size_t numActivePages = bucket.numFullPages;
        size_t numActiveBytes = numActivePages * bucketSlotSize * bucketNumSlots;
        const PartitionPageHeader* page = bucket.currPage;
        do {
            if (page != &bucket.root->seedPage) {
                ++numActivePages;
                numActiveBytes += (page->numAllocatedSlots * bucketSlotSize);
            }
            page = page->next;
        } while (page != bucket.currPage);
        printf("bucket size %ld: %ld/%ld bytes, %ld free pages\n", bucketSlotSize, numActiveBytes, numActivePages * kPageSize, numFreePages);
    }
}
#endif // !NDEBUG

} // namespace WTF


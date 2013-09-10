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

#include "wtf/PageAllocator.h"
#include "wtf/Vector.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

COMPILE_ASSERT(WTF::kPartitionPageSize < WTF::kSuperPageSize, ok_partition_page_size);

namespace WTF {

WTF_EXPORT void partitionAllocInit(PartitionRoot* root, size_t numBuckets, size_t maxAllocation)
{
    ASSERT(!root->initialized);
    root->initialized = true;
    root->lock = 0;
    root->numBuckets = numBuckets;
    root->maxAllocation = maxAllocation;
    size_t i;
    for (i = 0; i < root->numBuckets; ++i) {
        PartitionBucket* bucket = &root->buckets()[i];
        bucket->root = root;
        bucket->currPage = &root->seedPage;
        bucket->freePages = 0;
        bucket->numFullPages = 0;
    }

    root->nextSuperPage = 0;
    root->nextPartitionPage = 0;
    root->nextPartitionPageEnd = 0;
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

static ALWAYS_INLINE void partitionFreeSuperPage(PartitionPageHeader* page)
{
    freeSuperPages(page, kSuperPageSize);
}

static void partitionCollectIfSuperPage(PartitionPageHeader* partitionPage, Vector<PartitionPageHeader*>* superPages)
{
    PartitionPageHeader* superPage = reinterpret_cast<PartitionPageHeader*>(reinterpret_cast<uintptr_t>(partitionPage) & kSuperPageBaseMask);
    uintptr_t superPageOffset = reinterpret_cast<uintptr_t>(partitionPage) & kSuperPageOffsetMask;
    // If this partition page is at the start of a super page, note it so we can
    // free all the distinct super pages.
    if (!superPageOffset)
        superPages->append(superPage);
}

static bool partitionAllocShutdownBucket(PartitionBucket* bucket, Vector<PartitionPageHeader*>* superPages)
{
    // Failure here indicates a memory leak.
    bool noLeaks = !bucket->numFullPages;
    PartitionFreepagelistEntry* entry = bucket->freePages;
    while (entry) {
        PartitionFreepagelistEntry* next = entry->next;
        partitionCollectIfSuperPage(entry->page, superPages);
        partitionFree(entry);
        entry = next;
    }
    PartitionPageHeader* page = bucket->currPage;
    do {
        if (page->numAllocatedSlots)
            noLeaks = false;
        PartitionPageHeader* next = page->next;
        if (page != &bucket->root->seedPage)
            partitionCollectIfSuperPage(page, superPages);
        page = next;
    } while (page != bucket->currPage);

    return noLeaks;
}

bool partitionAllocShutdown(PartitionRoot* root)
{
    bool noLeaks = true;
    ASSERT(root->initialized);
    root->initialized = false;
    // As we iterate through all the partition pages, we keep a list of all the
    // distinct super pages that we have seen. This is so that we can free all
    // the super pages correctly. A super page must be freed all at once -- it
    // is not permissible to free a super page by freeing all its component
    // partition pages.
    // Note that we cannot free a super page upon discovering it, because a
    // single super page will likely contain partition pages from multiple
    // different buckets.
    Vector<PartitionPageHeader*> superPages;
    size_t i;
    // First, free the non-freepage buckets. Freeing the free pages in these
    // buckets will depend on the freepage bucket.
    for (i = 0; i < root->numBuckets; ++i) {
        if (i != kFreePageBucket) {
            PartitionBucket* bucket = &root->buckets()[i];
            if (!partitionAllocShutdownBucket(bucket, &superPages))
                noLeaks = false;
        }
    }
    // Finally, free the freepage bucket.
    (void) partitionAllocShutdownBucket(&root->buckets()[kFreePageBucket], &superPages);
    // Now that we've examined all partition pages in all buckets, it's safe
    // to free all our super pages.
    for (Vector<PartitionPageHeader*>::iterator it = superPages.begin(); it != superPages.end(); ++it)
        partitionFreeSuperPage(*it);

    return noLeaks;
}

static ALWAYS_INLINE PartitionPageHeader* partitionAllocPage(PartitionRoot* root)
{
    char* ret = 0;
    if (LIKELY(root->nextPartitionPage != 0)) {
        // In this case, we can still hand out pages from a previous
        // super page allocation.
        ret = root->nextPartitionPage;
        root->nextPartitionPage += kPartitionPageSize;
        if (UNLIKELY(root->nextPartitionPage == root->nextPartitionPageEnd)) {
            // We ran out, need to get more pages next time.
            root->nextPartitionPage = 0;
            root->nextPartitionPageEnd = 0;
        }
    } else {
        // Need a new super page.
        // We need to put a guard page in front if either:
        // a) This is the first super page allocation.
        // b) The super page did not end up at our suggested address.
        bool needsGuard = false;
        if (!root->nextSuperPage) {
            needsGuard = true;
            root->nextSuperPage = getRandomSuperPageBase();
        }
        ret = reinterpret_cast<char*>(allocSuperPages(root->nextSuperPage, kSuperPageSize));
        if (ret != root->nextSuperPage) {
            needsGuard = true;
            // Re-randomize the base location for next time just in case the
            // underlying operating system picks lousy locations for mappings.
            root->nextSuperPage = 0;
        } else {
            root->nextSuperPage = ret + kSuperPageSize;
        }
        root->nextPartitionPageEnd = ret + kSuperPageSize;
        if (needsGuard) {
            setSystemPagesInaccessible(ret, kPartitionPageSize);
            ret += kPartitionPageSize;
        }
        root->nextPartitionPage = ret + kPartitionPageSize;
    }
    return reinterpret_cast<PartitionPageHeader*>(ret);
}

static ALWAYS_INLINE void partitionUnusePage(PartitionPageHeader* page)
{
    decommitSystemPages(page, kPartitionPageSize);
}

static ALWAYS_INLINE size_t partitionBucketSlots(const PartitionBucket* bucket)
{
    COMPILE_ASSERT(!(sizeof(PartitionPageHeader) % sizeof(void*)), PartitionPageHeader_size_should_be_multiple_of_pointer_size);
    return (kPartitionPageSize - sizeof(PartitionPageHeader)) / partitionBucketSize(bucket);
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
        newPage = partitionAllocPage(bucket->root);
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
        PartitionFreepagelistEntry* entry = static_cast<PartitionFreepagelistEntry*>(partitionBucketAlloc(&bucket->root->buckets()[kFreePageBucket]));
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

void* partitionReallocGeneric(PartitionRoot* root, void* ptr, size_t oldSize, size_t newSize)
{
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
    return realloc(ptr, newSize);
#else
    size_t oldIndex = partitionAllocRoundup(oldSize) >> kBucketShift;
    if (oldIndex > root->numBuckets)
        oldIndex = root->numBuckets;
    size_t newIndex = partitionAllocRoundup(newSize) >> kBucketShift;
    if (newIndex > root->numBuckets)
        newIndex = root->numBuckets;

    if (oldIndex == newIndex) {
        // Same bucket. But kNumBuckets indicates the fastMalloc "bucket" so in
        // that case we're not done; we have to forward to fastRealloc.
        if (oldIndex == root->numBuckets)
            return WTF::fastRealloc(ptr, newSize);
        return ptr;
    }
    // This realloc cannot be resized in-place. Sadness.
    void* ret = partitionAllocGeneric(root, newSize);
    size_t copySize = oldSize;
    if (newSize < oldSize)
        copySize = newSize;
    memcpy(ret, ptr, copySize);
    partitionFreeGeneric(root, ptr, oldSize);
    return ret;
#endif
}

#ifndef NDEBUG

void partitionDumpStats(const PartitionRoot& root)
{
    size_t i;
    for (i = 0; i < root.numBuckets; ++i) {
        const PartitionBucket& bucket = root.buckets()[i];
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
        printf("bucket size %ld: %ld/%ld bytes, %ld free pages\n", bucketSlotSize, numActiveBytes, numActivePages * kPartitionPageSize, numFreePages);
    }
}
#endif // !NDEBUG

} // namespace WTF


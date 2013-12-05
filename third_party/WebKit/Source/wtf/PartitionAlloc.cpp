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

#include <string.h>

#ifndef NDEBUG
#include <stdio.h>
#endif

// Two partition pages are used as guard / metadata page so make sure the super
// page size is bigger.
COMPILE_ASSERT(WTF::kPartitionPageSize * 4 <= WTF::kSuperPageSize, ok_super_page_size);
COMPILE_ASSERT(!(WTF::kSuperPageSize % WTF::kPartitionPageSize), ok_super_page_multiple);
// Four system pages gives us room to hack out a still-guard-paged piece
// of metadata in the middle of a guard partition page.
COMPILE_ASSERT(WTF::kSystemPageSize * 4 <= WTF::kPartitionPageSize, ok_partition_page_size);
COMPILE_ASSERT(!(WTF::kPartitionPageSize % WTF::kSystemPageSize), ok_partition_page_multiple);
COMPILE_ASSERT(sizeof(WTF::PartitionPage) <= WTF::kPageMetadataSize, PartitionPage_not_too_big);
COMPILE_ASSERT(sizeof(WTF::PartitionSuperPageExtentEntry) <= WTF::kPageMetadataSize, PartitionSuperPageExtentEntry_not_too_big);
COMPILE_ASSERT(WTF::kPageMetadataSize * WTF::kNumPartitionPagesPerSuperPage <= WTF::kPartitionPageSize - (WTF::kSystemPageSize * 2), page_metadata_fits_in_hole);

namespace WTF {

static size_t partitionBucketPageSize(size_t size)
{
    double bestWasteRatio = 1.0f;
    size_t bestPages = 0;
    for (size_t i = kNumSystemPagesPerPartitionPage - 1; i <= kNumSystemPagesPerPartitionPage; ++i) {
        size_t pageSize = kSystemPageSize * i;
        size_t numSlots = pageSize / size;
        size_t waste = pageSize - (numSlots * size);
        // Leave a page unfaulted is not free; the page will occupy an empty page table entry.
        // Make a simple attempt to account for that.
        waste += sizeof(void*) * (kNumSystemPagesPerPartitionPage - i);
        double wasteRatio = (double) waste / (double) pageSize;
        if (wasteRatio < bestWasteRatio) {
            bestWasteRatio = wasteRatio;
            bestPages = i;
        }
    }
    ASSERT(bestPages > 0);
    return bestPages * kSystemPageSize;
}

WTF_EXPORT void partitionAllocInit(PartitionRoot* root, size_t numBuckets, size_t maxAllocation)
{
    ASSERT(!root->initialized);
    root->initialized = true;
    root->lock = 0;
    root->totalSizeOfSuperPages = 0;
    root->numBuckets = numBuckets;
    root->maxAllocation = maxAllocation;
    size_t i;
    for (i = 0; i < root->numBuckets; ++i) {
        PartitionBucket* bucket = &root->buckets()[i];
        bucket->root = root;
        bucket->activePagesHead = &root->seedPage;
        bucket->freePagesHead = 0;
        bucket->numFullPages = 0;
        size_t size = partitionBucketSize(bucket);
        bucket->pageSize = partitionBucketPageSize(size);
    }

    root->nextSuperPage = 0;
    root->nextPartitionPage = 0;
    root->nextPartitionPageEnd = 0;
    root->currentExtent = &root->firstExtent;
    root->firstExtent.superPageBase = 0;
    root->firstExtent.superPagesEnd = 0;
    root->firstExtent.next = 0;
    root->seedPage.numAllocatedSlots = 0;
    root->seedPage.numUnprovisionedSlots = 0;
    root->seedPage.bucket = &root->seedBucket;
    root->seedPage.freelistHead = 0;
    root->seedPage.next = &root->seedPage;
    root->seedPage.prev = &root->seedPage;

    root->seedBucket.root = root;
    root->seedBucket.activePagesHead = 0;
    root->seedBucket.freePagesHead = 0;
    root->seedBucket.numFullPages = 0;
}

static bool partitionAllocShutdownBucket(PartitionBucket* bucket)
{
    // Failure here indicates a memory leak.
    bool noLeaks = !bucket->numFullPages;
    PartitionPage* page = bucket->activePagesHead;
    do {
        if (page->numAllocatedSlots)
            noLeaks = false;
        page = page->next;
    } while (page != bucket->activePagesHead);

    return noLeaks;
}

bool partitionAllocShutdown(PartitionRoot* root)
{
    bool noLeaks = true;
    ASSERT(root->initialized);
    root->initialized = false;
    size_t i;
    for (i = 0; i < root->numBuckets; ++i) {
        PartitionBucket* bucket = &root->buckets()[i];
        if (!partitionAllocShutdownBucket(bucket))
            noLeaks = false;
    }

    // Now that we've examined all partition pages in all buckets, it's safe
    // to free all our super pages. We first collect the super page pointers
    // on the stack because some of them are themselves store in super pages.
    char* superPages[kMaxPartitionSize / kSuperPageSize];
    size_t numSuperPages = 0;
    PartitionSuperPageExtentEntry* entry = &root->firstExtent;
    while (entry) {
        char* superPage = entry->superPageBase;
        while (superPage != entry->superPagesEnd) {
            superPages[numSuperPages] = superPage;
            numSuperPages++;
            superPage += kSuperPageSize;
        }
        entry = entry->next;
    }
    ASSERT(numSuperPages == root->totalSizeOfSuperPages / kSuperPageSize);
    for (size_t i = 0; i < numSuperPages; ++i) {
        SuperPageBitmap::unregisterSuperPage(superPages[i]);
        freePages(superPages[i], kSuperPageSize);
    }

    return noLeaks;
}

static ALWAYS_INLINE void* partitionAllocPage(PartitionRoot* root)
{
    if (LIKELY(root->nextPartitionPage != 0)) {
        // In this case, we can still hand out pages from a previous
        // super page allocation.
        char* ret = root->nextPartitionPage;
        root->nextPartitionPage += kPartitionPageSize;
        if (UNLIKELY(root->nextPartitionPage == root->nextPartitionPageEnd)) {
            // We ran out, need to get more pages next time.
            root->nextPartitionPage = 0;
            root->nextPartitionPageEnd = 0;
        }
        return ret;
    }

    // Need a new super page.
    root->totalSizeOfSuperPages += kSuperPageSize;
    RELEASE_ASSERT(root->totalSizeOfSuperPages <= kMaxPartitionSize);
    char* requestedAddress = root->nextSuperPage;
    char* superPage = reinterpret_cast<char*>(allocPages(requestedAddress, kSuperPageSize, kSuperPageSize));
    SuperPageBitmap::registerSuperPage(superPage);
    root->nextSuperPage = superPage + kSuperPageSize;
    char* ret = superPage + kPartitionPageSize;
    root->nextPartitionPage = ret + kPartitionPageSize;
    root->nextPartitionPageEnd = root->nextSuperPage - kPartitionPageSize;
    // Make the first partition page in the super page a guard page, but leave a    // hole in the middle.
    // This is where we put page metadata and also a tiny amount of extent
    // metadata.
    setSystemPagesInaccessible(superPage, kSystemPageSize);
    setSystemPagesInaccessible(superPage + (kPartitionPageSize - kSystemPageSize), kSystemPageSize);
    // Also make the last partition page a guard page.
    setSystemPagesInaccessible(superPage + (kSuperPageSize - kPartitionPageSize), kPartitionPageSize);

    // If we were after a specific address, but didn't get it, assume that
    // the system chose a lousy address and re-randomize the next
    // allocation.
    if (requestedAddress && requestedAddress != superPage)
        root->nextSuperPage = 0;

    // We allocated a new super page so update super page metadata.
    // First check if this is a new extent or not.
    PartitionSuperPageExtentEntry* currentExtent = root->currentExtent;
    bool isNewExtent = (superPage != requestedAddress);
    if (UNLIKELY(isNewExtent)) {
        if (currentExtent->superPageBase) {
            // We already have a super page, so need to allocate metadata in the linked list.
            PartitionSuperPageExtentEntry* newEntry = reinterpret_cast<PartitionSuperPageExtentEntry*>(partitionSuperPageToMetadataArea(currentExtent->superPageBase));
            newEntry->next = 0;
            currentExtent->next = newEntry;
            currentExtent = newEntry;
            root->currentExtent = newEntry;
        }
        currentExtent->superPageBase = superPage;
        currentExtent->superPagesEnd = superPage + kSuperPageSize;
    } else {
        // We allocated next to an existing extent so just nudge the size up a little.
        currentExtent->superPagesEnd += kSuperPageSize;
        ASSERT(ret >= currentExtent->superPageBase && ret < currentExtent->superPagesEnd);
    }

    return ret;
}

static ALWAYS_INLINE void partitionUnusePage(PartitionPage* page)
{
    void* addr = partitionPageToPointer(page);
    decommitSystemPages(addr, kPartitionPageSize);
}

static ALWAYS_INLINE size_t partitionBucketSlots(const PartitionBucket* bucket)
{
    return bucket->pageSize / partitionBucketSize(bucket);
}

static ALWAYS_INLINE void partitionPageReset(PartitionPage* page, PartitionBucket* bucket)
{
    ASSERT(page != &bucket->root->seedPage);
    page->numAllocatedSlots = 0;
    page->numUnprovisionedSlots = partitionBucketSlots(bucket);
    ASSERT(page->numUnprovisionedSlots > 1);
    page->bucket = bucket;
    // NULLing the freelist is not strictly necessary but it makes an ASSERT in partitionPageFillFreelist simpler.
    page->freelistHead = 0;
}

static ALWAYS_INLINE char* partitionPageAllocAndFillFreelist(PartitionPage* page)
{
    ASSERT(page != &page->bucket->root->seedPage);
    size_t numSlots = page->numUnprovisionedSlots;
    ASSERT(numSlots);
    PartitionBucket* bucket = page->bucket;
    // We should only get here when _every_ slot is either used or unprovisioned.
    // (The third state is "on the freelist". If we have a non-empty freelist, we should not get here.)
    ASSERT(numSlots + page->numAllocatedSlots == partitionBucketSlots(bucket));
    // Similarly, make explicitly sure that the freelist is empty.
    ASSERT(!page->freelistHead);

    size_t size = partitionBucketSize(bucket);
    char* base = reinterpret_cast<char*>(partitionPageToPointer(page));
    char* returnObject = base + (size * page->numAllocatedSlots);
    char* nextFreeObject = returnObject + size;
    char* subPageLimit = reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(returnObject) + kSystemPageSize) & kSystemPageBaseMask);

    size_t numNewFreelistEntries = 0;
    if (LIKELY(subPageLimit > nextFreeObject))
        numNewFreelistEntries = (subPageLimit - nextFreeObject) / size;

    // We always return an object slot -- that's the +1 below.
    // We do not neccessarily create any new freelist entries, because we cross sub page boundaries frequently for large bucket sizes.
    numSlots -= (numNewFreelistEntries + 1);
    page->numUnprovisionedSlots = numSlots;
    page->numAllocatedSlots++;

    if (LIKELY(numNewFreelistEntries)) {
        PartitionFreelistEntry* entry = reinterpret_cast<PartitionFreelistEntry*>(nextFreeObject);
        page->freelistHead = entry;
        while (--numNewFreelistEntries) {
            nextFreeObject += size;
            PartitionFreelistEntry* nextEntry = reinterpret_cast<PartitionFreelistEntry*>(nextFreeObject);
            entry->next = partitionFreelistMask(nextEntry);
            entry = nextEntry;
        }
        entry->next = partitionFreelistMask(0);
    } else {
        page->freelistHead = 0;
    }
    return returnObject;
}

static ALWAYS_INLINE void partitionUnlinkPage(PartitionPage* page)
{
    ASSERT(page != &page->bucket->root->seedPage);
    ASSERT(page->prev->next == page);
    ASSERT(page->next->prev == page);

    page->next->prev = page->prev;
    page->prev->next = page->next;
}

static ALWAYS_INLINE void partitionLinkPageBefore(PartitionPage* newPage, PartitionPage* nextPage)
{
    ASSERT(nextPage != &nextPage->bucket->root->seedPage);
    ASSERT(nextPage->prev->next == nextPage);
    ASSERT(nextPage->next->prev == nextPage);

    newPage->next = nextPage;
    newPage->prev = nextPage->prev;

    nextPage->prev->next = newPage;
    nextPage->prev = newPage;
}

void* partitionAllocSlowPath(PartitionBucket* bucket)
{
    // The slow path is called when the freelist is empty.
    PartitionPage* page = bucket->activePagesHead;
    PartitionPage* next = page->next;
    ASSERT(page == &bucket->root->seedPage || (page->bucket == bucket && next->bucket == bucket));

    // First, see if the current partition page still has capacity and if so,
    // fill out the freelist a little more.
    if (LIKELY(page->numUnprovisionedSlots))
        return partitionPageAllocAndFillFreelist(page);

    // Second, look for a page in our linked ring list of non-full pages.
    while (LIKELY(next != page)) {
        ASSERT(next->bucket == bucket);
        ASSERT(next->next->prev == next);
        ASSERT(next->prev->next == next);
        ASSERT(next != &bucket->root->seedPage);
        if (LIKELY(next->freelistHead != 0)) {
            bucket->activePagesHead = next;
            PartitionFreelistEntry* ret = next->freelistHead;
            next->freelistHead = partitionFreelistMask(ret->next);
            next->numAllocatedSlots++;
            return ret;
        }
        if (LIKELY(next->numUnprovisionedSlots)) {
            bucket->activePagesHead = next;
            return partitionPageAllocAndFillFreelist(next);
        }
        // Pull this page out of the non-full page list, since it has no free
        // slots.
        // This tags the page as full so that free'ing can tell, and move
        // the page back into the non-full page list when appropriate.
        ASSERT(next->numAllocatedSlots == static_cast<int>(partitionBucketSlots(bucket)));
        next->numAllocatedSlots = -next->numAllocatedSlots;
        partitionUnlinkPage(next);
        ++bucket->numFullPages;

        next = next->next;
    }

    // After we've considered and rejected every partition page in the list,
    // we should by definition have a single self-linked page left. We will
    // replace this single page with the new page we choose.
    ASSERT(page == page->next);
    ASSERT(page == page->prev);
    ASSERT(page == &bucket->root->seedPage || page->numAllocatedSlots == static_cast<int>(partitionBucketSlots(bucket)));
    if (LIKELY(page != &bucket->root->seedPage)) {
        page->numAllocatedSlots = -page->numAllocatedSlots;
        ++bucket->numFullPages;
    }

    // Third, look in our list of freed but reserved pages.
    PartitionPage* newPage = bucket->freePagesHead;
    if (LIKELY(newPage != 0)) {
        ASSERT(newPage != &bucket->root->seedPage);
        bucket->freePagesHead = newPage->next;
    } else {
        // Fourth. If we get here, we need a brand new page.
        void* rawNewPage = partitionAllocPage(bucket->root);
        newPage = partitionPointerToPageNoAlignmentCheck(rawNewPage);
    }

    newPage->prev = newPage;
    newPage->next = newPage;
    bucket->activePagesHead = newPage;
    partitionPageReset(newPage, bucket);
    return partitionPageAllocAndFillFreelist(newPage);
}

void partitionFreeSlowPath(PartitionPage* page)
{
    PartitionBucket* bucket = page->bucket;
    ASSERT(page != &bucket->root->seedPage);
    ASSERT(bucket->activePagesHead != &bucket->root->seedPage);
    if (LIKELY(page->numAllocatedSlots == 0)) {
        // Page became fully unused.
        // If it's the current page, change it!
        if (LIKELY(page == bucket->activePagesHead)) {
            if (UNLIKELY(page->next == page)) {
                // We do not free the last active page in a bucket.
                // To do so is a serious performance issue as we can get into
                // a repeating state change between 0 and 1 objects of a given
                // size allocated; and each state change incurs either a system
                // call or a page fault, or more.
                return;
            }
            bucket->activePagesHead = page->next;
        }

        partitionUnlinkPage(page);
        partitionUnusePage(page);
        page->next = bucket->freePagesHead;
        bucket->freePagesHead = page;
    } else {
        // Ensure that the page is full. That's the only valid case if we
        // arrive here.
        ASSERT(page->numAllocatedSlots < 0);
        // Fully used page became partially used. It must be put back on the
        // non-full page list. Also make it the current page to increase the
        // chances of it being filled up again. The old current page will be
        // the next page.
        partitionLinkPageBefore(page, bucket->activePagesHead);
        bucket->activePagesHead = page;
        page->numAllocatedSlots = -page->numAllocatedSlots - 2;
        ASSERT(page->numAllocatedSlots == static_cast<int>(partitionBucketSlots(bucket) - 1));
        --bucket->numFullPages;
    }
}

void* partitionReallocGeneric(PartitionRoot* root, void* ptr, size_t newSize)
{
    RELEASE_ASSERT(newSize <= QuantizedAllocation::kMaxUnquantizedAllocation);
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
    return realloc(ptr, newSize);
#else
    size_t oldIndex;
    if (LIKELY(partitionPointerIsValid(root, ptr))) {
        void* realPtr = partitionCookieFreePointerAdjust(ptr);
        PartitionBucket* bucket = partitionPointerToPage(realPtr)->bucket;
        ASSERT(bucket->root == root);
        oldIndex = bucket - root->buckets();
    } else {
        oldIndex = root->numBuckets;
    }

    size_t allocSize = QuantizedAllocation::quantizedSize(newSize);
    allocSize = partitionCookieSizeAdjustAdd(allocSize);
    size_t newIndex = allocSize >> kBucketShift;
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
    size_t copySize = oldIndex << kBucketShift;
    copySize = partitionCookieSizeAdjustSubtract(copySize);
    if (newSize < copySize)
        copySize = newSize;
    memcpy(ret, ptr, copySize);
    partitionFreeGeneric(root, ptr);
    return ret;
#endif
}

#if CPU(32BIT)
unsigned char SuperPageBitmap::s_bitmap[1 << (32 - kSuperPageShift - 3)];

static int bitmapLock = 0;

void SuperPageBitmap::registerSuperPage(void* ptr)
{
    ASSERT(!isPointerInSuperPage(ptr));
    uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
    raw >>= kSuperPageShift;
    size_t byteIndex = raw >> 3;
    size_t bit = raw & 7;
    ASSERT(byteIndex < sizeof(s_bitmap));
    // The read/modify/write is not guaranteed atomic, so take a lock.
    spinLockLock(&bitmapLock);
    s_bitmap[byteIndex] |= (1 << bit);
    spinLockUnlock(&bitmapLock);
}

void SuperPageBitmap::unregisterSuperPage(void* ptr)
{
    ASSERT(isPointerInSuperPage(ptr));
    uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
    raw >>= kSuperPageShift;
    size_t byteIndex = raw >> 3;
    size_t bit = raw & 7;
    ASSERT(byteIndex < sizeof(s_bitmap));
    // The read/modify/write is not guaranteed atomic, so take a lock.
    spinLockLock(&bitmapLock);
    s_bitmap[byteIndex] &= ~(1 << bit);
    spinLockUnlock(&bitmapLock);
}
#endif

#ifndef NDEBUG

void partitionDumpStats(const PartitionRoot& root)
{
    size_t i;
    size_t totalLive = 0;
    size_t totalResident = 0;
    size_t totalFreeable = 0;
    for (i = 0; i < root.numBuckets; ++i) {
        const PartitionBucket& bucket = root.buckets()[i];
        if (bucket.activePagesHead == &bucket.root->seedPage && !bucket.freePagesHead && !bucket.numFullPages) {
            // Empty bucket with no freelist or full pages. Skip reporting it.
            continue;
        }
        size_t numFreePages = 0;
        const PartitionPage* freePages = bucket.freePagesHead;
        while (freePages) {
            ++numFreePages;
            freePages = freePages->next;
        }
        size_t bucketSlotSize = partitionBucketSize(&bucket);
        size_t bucketNumSlots = partitionBucketSlots(&bucket);
        size_t bucketUsefulStorage = bucketSlotSize * bucketNumSlots;
        size_t bucketWaste = bucket.pageSize - bucketUsefulStorage;
        size_t numActiveBytes = bucket.numFullPages * bucketUsefulStorage;
        size_t numResidentBytes = bucket.numFullPages * bucket.pageSize;
        size_t numFreeableBytes = 0;
        size_t numActivePages = 0;
        const PartitionPage* page = bucket.activePagesHead;
        do {
            if (page != &bucket.root->seedPage) {
                ++numActivePages;
                numActiveBytes += (page->numAllocatedSlots * bucketSlotSize);
                size_t pageBytesResident = (bucketNumSlots - page->numUnprovisionedSlots) * bucketSlotSize;
                // Round up to system page size.
                pageBytesResident = (pageBytesResident + kSystemPageOffsetMask) & kSystemPageBaseMask;
                numResidentBytes += pageBytesResident;
                if (!page->numAllocatedSlots)
                    numFreeableBytes += pageBytesResident;
            }
            page = page->next;
        } while (page != bucket.activePagesHead);
        totalLive += numActiveBytes;
        totalResident += numResidentBytes;
        totalFreeable += numFreeableBytes;
        printf("bucket size %zu (pageSize %zu waste %zu): %zu alloc/%zu commit/%zu freeable bytes, %zu/%zu/%zu full/active/free pages\n", bucketSlotSize, static_cast<size_t>(bucket.pageSize), bucketWaste, numActiveBytes, numResidentBytes, numFreeableBytes, static_cast<size_t>(bucket.numFullPages), numActivePages, numFreePages);
    }
    printf("total live: %zu bytes\n", totalLive);
    printf("total resident: %zu bytes\n", totalResident);
    printf("total freeable: %zu bytes\n", totalFreeable);
    fflush(stdout);
}

#endif // !NDEBUG

} // namespace WTF


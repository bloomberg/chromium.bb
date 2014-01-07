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
COMPILE_ASSERT(WTF::kPageMetadataSize * WTF::kNumPartitionPagesPerSuperPage <= WTF::kSystemPageSize, page_metadata_fits_in_hole);
// Check that some of our zanier calculations worked out as expected.
COMPILE_ASSERT(WTF::kGenericSmallestBucket == 8, generic_smallest_bucket);
COMPILE_ASSERT(WTF::kGenericMaxBucketed == 30720, generic_max_bucketed);

namespace WTF {

static size_t partitionBucketNumSystemPages(size_t size)
{
    // This works out reasonably for the current bucket sizes of the generic
    // allocator, and the current values of partition page size and constants.
    // Specifically, we have enough room to always pack the slots perfectly into
    // some number of system pages. The only waste is the waste associated with     // unfaulted pages (i.e. wasted address space).
    // TODO: we end up using a lot of system pages for very small sizes. For
    // example, we'll use 12 system pages for slot size 24. The slot size is
    // so small that the waste would be tiny with just 4, or 1, system pages.
    // Later, we can investigate whether there are anti-fragmentation benefits
    // to using fewer system pages.
    double bestWasteRatio = 1.0f;
    size_t bestPages = 0;
    ASSERT(size <= kMaxSystemPagesPerSlotSpan * kSystemPageSize);
    for (size_t i = kNumSystemPagesPerPartitionPage - 1; i <= kMaxSystemPagesPerSlotSpan; ++i) {
        size_t pageSize = kSystemPageSize * i;
        size_t numSlots = pageSize / size;
        size_t waste = pageSize - (numSlots * size);
        // Leaving a page unfaulted is not free; the page will occupy an empty page table entry.
        // Make a simple attempt to account for that.
        size_t numRemainderPages = i & (kNumSystemPagesPerPartitionPage - 1);
        size_t numUnfaultedPages = numRemainderPages ? (kNumSystemPagesPerPartitionPage - numRemainderPages) : 0;
        waste += sizeof(void*) * numUnfaultedPages;
        double wasteRatio = (double) waste / (double) pageSize;
        if (wasteRatio < bestWasteRatio) {
            bestWasteRatio = wasteRatio;
            bestPages = i;
        }
    }
    ASSERT(bestPages > 0);
    return bestPages;
}

static ALWAYS_INLINE void partitionPageMarkFree(PartitionPage* page)
{
    ASSERT(!partitionPageIsFree(page));
    page->flags |= kPartitionPageFlagFree;
}

static void parititonAllocBaseInit(PartitionRootBase* root)
{
    ASSERT(!root->initialized);
    root->initialized = true;
    root->totalSizeOfSuperPages = 0;
    root->nextSuperPage = 0;
    root->nextPartitionPage = 0;
    root->nextPartitionPageEnd = 0;
    root->currentExtent = &root->firstExtent;
    root->firstExtent.superPageBase = 0;
    root->firstExtent.superPagesEnd = 0;
    root->firstExtent.next = 0;
    root->seedPage.bucket = &root->seedBucket;
    root->seedPage.activePageNext = 0;
    root->seedPage.numAllocatedSlots = 0;
    root->seedPage.numUnprovisionedSlots = 0;
    root->seedPage.flags = 0;
    partitionPageSetFreelistHead(&root->seedPage, 0);
    // We mark the seed page as free to make sure it is skipped by our logic to
    // find a new active page.
    partitionPageMarkFree(&root->seedPage);

    root->seedBucket.activePagesHead = 0;
    root->seedBucket.freePagesHead = 0;
    root->seedBucket.numFullPages = 0;
#ifndef NDEBUG
    root->seedBucket.root = root;
#endif
}

static void partitionBucketInitBase(PartitionBucket* bucket, PartitionRootBase* root)
{
    bucket->activePagesHead = &root->seedPage;
    bucket->freePagesHead = 0;
    bucket->numFullPages = 0;
    bucket->numSystemPagesPerSlotSpan = partitionBucketNumSystemPages(bucket->slotSize);
#ifndef NDEBUG
    bucket->root = root;
#endif
}

void partitionAllocInit(PartitionRoot* root, size_t numBuckets, size_t maxAllocation)
{
    parititonAllocBaseInit(root);

    root->numBuckets = numBuckets;
    root->maxAllocation = maxAllocation;
    size_t i;
    for (i = 0; i < root->numBuckets; ++i) {
        PartitionBucket* bucket = &root->buckets()[i];
        if (!i)
            bucket->slotSize = kAllocationGranularity;
        else
            bucket->slotSize = i << kBucketShift;
        partitionBucketInitBase(bucket, root);
    }
}

void partitionAllocGenericInit(PartitionRootGeneric* root)
{
    parititonAllocBaseInit(root);

    root->lock = 0;
    root->pagedBucket.activePagesHead = &root->seedPage;
    root->pagedBucket.freePagesHead = 0;
    root->pagedBucket.slotSize = 0;
    root->pagedBucket.numSystemPagesPerSlotSpan = 0;
    root->pagedBucket.numFullPages = 0;

    // Precalculate some shift and mask constants used in the hot path.
    // Example: malloc(41) == 101001 binary.
    // Order is 6 (1 << 6-1)==32 is highest bit set.
    // orderIndex is the next three MSB == 010 == 2.
    // subOrderIndexMask is a mask for the remaining bits == 11 (masking to 01 for the subOrderIndex).
    size_t order;
    for (order = 0; order <= kBitsPerSizet; ++order) {
        size_t orderIndexShift;
        if (order < kGenericNumBucketsPerOrderBits + 1)
            orderIndexShift = 0;
        else
            orderIndexShift = order - (kGenericNumBucketsPerOrderBits + 1);
        root->orderIndexShifts[order] = orderIndexShift;
        size_t subOrderIndexMask;
        if (order == kBitsPerSizet) {
            // This avoids invoking undefined behavior for an excessive shift.
            subOrderIndexMask = static_cast<size_t>(-1) >> (kGenericNumBucketsPerOrderBits + 1);
        } else {
            subOrderIndexMask = ((1 << order) - 1) >> (kGenericNumBucketsPerOrderBits + 1);
        }
        root->orderSubIndexMasks[order] = subOrderIndexMask;
    }

    // Set up the actual usable buckets first.
    // Note that typical values (i.e. min allocation size of 8) will result in
    // invalid buckets (size==9 etc. or more generally, size is not a multiple
    // of the smallest allocation granularity).
    // We avoid them in the bucket lookup map, but we tolerate them to keep the
    // code simpler and the structures more generic.
    size_t i, j;
    size_t currentSize = kGenericSmallestBucket;
    size_t currentIncrement = kGenericSmallestBucket >> kGenericNumBucketsPerOrderBits;
    PartitionBucket* bucket = &root->buckets[0];
    for (i = 0; i < kGenericNumBucketedOrders; ++i) {
        for (j = 0; j < kGenericNumBucketsPerOrder; ++j) {
            bucket->slotSize = currentSize;
            partitionBucketInitBase(bucket, root);
            // Disable invalid buckets so that touching them faults.
            if (currentSize % kGenericSmallestBucket)
                bucket->activePagesHead = 0;
            currentSize += currentIncrement;
            ++bucket;
        }
        currentIncrement <<= 1;
    }
    ASSERT(currentSize == 1 << kGenericMaxBucketedOrder);
    ASSERT(bucket == &root->buckets[0] + (kGenericNumBucketedOrders * kGenericNumBucketsPerOrder));

    // Then set up the fast size -> bucket lookup table.
    bucket = &root->buckets[0];
    PartitionBucket** bucketPtr = &root->bucketLookups[0];
    for (order = 0; order <= kBitsPerSizet; ++order) {
        for (j = 0; j < kGenericNumBucketsPerOrder; ++j) {
            if (order < kGenericMinBucketedOrder) {
                // Use the bucket of finest granularity for malloc(0) etc.
                *bucketPtr++ = &root->buckets[0];
            } else if (order > kGenericMaxBucketedOrder) {
                *bucketPtr++ = &root->pagedBucket;
            } else {
                PartitionBucket* validBucket = bucket;
                // Skip over invalid buckets.
                while (validBucket->slotSize % kGenericSmallestBucket)
                    validBucket++;
                *bucketPtr++ = validBucket;
                bucket++;
            }
        }
    }
    ASSERT(bucket == &root->buckets[0] + (kGenericNumBucketedOrders * kGenericNumBucketsPerOrder));
    ASSERT(bucketPtr == &root->bucketLookups[0] + ((kBitsPerSizet + 1) * kGenericNumBucketsPerOrder));
    // And there's one last bucket lookup that will be hit for e.g. malloc(-1),
    // which tries to overflow to a non-existant order.
    *bucketPtr = &root->pagedBucket;
}

static bool partitionAllocShutdownBucket(PartitionBucket* bucket)
{
    // Failure here indicates a memory leak.
    bool noLeaks = !bucket->numFullPages;
    PartitionPage* page = bucket->activePagesHead;
    while (page) {
        if (!partitionPageIsFree(page) && page->numAllocatedSlots)
            noLeaks = false;
        page = page->activePageNext;
    }

    return noLeaks;
}

static void partitionAllocBaseShutdown(PartitionRootBase* root)
{
    ASSERT(root->initialized);
    root->initialized = false;

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
}

bool partitionAllocShutdown(PartitionRoot* root)
{
    bool noLeaks = true;
    size_t i;
    for (i = 0; i < root->numBuckets; ++i) {
        PartitionBucket* bucket = &root->buckets()[i];
        if (!partitionAllocShutdownBucket(bucket))
            noLeaks = false;
    }

    partitionAllocBaseShutdown(root);
    return noLeaks;
}


bool partitionAllocGenericShutdown(PartitionRootGeneric* root)
{
    bool noLeaks = true;
    size_t i;
    for (i = 0; i < kGenericNumBucketedOrders * kGenericNumBucketsPerOrder; ++i) {
        PartitionBucket* bucket = &root->buckets[i];
        if (!partitionAllocShutdownBucket(bucket))
            noLeaks = false;
    }
    partitionAllocBaseShutdown(root);
    return noLeaks;
}

static ALWAYS_INLINE void* partitionAllocPartitionPages(PartitionRootBase* root, size_t numPartitionPages)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(root->nextPartitionPage) % kPartitionPageSize));
    ASSERT(!(reinterpret_cast<uintptr_t>(root->nextPartitionPageEnd) % kPartitionPageSize));
    RELEASE_ASSERT(numPartitionPages <= kNumPartitionPagesPerSuperPage);
    size_t totalSize = kPartitionPageSize * numPartitionPages;
    size_t numPartitionPagesLeft = (root->nextPartitionPageEnd - root->nextPartitionPage) >> kPartitionPageShift;
    if (LIKELY(numPartitionPagesLeft >= numPartitionPages)) {
        // In this case, we can still hand out pages from the current super page
        // allocation.
        char* ret = root->nextPartitionPage;
        root->nextPartitionPage += totalSize;
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
    root->nextPartitionPage = ret + totalSize;
    root->nextPartitionPageEnd = root->nextSuperPage - kPartitionPageSize;
    // Make the first partition page in the super page a guard page, but leave a
    // hole in the middle.
    // This is where we put page metadata and also a tiny amount of extent
    // metadata.
    setSystemPagesInaccessible(superPage, kSystemPageSize);
    setSystemPagesInaccessible(superPage + (kSystemPageSize * 2), kPartitionPageSize - (kSystemPageSize * 2));
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
    decommitSystemPages(addr, page->bucket->numSystemPagesPerSlotSpan * kSystemPageSize);
}

static ALWAYS_INLINE size_t partitionBucketSlots(const PartitionBucket* bucket)
{
    return (bucket->numSystemPagesPerSlotSpan * kSystemPageSize) / bucket->slotSize;
}

static ALWAYS_INLINE size_t partitionBucketPartitionPages(const PartitionBucket* bucket)
{
    return (bucket->numSystemPagesPerSlotSpan + (kNumSystemPagesPerPartitionPage - 1)) / kNumSystemPagesPerPartitionPage;
}

static ALWAYS_INLINE void partitionPageReset(PartitionPage* page, PartitionBucket* bucket)
{
    ASSERT(page != &bucket->root->seedPage);
    page->flags = 0;
    page->numAllocatedSlots = 0;
    page->numUnprovisionedSlots = partitionBucketSlots(bucket);
    ASSERT(page->numUnprovisionedSlots);
    page->bucket = bucket;
    // NULLing the freelist is not strictly necessary but it makes an ASSERT in partitionPageFillFreelist simpler.
    partitionPageSetFreelistHead(page, 0);
    page->pageOffset = 0;
    size_t numPartitionPages = partitionBucketPartitionPages(bucket);
    size_t i;
    char* pageCharPtr = reinterpret_cast<char*>(page);
    for (i = 1; i < numPartitionPages; ++i) {
        pageCharPtr += kPageMetadataSize;
        PartitionPage* secondaryPage = reinterpret_cast<PartitionPage*>(pageCharPtr);
        secondaryPage->pageOffset = i;
    }
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
    ASSERT(!partitionPageFreelistHead(page));

    size_t size = bucket->slotSize;
    char* base = reinterpret_cast<char*>(partitionPageToPointer(page));
    char* returnObject = base + (size * page->numAllocatedSlots);
    char* firstFreelistPointer = returnObject + size;
    char* firstFreelistPointerExtent = firstFreelistPointer + sizeof(PartitionFreelistEntry*);
    // Our goal is to fault as few system pages as possible. We calculate the
    // page containing the "end" of the returned slot, and then allow freelist
    // pointers to be written up to the end of that page.
    char* subPageLimit = reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(firstFreelistPointer) + kSystemPageOffsetMask) & kSystemPageBaseMask);
    char* slotsLimit = returnObject + (size * page->numUnprovisionedSlots);
    char* freelistLimit = subPageLimit;
    if (UNLIKELY(slotsLimit < freelistLimit))
        freelistLimit = slotsLimit;

    size_t numNewFreelistEntries = 0;
    if (LIKELY(firstFreelistPointerExtent <= freelistLimit)) {
        // Only consider used space in the slot span. If we consider wasted
        // space, we may get an off-by-one when a freelist pointer fits in the
        // wasted space, but a slot does not.
        // We know we can fit at least one freelist pointer.
        numNewFreelistEntries = 1;
        // Any further entries require space for the whole slot span.
        numNewFreelistEntries += (freelistLimit - firstFreelistPointerExtent) / size;
    }

    // We always return an object slot -- that's the +1 below.
    // We do not neccessarily create any new freelist entries, because we cross sub page boundaries frequently for large bucket sizes.
    ASSERT(numNewFreelistEntries + 1 <= numSlots);
    numSlots -= (numNewFreelistEntries + 1);
    page->numUnprovisionedSlots = numSlots;
    page->numAllocatedSlots++;

    if (LIKELY(numNewFreelistEntries)) {
        char* freelistPointer = firstFreelistPointer;
        PartitionFreelistEntry* entry = reinterpret_cast<PartitionFreelistEntry*>(freelistPointer);
        partitionPageSetFreelistHead(page, entry);
        while (--numNewFreelistEntries) {
            freelistPointer += size;
            PartitionFreelistEntry* nextEntry = reinterpret_cast<PartitionFreelistEntry*>(freelistPointer);
            entry->next = partitionFreelistMask(nextEntry);
            entry = nextEntry;
        }
        entry->next = partitionFreelistMask(0);
    } else {
        partitionPageSetFreelistHead(page, 0);
    }
    return returnObject;
}

// This helper function scans the active page list for a suitable new active
// page, starting at the page passed in. When it finds a suitable new active
// page (one that has free slots), it is also set as the new active page. If
// there is no suitable new active page, the current active page is set to null.
static ALWAYS_INLINE void partitionSetNewActivePage(PartitionBucket* bucket, PartitionPage* page)
{
    ASSERT(page == &bucket->root->seedPage || page->bucket == bucket);
    for (; page; page = page->activePageNext) {
        // Skip over free pages. We need this check first so that we can trust
        // that the page union field really containts a freelist pointer.
        if (UNLIKELY(partitionPageIsFree(page)))
            continue;

        // Page is usable if it has something on the freelist, or unprovisioned
        // slots that can be turned into a freelist.
        if (LIKELY(partitionPageFreelistHead(page) != 0) || LIKELY(page->numUnprovisionedSlots))
            break;
        // If we get here, we found a full page. Skip over it too, and also tag
        // it as full (via a negative value). We need it tagged so that free'ing
        // can tell, and move it back into the active page list.
        ASSERT(page->numAllocatedSlots == static_cast<int>(partitionBucketSlots(bucket)));
        page->numAllocatedSlots = -page->numAllocatedSlots;
        ++bucket->numFullPages;
    }

    bucket->activePagesHead = page;
}

static ALWAYS_INLINE void partitionPageSetFreePageNext(PartitionPage* page, PartitionPage* nextFree)
{
    ASSERT(partitionPageIsFree(page));
    page->u.freePageNext = nextFree;
}

static ALWAYS_INLINE PartitionPage* partitionPageFreePageNext(PartitionPage* page)
{
    ASSERT(partitionPageIsFree(page));
    return page->u.freePageNext;
}

static ALWAYS_INLINE bool partitionBucketIsPaged(PartitionBucket* bucket)
{
    return !bucket->slotSize;
}

void* partitionAllocSlowPath(PartitionRootBase* root, size_t size, PartitionBucket* bucket)
{
    // The slow path is called when the freelist is empty.
    ASSERT(!partitionPageFreelistHead(bucket->activePagesHead));

    // For the partitionAllocGeneric API, we have a bunch of buckets marked
    // as special cases. We bounce them through to the slow path so that we
    // can still have a blazing fast hot path due to lack of corner-case
    // branches.
    if (UNLIKELY(partitionBucketIsPaged(bucket))) {
        RELEASE_ASSERT(size <= INT_MAX);
        ASSERT(size > kGenericMaxBucketed);
        // This will be going away real soon now.
        return fastMalloc(size);
    }

    // First, look for a usable page in the existing active pages list.
    PartitionPage* page = bucket->activePagesHead;
    partitionSetNewActivePage(bucket, page);
    page = bucket->activePagesHead;
    if (LIKELY(page != 0)) {
        if (LIKELY(partitionPageFreelistHead(page) != 0)) {
            PartitionFreelistEntry* ret = partitionPageFreelistHead(page);
            partitionPageSetFreelistHead(page, partitionFreelistMask(ret->next));
            page->numAllocatedSlots++;
            return ret;
        }
        ASSERT(page->numUnprovisionedSlots);
        return partitionPageAllocAndFillFreelist(page);
    }

    // Second, look in our list of freed but reserved pages.
    PartitionPage* newPage = bucket->freePagesHead;
    if (LIKELY(newPage != 0)) {
        ASSERT(newPage != &bucket->root->seedPage);
        bucket->freePagesHead = partitionPageFreePageNext(newPage);
    } else {
        // Third. If we get here, we need a brand new page.
        size_t numPartitionPages = partitionBucketPartitionPages(bucket);
        void* rawNewPage = partitionAllocPartitionPages(root, numPartitionPages);
        // Skip the alignment check because it depends on page->bucket, which is not yet set.
        newPage = partitionPointerToPageNoAlignmentCheck(rawNewPage);
    }

    newPage->activePageNext = 0;
    partitionPageReset(newPage, bucket);
    bucket->activePagesHead = newPage;
    return partitionPageAllocAndFillFreelist(newPage);
}

static ALWAYS_INLINE void partitionPutPageOnFreelist(PartitionPage* page, PartitionBucket* bucket)
{
    partitionUnusePage(page);
    // We actually leave the freed page in the active list as well as
    // putting it on the free list. We'll evict it from the active list soon
    // enough in partitionAllocSlowPath. Pulling this trick enables us to
    // use a singly-linked page list for all cases, which is critical in
    // keeping the page metadata structure down to 32-bytes in size.
    partitionPageMarkFree(page);
    partitionPageSetFreePageNext(page, bucket->freePagesHead);
    bucket->freePagesHead = page;
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
            PartitionPage* newPage = 0;
            if (page->activePageNext) {
                partitionSetNewActivePage(bucket, page->activePageNext);
                newPage = bucket->activePagesHead;
            }

            ASSERT(newPage != page);
            if (UNLIKELY(!newPage)) {
                // We do not free the last active page in a bucket.
                // To do so is a serious performance issue as we can get into
                // a repeating state change between 0 and 1 objects of a given
                // size allocated; and each state change incurs either a system
                // call or a page fault, or more.
                page->activePageNext = 0;
                bucket->activePagesHead = page;
                return;
            }
            bucket->activePagesHead = newPage;
        }
        partitionPutPageOnFreelist(page, bucket);
    } else {
        // Ensure that the page is full. That's the only valid case if we
        // arrive here.
        ASSERT(page->numAllocatedSlots < 0);
        // Fully used page became partially used. It must be put back on the
        // non-full page list. Also make it the current page to increase the
        // chances of it being filled up again. The old current page will be
        // the next page.
        page->numAllocatedSlots = -page->numAllocatedSlots - 2;
        ASSERT(page->numAllocatedSlots == static_cast<int>(partitionBucketSlots(bucket) - 1));
        // Special case: for a partition page with just a single slot, we want
        // to bounce it straight to the free list.
        if (UNLIKELY(page->numAllocatedSlots == 0)) {
            partitionPutPageOnFreelist(page, bucket);
        } else {
            page->activePageNext = bucket->activePagesHead;
            bucket->activePagesHead = page;
        }
        --bucket->numFullPages;
        // Note: there's an opportunity here to look to see if the old active
        // page is now freeable.
    }
}

void* partitionReallocGeneric(PartitionRootGeneric* root, void* ptr, size_t newSize)
{
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
    return realloc(ptr, newSize);
#else
    // TODO: note that tcmalloc will "ignore" a downsizing realloc() unless the
    // new size is a significant percentage smaller. We could do the same if we
    // determine it is a win.
    PartitionBucket* oldBucket = &root->pagedBucket;
    if (LIKELY(partitionPointerIsValid(root, ptr))) {
        void* realPtr = partitionCookieFreePointerAdjust(ptr);
        oldBucket = partitionPointerToPage(realPtr)->bucket;
        ASSERT(oldBucket->root == root);
    }

    size_t allocSize = partitionCookieSizeAdjustAdd(newSize);
    PartitionBucket* newBucket = partitionGenericSizeToBucket(root, allocSize);

    if (oldBucket == newBucket) {
        // Same bucket. But if the bucket represents the external allocator,
        // delegate to realloc on that allocator.
        if (oldBucket == &root->pagedBucket)
            return WTF::fastRealloc(ptr, newSize);
        return ptr;
    }
    // This realloc cannot be resized in-place. Sadness.
    void* ret = partitionAllocGeneric(root, newSize);
    size_t copySize;
    if (oldBucket == &root->pagedBucket) {
        ASSERT(partitionPointerIsValid(root, ret));
        // Special case, if we're downsizing into a bucket from a fastMalloc()
        // allocation, we don't know the old size. But the new size is by
        // definition smaller so we can just use that.
        copySize = newSize;
    } else {
        copySize = oldBucket->slotSize;
        copySize = partitionCookieSizeAdjustSubtract(copySize);
        if (newSize < copySize)
            copySize = newSize;
    }
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
        PartitionPage* freePages = bucket.freePagesHead;
        while (freePages) {
            ++numFreePages;
            freePages = partitionPageFreePageNext(freePages);
        }
        size_t bucketSlotSize = bucket.slotSize;
        size_t bucketNumSlots = partitionBucketSlots(&bucket);
        size_t bucketUsefulStorage = bucketSlotSize * bucketNumSlots;
        size_t bucketPageSize = bucket.numSystemPagesPerSlotSpan * kSystemPageSize;
        size_t bucketWaste = bucketPageSize - bucketUsefulStorage;
        size_t numActiveBytes = bucket.numFullPages * bucketUsefulStorage;
        size_t numResidentBytes = bucket.numFullPages * bucketPageSize;
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
            page = page->activePageNext;
        } while (page != bucket.activePagesHead);
        totalLive += numActiveBytes;
        totalResident += numResidentBytes;
        totalFreeable += numFreeableBytes;
        printf("bucket size %zu (pageSize %zu waste %zu): %zu alloc/%zu commit/%zu freeable bytes, %zu/%zu/%zu full/active/free pages\n", bucketSlotSize, bucketPageSize, bucketWaste, numActiveBytes, numResidentBytes, numFreeableBytes, static_cast<size_t>(bucket.numFullPages), numActivePages, numFreePages);
    }
    printf("total live: %zu bytes\n", totalLive);
    printf("total resident: %zu bytes\n", totalResident);
    printf("total freeable: %zu bytes\n", totalFreeable);
    fflush(stdout);
}

#endif // !NDEBUG

} // namespace WTF


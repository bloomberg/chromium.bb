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

#include "wtf/OwnArrayPtr.h"
#include "wtf/PageAllocator.h"
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>

#if OS(POSIX)
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif // OS(POSIX)

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

static PartitionAllocator<4096> allocator;

static const int kTestAllocSize = sizeof(void*);

static void TestSetup()
{
    allocator.init();
}

static void TestShutdown()
{
    // We expect no leaks in the general case. We have a test for leak
    // detection.
    EXPECT_TRUE(allocator.shutdown());
}

static WTF::PartitionPageHeader* GetFullPage(size_t size)
{
    size_t bucketIdx = size >> WTF::kBucketShift;
    WTF::PartitionBucket* bucket = &allocator.root()->buckets()[bucketIdx];
    size_t numSlots = (WTF::kPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / size;
    void* first = 0;
    void* last = 0;
    size_t i;
    for (i = 0; i < numSlots; ++i) {
        void* ptr = partitionAlloc(allocator.root(), size);
        EXPECT_TRUE(ptr);
        if (!i)
            first = ptr;
        else if (i == numSlots - 1)
            last = ptr;
    }
    EXPECT_EQ(reinterpret_cast<size_t>(first) & WTF::kPartitionPageBaseMask, reinterpret_cast<size_t>(last) & WTF::kPartitionPageBaseMask);
    EXPECT_EQ(numSlots, static_cast<size_t>(bucket->currPage->numAllocatedSlots));
    EXPECT_EQ(0, bucket->currPage->freelistHead);
    EXPECT_TRUE(bucket->currPage);
    EXPECT_TRUE(bucket->currPage != &allocator.root()->seedPage);
    return bucket->currPage;
}

static void FreeFullPage(WTF::PartitionPageHeader* page, size_t size)
{
    size_t numSlots = (WTF::kPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / size;
    EXPECT_EQ(numSlots, static_cast<size_t>(abs(page->numAllocatedSlots)));
    char* ptr = reinterpret_cast<char*>(page);
    ptr += sizeof(WTF::PartitionPageHeader);
    size_t i;
    for (i = 0; i < numSlots; ++i) {
        partitionFree(ptr);
        ptr += size;
    }
    EXPECT_EQ(0, page->numAllocatedSlots);
}

// Check that the most basic of allocate / free pairs work.
TEST(WTF_PartitionAlloc, Basic)
{
    TestSetup();
    size_t bucketIdx = kTestAllocSize >> WTF::kBucketShift;
    WTF::PartitionBucket* bucket = &allocator.root()->buckets()[bucketIdx];

    EXPECT_EQ(0, bucket->freePages);
    EXPECT_EQ(&bucket->root->seedPage, bucket->currPage);
    EXPECT_EQ(&bucket->root->seedPage, bucket->currPage->next);
    EXPECT_EQ(&bucket->root->seedPage, bucket->currPage->prev);

    void* ptr = partitionAlloc(allocator.root(), kTestAllocSize);
    EXPECT_TRUE(ptr);
    EXPECT_EQ(sizeof(WTF::PartitionPageHeader), reinterpret_cast<size_t>(ptr) & WTF::kPartitionPageOffsetMask);
    // Check that the offset appears to include a guard page.
    EXPECT_EQ(WTF::kPartitionPageSize + sizeof(WTF::PartitionPageHeader), reinterpret_cast<size_t>(ptr) & WTF::kSuperPageOffsetMask);

    partitionFree(ptr);
    // Expect that a just-freed page doesn't get tossed to the freelist.
    EXPECT_EQ(0, bucket->freePages);

    TestShutdown();
}

// Check that we can detect a memory leak.
TEST(WTF_PartitionAlloc, SimpleLeak)
{
    TestSetup();
    void* leakedPtr = partitionAlloc(allocator.root(), kTestAllocSize);
    EXPECT_FALSE(allocator.shutdown());
}

// Test multiple allocations, and freelist handling.
TEST(WTF_PartitionAlloc, MultiAlloc)
{
    TestSetup();

    char* ptr1 = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    char* ptr2 = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    EXPECT_TRUE(ptr1);
    EXPECT_TRUE(ptr2);
    ptrdiff_t diff = ptr2 - ptr1;
    EXPECT_EQ(kTestAllocSize, diff);

    // Check that we re-use the just-freed slot.
    partitionFree(ptr2);
    ptr2 = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    EXPECT_TRUE(ptr2);
    diff = ptr2 - ptr1;
    EXPECT_EQ(kTestAllocSize, diff);
    partitionFree(ptr1);
    ptr1 = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    EXPECT_TRUE(ptr1);
    diff = ptr2 - ptr1;
    EXPECT_EQ(kTestAllocSize, diff);

    char* ptr3 = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    EXPECT_TRUE(ptr3);
    diff = ptr3 - ptr1;
    EXPECT_EQ(kTestAllocSize * 2, diff);

    partitionFree(ptr1);
    partitionFree(ptr2);
    partitionFree(ptr3);

    TestShutdown();
}

// Test a bucket with multiple pages.
TEST(WTF_PartitionAlloc, MultiPages)
{
    TestSetup();
    size_t bucketIdx = kTestAllocSize >> WTF::kBucketShift;
    WTF::PartitionBucket* bucket = &allocator.root()->buckets()[bucketIdx];

    WTF::PartitionPageHeader* page = GetFullPage(kTestAllocSize);
    FreeFullPage(page, kTestAllocSize);
    EXPECT_EQ(0, bucket->freePages);
    EXPECT_EQ(page, bucket->currPage);
    EXPECT_EQ(page, page->next);
    EXPECT_EQ(page, page->prev);

    page = GetFullPage(kTestAllocSize);
    WTF::PartitionPageHeader* page2 = GetFullPage(kTestAllocSize);

    EXPECT_EQ(page2, bucket->currPage);

    // Fully free the non-current page, it should be freelisted.
    FreeFullPage(page, kTestAllocSize);
    EXPECT_EQ(0, page->numAllocatedSlots);
    EXPECT_TRUE(bucket->freePages);
    EXPECT_EQ(page, bucket->freePages->page);
    EXPECT_EQ(page2, bucket->currPage);

    // Allocate a new page, it should pull from the freelist.
    page = GetFullPage(kTestAllocSize);
    EXPECT_FALSE(bucket->freePages);
    EXPECT_EQ(page, bucket->currPage);

    FreeFullPage(page, kTestAllocSize);
    FreeFullPage(page2, kTestAllocSize);
    EXPECT_EQ(0, page->numAllocatedSlots);
    EXPECT_EQ(0, page2->numAllocatedSlots);

    TestShutdown();
}

// Test some finer aspects of internal page transitions.
TEST(WTF_PartitionAlloc, PageTransitions)
{
    TestSetup();
    size_t bucketIdx = kTestAllocSize >> WTF::kBucketShift;
    WTF::PartitionBucket* bucket = &allocator.root()->buckets()[bucketIdx];

    WTF::PartitionPageHeader* page1 = GetFullPage(kTestAllocSize);
    WTF::PartitionPageHeader* page2 = GetFullPage(kTestAllocSize);
    EXPECT_EQ(page2, bucket->currPage);
    EXPECT_EQ(page1, page2->next);
    EXPECT_EQ(page1, page2->prev);
    // Allocating another page at this point should cause us to scan over page1
    // (which is both full and NOT our current page), and evict it from the
    // freelist. Older code had a O(n^2) condition due to failure to do this.
    WTF::PartitionPageHeader* page3 = GetFullPage(kTestAllocSize);
    EXPECT_EQ(page3, bucket->currPage);
    EXPECT_EQ(page2, page3->next);
    EXPECT_EQ(page3, page2->next);

    // Work out a pointer into page2 and free it.
    char* ptr = reinterpret_cast<char*>(page2) + sizeof(WTF::PartitionPageHeader);
    partitionFree(ptr);
    // Trying to allocate at this time should cause us to cycle around to page2
    // and find the recently freed slot.
    char* newPtr = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    EXPECT_EQ(ptr, newPtr);
    EXPECT_EQ(page2, bucket->currPage);

    // Work out a pointer into page1 and free it. This should pull the page
    // back into the ring list of available pages.
    ptr = reinterpret_cast<char*>(page1) + sizeof(WTF::PartitionPageHeader);
    partitionFree(ptr);
    // This allocation should be satisfied by page1.
    newPtr = reinterpret_cast<char*>(partitionAlloc(allocator.root(), kTestAllocSize));
    EXPECT_EQ(ptr, newPtr);
    EXPECT_EQ(page1, bucket->currPage);

    FreeFullPage(page3, kTestAllocSize);
    FreeFullPage(page2, kTestAllocSize);
    FreeFullPage(page1, kTestAllocSize);

    TestShutdown();
}

// Test some corner cases relating to page transitions in the internal
// free page list metadata bucket.
TEST(WTF_PartitionAlloc, FreePageListPageTransitions)
{
    TestSetup();
    WTF::PartitionBucket* freePageBucket = &allocator.root()->buckets()[WTF::kFreePageBucket];
    size_t bucketIdx = kTestAllocSize >> WTF::kBucketShift;
    WTF::PartitionBucket* bucket = &allocator.root()->buckets()[bucketIdx];

    size_t numToFillFreeListPage = (WTF::kPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / sizeof(WTF::PartitionFreepagelistEntry);
    OwnArrayPtr<WTF::PartitionPageHeader*> pages = adoptArrayPtr(new WTF::PartitionPageHeader*[numToFillFreeListPage + 1]);
    size_t i;
    // The +1 is because we need to account for the fact that the current page
    // never gets thrown on the freelist.
    for (i = 0; i < numToFillFreeListPage + 1; ++i) {
        pages[i] = GetFullPage(kTestAllocSize);
    }
    EXPECT_EQ(pages[numToFillFreeListPage], bucket->currPage);
    for (i = 0; i < numToFillFreeListPage + 1; ++i) {
        FreeFullPage(pages[i], kTestAllocSize);
    }
    EXPECT_EQ(pages[numToFillFreeListPage], bucket->currPage);

    // At this moment, we should have filled an entire partition page full of
    // WTF::PartitionFreepagelistEntry, in the special free list entry bucket.
    EXPECT_EQ(numToFillFreeListPage, freePageBucket->currPage->numAllocatedSlots);
    EXPECT_EQ(0, freePageBucket->currPage->freelistHead);

    // Allocate / free a full couple of pages of a different bucket size so
    // we get control of a different free page list.
    WTF::PartitionPageHeader* page1 = GetFullPage(kTestAllocSize * 2);
    WTF::PartitionPageHeader* page2 = GetFullPage(kTestAllocSize * 2);
    FreeFullPage(page1, kTestAllocSize * 2);
    FreeFullPage(page2, kTestAllocSize * 2);

    // Now, we have a second page for free page objects, with a single entry
    // in it -- from a free page in the "kTestAllocSize * 2" bucket.
    EXPECT_EQ(1, freePageBucket->currPage->numAllocatedSlots);
    EXPECT_EQ(0, freePageBucket->freePages);

    // If we re-allocate all kTestAllocSize allocations, we'll pull all the
    // free pages and end up freeing the first page for free page objects.
    // It's getting a bit tricky but a nice re-entrancy is going on:
    // alloc(kTestAllocSize) -> pulls page from free page list ->
    // free(PartitionFreepagelistEntry) -> last entry in page freed ->
    // alloc(PartitionFreepagelistEntry).
    for (i = 0; i < numToFillFreeListPage + 1; ++i) {
        pages[i] = GetFullPage(kTestAllocSize);
    }
    EXPECT_EQ(pages[numToFillFreeListPage], bucket->currPage);
    EXPECT_EQ(2, freePageBucket->currPage->numAllocatedSlots);
    EXPECT_TRUE(freePageBucket->freePages);

    // As part of the final free-up, we'll test another re-entrancy:
    // free(kTestAllocSize) -> last entry in page freed ->
    // alloc(PartitionFreepagelistEntry) -> pulls page from free page list ->
    // free(PartitionFreepagelistEntry)
    for (i = 0; i < numToFillFreeListPage + 1; ++i) {
        FreeFullPage(pages[i], kTestAllocSize);
    }
    EXPECT_EQ(pages[numToFillFreeListPage], bucket->currPage);

    TestShutdown();
}

// Test a large series of allocations that cross more than one underlying
// 64KB super page allocation.
TEST(WTF_PartitionAlloc, MultiPageAllocs)
{
    TestSetup();
    // This is guaranteed to cross a super page boundary because the first
    // partition page "slot" will be taken up by a guard page.
    size_t numPagesNeeded = WTF::kSuperPageSize / WTF::kPartitionPageSize;
    EXPECT_GT(numPagesNeeded, 1u);
    OwnArrayPtr<WTF::PartitionPageHeader*> pages;
    pages = adoptArrayPtr(new WTF::PartitionPageHeader*[numPagesNeeded]);
    uintptr_t firstSuperPageBase = 0;
    size_t i;
    for (i = 0; i < numPagesNeeded; ++i) {
        pages[i] = GetFullPage(kTestAllocSize);
        if (!i)
            firstSuperPageBase = (reinterpret_cast<uintptr_t>(pages[i]) & WTF::kSuperPageBaseMask);
        if (i == numPagesNeeded - 1) {
            uintptr_t secondSuperPageBase = reinterpret_cast<uintptr_t>(pages[i]) & WTF::kSuperPageBaseMask;
            EXPECT_FALSE(secondSuperPageBase == firstSuperPageBase);
            // If the two super pages are contiguous, also check that we didn't
            // erroneously allocate a guard page for the second page.
            if (secondSuperPageBase == firstSuperPageBase + WTF::kSuperPageSize)
                EXPECT_EQ(0u, secondSuperPageBase & WTF::kSuperPageOffsetMask);
        }
    }
    for (i = 0; i < numPagesNeeded; ++i) {
        FreeFullPage(pages[i], kTestAllocSize);
    }

    TestShutdown();
}

// Test the generic allocation functions that can handle arbitrary sizes and
// reallocing etc.
TEST(WTF_PartitionAlloc, GenericAlloc)
{
    TestSetup();

    void* ptr = partitionAllocGeneric(allocator.root(), 1);
    EXPECT_TRUE(ptr);
    partitionFreeGeneric(allocator.root(), ptr, 1);
    ptr = partitionAllocGeneric(allocator.root(), PartitionAllocator<4096>::kMaxAllocation + 1);
    EXPECT_TRUE(ptr);
    partitionFreeGeneric(allocator.root(), ptr, PartitionAllocator<4096>::kMaxAllocation + 1);

    ptr = partitionAllocGeneric(allocator.root(), 1);
    EXPECT_TRUE(ptr);
    void* origPtr = ptr;
    char* charPtr = static_cast<char*>(ptr);
    *charPtr = 'A';

    // Change the size of the realloc, remaining inside the same bucket.
    void* newPtr = partitionReallocGeneric(allocator.root(), ptr, 1, 2);
    EXPECT_EQ(ptr, newPtr);
    newPtr = partitionReallocGeneric(allocator.root(), ptr, 2, 1);
    EXPECT_EQ(ptr, newPtr);
    newPtr = partitionReallocGeneric(allocator.root(), ptr, 1, WTF::kAllocationGranularity);
    EXPECT_EQ(ptr, newPtr);

    // Change the size of the realloc, switching buckets.
    newPtr = partitionReallocGeneric(allocator.root(), ptr, WTF::kAllocationGranularity, WTF::kAllocationGranularity + 1);
    EXPECT_NE(newPtr, ptr);
    // Check that the realloc copied correctly.
    char* newCharPtr = static_cast<char*>(newPtr);
    EXPECT_EQ(*newCharPtr, 'A');
    *newCharPtr = 'B';
    // The realloc moved. To check that the old allocation was freed, we can
    // do an alloc of the old allocation size and check that the old allocation
    // address is at the head of the freelist and reused.
    void* reusedPtr = partitionAllocGeneric(allocator.root(), 1);
    EXPECT_EQ(reusedPtr, origPtr);
    partitionFreeGeneric(allocator.root(), reusedPtr, 1);

    // Downsize the realloc.
    ptr = newPtr;
    newPtr = partitionReallocGeneric(allocator.root(), ptr, WTF::kAllocationGranularity + 1, 1);
    EXPECT_EQ(newPtr, origPtr);
    newCharPtr = static_cast<char*>(newPtr);
    EXPECT_EQ(*newCharPtr, 'B');
    *newCharPtr = 'C';

    // Upsize the realloc to outside the partition.
    ptr = newPtr;
    newPtr = partitionReallocGeneric(allocator.root(), ptr, 1, PartitionAllocator<4096>::kMaxAllocation + 1);
    EXPECT_NE(newPtr, ptr);
    newCharPtr = static_cast<char*>(newPtr);
    EXPECT_EQ(*newCharPtr, 'C');
    *newCharPtr = 'D';

    // Upsize and downsize the realloc, remaining outside the partition.
    ptr = newPtr;
    newPtr = partitionReallocGeneric(allocator.root(), ptr, PartitionAllocator<4096>::kMaxAllocation + 1, PartitionAllocator<4096>::kMaxAllocation * 10);
    newCharPtr = static_cast<char*>(newPtr);
    EXPECT_EQ(*newCharPtr, 'D');
    *newCharPtr = 'E';
    ptr = newPtr;
    newPtr = partitionReallocGeneric(allocator.root(), ptr, PartitionAllocator<4096>::kMaxAllocation * 10, PartitionAllocator<4096>::kMaxAllocation * 2);
    newCharPtr = static_cast<char*>(newPtr);
    EXPECT_EQ(*newCharPtr, 'E');
    *newCharPtr = 'F';

    // Downsize the realloc to inside the partition.
    ptr = newPtr;
    newPtr = partitionReallocGeneric(allocator.root(), ptr, PartitionAllocator<4096>::kMaxAllocation * 2, 1);
    EXPECT_NE(newPtr, ptr);
    EXPECT_EQ(newPtr, origPtr);
    newCharPtr = static_cast<char*>(newPtr);
    EXPECT_EQ(*newCharPtr, 'F');

    partitionFreeGeneric(allocator.root(), newPtr, 1);
    TestShutdown();
}

// Tests the handing out of freelists for partial pages.
TEST(WTF_PartitionAlloc, PartialPageFreelists)
{
    TestSetup();

    size_t bigSize = allocator.root()->maxAllocation;
    EXPECT_EQ(WTF::kSubPartitionPageSize - WTF::kAllocationGranularity, bigSize);
    size_t bucketIdx = bigSize >> WTF::kBucketShift;
    WTF::PartitionBucket* bucket = &allocator.root()->buckets()[bucketIdx];
    EXPECT_EQ(0, bucket->freePages);

    void* ptr = partitionAlloc(allocator.root(), bigSize);
    EXPECT_TRUE(ptr);

    WTF::PartitionPageHeader* page = reinterpret_cast<WTF::PartitionPageHeader*>(reinterpret_cast<size_t>(ptr) & WTF::kPartitionPageBaseMask);
    // The freelist should be empty as only one slot could be allocated without
    // touching more system pages.
    EXPECT_EQ(0, page->freelistHead);
    EXPECT_EQ(1, page->numAllocatedSlots);

    void* ptr2 = partitionAlloc(allocator.root(), bigSize);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(0, page->freelistHead);
    EXPECT_EQ(2, page->numAllocatedSlots);

    void* ptr3 = partitionAlloc(allocator.root(), bigSize);
    EXPECT_TRUE(ptr3);
    EXPECT_EQ(0, page->freelistHead);
    EXPECT_EQ(3, page->numAllocatedSlots);

    void* ptr4 = partitionAlloc(allocator.root(), bigSize);
    EXPECT_TRUE(ptr4);
    WTF::PartitionPageHeader* page2 = reinterpret_cast<WTF::PartitionPageHeader*>(reinterpret_cast<size_t>(ptr4) & WTF::kPartitionPageBaseMask);
    EXPECT_EQ(1, page2->numAllocatedSlots);

    partitionFree(ptr);
    partitionFree(ptr2);
    partitionFree(ptr3);
    partitionFree(ptr4);
    EXPECT_TRUE(bucket->freePages);
    EXPECT_EQ(page, bucket->freePages->page);
    EXPECT_TRUE(page2->freelistHead);
    EXPECT_EQ(0, page2->numAllocatedSlots);

    // And test a couple of sizes that do not cross kSubPartitionPageSize with a single allocation.
    size_t mediumSize = WTF::kSubPartitionPageSize / 2;
    bucketIdx = mediumSize >> WTF::kBucketShift;
    bucket = &allocator.root()->buckets()[bucketIdx];
    EXPECT_EQ(0, bucket->freePages);

    ptr = partitionAlloc(allocator.root(), mediumSize);
    EXPECT_TRUE(ptr);
    page = reinterpret_cast<WTF::PartitionPageHeader*>(reinterpret_cast<size_t>(ptr) & WTF::kPartitionPageBaseMask);
    EXPECT_EQ(1, page->numAllocatedSlots);
    EXPECT_EQ(((WTF::kPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / mediumSize) - 1, page->numUnprovisionedSlots);

    partitionFree(ptr);

    size_t smallSize = WTF::kSubPartitionPageSize / 4;
    bucketIdx = smallSize >> WTF::kBucketShift;
    bucket = &allocator.root()->buckets()[bucketIdx];
    EXPECT_EQ(0, bucket->freePages);

    ptr = partitionAlloc(allocator.root(), smallSize);
    EXPECT_TRUE(ptr);
    page = reinterpret_cast<WTF::PartitionPageHeader*>(reinterpret_cast<size_t>(ptr) & WTF::kPartitionPageBaseMask);
    EXPECT_EQ(1, page->numAllocatedSlots);
    size_t totalSlots = (WTF::kPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / smallSize;
    size_t firstPageSlots = (WTF::kSubPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / smallSize;
    EXPECT_EQ(totalSlots - firstPageSlots, page->numUnprovisionedSlots);

    partitionFree(ptr);
    EXPECT_TRUE(page->freelistHead);
    EXPECT_EQ(0, page->numAllocatedSlots);

    size_t verySmallSize = WTF::kAllocationGranularity;
    bucketIdx = verySmallSize >> WTF::kBucketShift;
    bucket = &allocator.root()->buckets()[bucketIdx];
    EXPECT_EQ(0, bucket->freePages);

    ptr = partitionAlloc(allocator.root(), verySmallSize);
    EXPECT_TRUE(ptr);
    page = reinterpret_cast<WTF::PartitionPageHeader*>(reinterpret_cast<size_t>(ptr) & WTF::kPartitionPageBaseMask);
    EXPECT_EQ(1, page->numAllocatedSlots);
    totalSlots = (WTF::kPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / verySmallSize;
    firstPageSlots = (WTF::kSubPartitionPageSize - sizeof(WTF::PartitionPageHeader)) / verySmallSize;
    EXPECT_EQ(totalSlots - firstPageSlots, page->numUnprovisionedSlots);

    partitionFree(ptr);
    EXPECT_TRUE(page->freelistHead);
    EXPECT_EQ(0, page->numAllocatedSlots);

    TestShutdown();
}

#if OS(POSIX)

// Test correct handling if our mapping collides with another.
TEST(WTF_PartitionAlloc, MappingCollision)
{
    TestSetup();

    WTF::PartitionPageHeader* page1 = GetFullPage(kTestAllocSize);
    char* pageBase = reinterpret_cast<char*>(page1);
    // Map a single system page either side of the mapping for our allocations,
    // with the goal of tripping up alignment of the next mapping.
    void* map1 = mmap(pageBase - WTF::kSystemPageSize, WTF::kSystemPageSize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    EXPECT_TRUE(map1 && map1 != MAP_FAILED);
    void* map2 = mmap(pageBase + WTF::kSuperPageSize, WTF::kSystemPageSize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    EXPECT_TRUE(map2 && map2 != MAP_FAILED);

    WTF::PartitionPageHeader* page2 = GetFullPage(kTestAllocSize);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(page2) & WTF::kPartitionPageOffsetMask);
    FreeFullPage(page2, kTestAllocSize);

    FreeFullPage(page1, kTestAllocSize);
    munmap(map1, WTF::kSystemPageSize);
    munmap(map2, WTF::kSystemPageSize);

    TestShutdown();
}

#endif // OS(POSIX)

} // namespace

#endif // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

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

#ifndef Heap_h
#define Heap_h

#include "platform/PlatformExport.h"
#include "platform/heap/AddressSanitizer.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebThread.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/Forward.h"
#include "wtf/HashCountedSet.h"
#include "wtf/LinkedHashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/PageAllocator.h"
#include <stdint.h>

namespace blink {

const size_t blinkPageSizeLog2 = 17;
const size_t blinkPageSize = 1 << blinkPageSizeLog2;
const size_t blinkPageOffsetMask = blinkPageSize - 1;
const size_t blinkPageBaseMask = ~blinkPageOffsetMask;

// We allocate pages at random addresses but in groups of
// blinkPagesPerRegion at a given random address. We group pages to
// not spread out too much over the address space which would blow
// away the page tables and lead to bad performance.
const size_t blinkPagesPerRegion = 10;

// Double precision floats are more efficient when 8 byte aligned, so we 8 byte
// align all allocations even on 32 bit.
const size_t allocationGranularity = 8;
const size_t allocationMask = allocationGranularity - 1;
const size_t objectStartBitMapSize = (blinkPageSize + ((8 * allocationGranularity) - 1)) / (8 * allocationGranularity);
const size_t reservedForObjectBitMap = ((objectStartBitMapSize + allocationMask) & ~allocationMask);
const size_t maxHeapObjectSizeLog2 = 27;
const size_t maxHeapObjectSize = 1 << maxHeapObjectSizeLog2;
const size_t largeObjectSizeThreshold = blinkPageSize / 2;

const uint8_t freelistZapValue = 42;
const uint8_t finalizedZapValue = 24;
// The orphaned zap value must be zero in the lowest bits to allow for using
// the mark bit when tracing.
const uint8_t orphanedZapValue = 240;
// A zap value for vtables should be < 4K to ensure it cannot be
// used for dispatch.
static const intptr_t zappedVTable = 0xd0d;

#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define FILL_ZERO_IF_PRODUCTION(address, size) do { } while (false)
#define FILL_ZERO_IF_NOT_PRODUCTION(address, size) memset((address), 0, (size))
#else
#define FILL_ZERO_IF_PRODUCTION(address, size) memset((address), 0, (size))
#define FILL_ZERO_IF_NOT_PRODUCTION(address, size) do { } while (false)
#endif

class CallbackStack;
class PageMemory;
class NormalPageHeap;
template<ThreadAffinity affinity> class ThreadLocalPersistents;
template<typename T, typename RootsAccessor = ThreadLocalPersistents<ThreadingTrait<T>::Affinity>> class Persistent;

#if ENABLE(GC_PROFILING)
class TracedValue;
#endif

// HeapObjectHeader is 4 byte (32 bit) that has the following layout:
//
// | gcInfoIndex (15 bit) | size (14 bit) | dead bit (1 bit) | freed bit (1 bit) | mark bit (1 bit) |
//
// - For non-large objects, 14 bit is enough for |size| because the blink
//   page size is 2^17 byte and each object is guaranteed to be aligned with
//   2^3 byte.
// - For large objects, |size| is 0. The actual size of a large object is
//   stored in LargeObjectPage::m_payloadSize.
// - 15 bit is enough for gcInfoIndex because there are less than 2^15 types
//   in Blink.
const size_t headerGCInfoIndexShift = 17;
const size_t headerGCInfoIndexMask = (static_cast<size_t>((1 << 15) - 1)) << headerGCInfoIndexShift;
const size_t headerSizeMask = (static_cast<size_t>((1 << 14) - 1)) << 3;
const size_t headerMarkBitMask = 1;
const size_t headerFreedBitMask = 2;
// The dead bit is used for objects that have gone through a GC marking, but did
// not get swept before a new GC started. In that case we set the dead bit on
// objects that were not marked in the previous GC to ensure we are not tracing
// them via a conservatively found pointer. Tracing dead objects could lead to
// tracing of already finalized objects in another thread's heap which is a
// use-after-free situation.
const size_t headerDeadBitMask = 4;
// On free-list entries we reuse the dead bit to distinguish a normal free-list
// entry from one that has been promptly freed.
const size_t headerPromptlyFreedBitMask = headerFreedBitMask | headerDeadBitMask;
const size_t largeObjectSizeInHeader = 0;
const size_t gcInfoIndexForFreeListHeader = 0;
const size_t nonLargeObjectPageSizeMax = 1 << 17;

static_assert(nonLargeObjectPageSizeMax >= blinkPageSize, "max size supported by HeapObjectHeader must at least be blinkPageSize");

class PLATFORM_EXPORT HeapObjectHeader {
public:
    // If gcInfoIndex is 0, this header is interpreted as a free list header.
    NO_SANITIZE_ADDRESS
    HeapObjectHeader(size_t size, size_t gcInfoIndex)
    {
#if ENABLE(ASSERT)
        m_magic = magic;
#endif
#if ENABLE(GC_PROFILING)
        m_age = 0;
#endif
        // sizeof(HeapObjectHeader) must be equal to or smaller than
        // allocationGranurarity, because HeapObjectHeader is used as a header
        // for an freed entry.  Given that the smallest entry size is
        // allocationGranurarity, HeapObjectHeader must fit into the size.
        static_assert(sizeof(HeapObjectHeader) <= allocationGranularity, "size of HeapObjectHeader must be smaller than allocationGranularity");
#if CPU(64BIT)
        static_assert(sizeof(HeapObjectHeader) == 8, "size of HeapObjectHeader must be 8 byte aligned");
#endif

        ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
        ASSERT(size < nonLargeObjectPageSizeMax);
        ASSERT(!(size & allocationMask));
        m_encoded = (gcInfoIndex << headerGCInfoIndexShift) | size | (gcInfoIndex ? 0 : headerFreedBitMask);
    }

    NO_SANITIZE_ADDRESS
    bool isFree() const { return m_encoded & headerFreedBitMask; }
    NO_SANITIZE_ADDRESS
    bool isPromptlyFreed() const { return (m_encoded & headerPromptlyFreedBitMask) == headerPromptlyFreedBitMask; }
    NO_SANITIZE_ADDRESS
    void markPromptlyFreed() { m_encoded |= headerPromptlyFreedBitMask; }
    size_t size() const;

    NO_SANITIZE_ADDRESS
    size_t gcInfoIndex() const { return (m_encoded & headerGCInfoIndexMask) >> headerGCInfoIndexShift; }
    NO_SANITIZE_ADDRESS
    void setSize(size_t size) { m_encoded = size | (m_encoded & ~headerSizeMask); }
    bool isMarked() const;
    void mark();
    void unmark();
    void markDead();
    bool isDead() const;

    Address payload();
    size_t payloadSize();
    Address payloadEnd();

    void checkHeader() const;
#if ENABLE(ASSERT)
    // Zap magic number with a new magic number that means there was once an
    // object allocated here, but it was freed because nobody marked it during
    // GC.
    void zapMagic();
#endif

    void finalize(Address, size_t);
    static HeapObjectHeader* fromPayload(const void*);

    static const uint16_t magic = 0xfff1;
    static const uint16_t zappedMagic = 0x4321;

#if ENABLE(GC_PROFILING)
    NO_SANITIZE_ADDRESS
    size_t encodedSize() const { return m_encoded; }

    NO_SANITIZE_ADDRESS
    size_t age() const { return m_age; }

    NO_SANITIZE_ADDRESS
    void incrementAge()
    {
        if (m_age < maxHeapObjectAge)
            m_age++;
    }
#endif

#if !ENABLE(ASSERT) && !ENABLE(GC_PROFILING) && CPU(64BIT)
    // This method is needed just to avoid compilers from removing m_padding.
    uint64_t unusedMethod() const { return m_padding; }
#endif

private:
    uint32_t m_encoded;
#if ENABLE(ASSERT)
    uint16_t m_magic;
#endif
#if ENABLE(GC_PROFILING)
    uint8_t m_age;
#endif

    // In 64 bit architectures, we intentionally add 4 byte padding immediately
    // after the HeapHeaderObject. This is because:
    //
    // | HeapHeaderObject (4 byte) | padding (4 byte) | object payload (8 * n byte) |
    // ^8 byte aligned                                ^8 byte aligned
    //
    // is better than:
    //
    // | HeapHeaderObject (4 byte) | object payload (8 * n byte) | padding (4 byte) |
    // ^4 byte aligned             ^8 byte aligned               ^4 byte aligned
    //
    // since the former layout aligns both header and payload to 8 byte.
#if !ENABLE(ASSERT) && !ENABLE(GC_PROFILING) && CPU(64BIT)
    uint32_t m_padding;
#endif
};

inline HeapObjectHeader* HeapObjectHeader::fromPayload(const void* payload)
{
    Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
    HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(addr - sizeof(HeapObjectHeader));
    return header;
}

class FreeListEntry final : public HeapObjectHeader {
public:
    NO_SANITIZE_ADDRESS
    explicit FreeListEntry(size_t size)
        : HeapObjectHeader(size, gcInfoIndexForFreeListHeader)
        , m_next(nullptr)
    {
#if ENABLE(ASSERT) && !defined(ADDRESS_SANITIZER)
        // Zap free area with asterisks, aka 0x2a2a2a2a.
        // For ASan don't zap since we keep accounting in the freelist entry.
        for (size_t i = sizeof(*this); i < size; ++i)
            reinterpret_cast<Address>(this)[i] = freelistZapValue;
        ASSERT(size >= sizeof(HeapObjectHeader));
        zapMagic();
#endif
    }

    Address address() { return reinterpret_cast<Address>(this); }

    NO_SANITIZE_ADDRESS
    void unlink(FreeListEntry** prevNext)
    {
        *prevNext = m_next;
        m_next = nullptr;
    }

    NO_SANITIZE_ADDRESS
    void link(FreeListEntry** prevNext)
    {
        m_next = *prevNext;
        *prevNext = this;
    }

    NO_SANITIZE_ADDRESS
    FreeListEntry* next() const { return m_next; }

    NO_SANITIZE_ADDRESS
    void append(FreeListEntry* next)
    {
        ASSERT(!m_next);
        m_next = next;
    }

#if defined(ADDRESS_SANITIZER)
    NO_SANITIZE_ADDRESS
    bool shouldAddToFreeList()
    {
        // Init if not already magic.
        if ((m_asanMagic & ~asanDeferMemoryReuseMask) != asanMagic) {
            m_asanMagic = asanMagic | asanDeferMemoryReuseCount;
            return false;
        }
        // Decrement if count part of asanMagic > 0.
        if (m_asanMagic & asanDeferMemoryReuseMask)
            m_asanMagic--;
        return !(m_asanMagic & asanDeferMemoryReuseMask);
    }
#endif

private:
    FreeListEntry* m_next;
#if defined(ADDRESS_SANITIZER)
    unsigned m_asanMagic;
#endif
};

// Blink heap pages are set up with a guard page before and after the payload.
inline size_t blinkPagePayloadSize()
{
    return blinkPageSize - 2 * WTF::kSystemPageSize;
}

// Blink heap pages are aligned to the Blink heap page size.
// Therefore, the start of a Blink page can be obtained by
// rounding down to the Blink page size.
inline Address roundToBlinkPageStart(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) & blinkPageBaseMask);
}

inline Address roundToBlinkPageEnd(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address - 1) & blinkPageBaseMask) + blinkPageSize;
}

// Masks an address down to the enclosing blink page base address.
inline Address blinkPageAddress(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) & blinkPageBaseMask);
}

#if ENABLE(ASSERT)

// Sanity check for a page header address: the address of the page
// header should be OS page size away from being Blink page size
// aligned.
inline bool isPageHeaderAddress(Address address)
{
    return !((reinterpret_cast<uintptr_t>(address) & blinkPageOffsetMask) - WTF::kSystemPageSize);
}
#endif

// BasePage is a base class for NormalPage and LargeObjectPage.
//
// - NormalPage is a page whose size is |blinkPageSize|. NormalPage can contain
//   multiple objects in the page. An object whose size is smaller than
//   |largeObjectSizeThreshold| is stored in NormalPage.
//
// - LargeObjectPage is a page that contains only one object. The object size
//   is arbitrary. An object whose size is larger than |blinkPageSize| is stored
//   as a single project in LargeObjectPage.
//
// Note: An object whose size is between |largeObjectSizeThreshold| and
// |blinkPageSize| can go to either of NormalPage or LargeObjectPage.
class BasePage {
public:
    BasePage(PageMemory*, BaseHeap*);
    virtual ~BasePage() { }

    void link(BasePage** previousNext)
    {
        m_next = *previousNext;
        *previousNext = this;
    }
    void unlink(BasePage** previousNext)
    {
        *previousNext = m_next;
        m_next = nullptr;
    }
    BasePage* next() const { return m_next; }

    // virtual methods are slow. So performance-sensitive methods
    // should be defined as non-virtual methods on NormalPage and LargeObjectPage.
    // The following methods are not performance-sensitive.
    virtual size_t objectPayloadSizeForTesting() = 0;
    virtual bool isEmpty() = 0;
    virtual void removeFromHeap() = 0;
    virtual void sweep() = 0;
    virtual void markUnmarkedObjectsDead() = 0;
    // Check if the given address points to an object in this
    // heap page. If so, find the start of that object and mark it
    // using the given Visitor. Otherwise do nothing. The pointer must
    // be within the same aligned blinkPageSize as the this-pointer.
    //
    // This is used during conservative stack scanning to
    // conservatively mark all objects that could be referenced from
    // the stack.
    virtual void checkAndMarkPointer(Visitor*, Address) = 0;
    virtual void markOrphaned();
#if ENABLE(GC_PROFILING)
    virtual const GCInfo* findGCInfo(Address) = 0;
    virtual void snapshot(TracedValue*, ThreadState::SnapshotInfo*) = 0;
    virtual void incrementMarkedObjectsAge() = 0;
    virtual void countMarkedObjects(ClassAgeCountsMap&) = 0;
    virtual void countObjectsToSweep(ClassAgeCountsMap&) = 0;
#endif
#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
    virtual bool contains(Address) = 0;
#endif
    virtual size_t size() = 0;
    virtual bool isLargeObjectPage() { return false; }

    Address address() { return reinterpret_cast<Address>(this); }
    PageMemory* storage() const { return m_storage; }
    BaseHeap* heap() const { return m_heap; }
    bool orphaned() { return !m_heap; }
    bool terminating() { return m_terminating; }
    void setTerminating() { m_terminating = true; }

    // Returns true if this page has been swept by the ongoing lazy sweep.
    bool hasBeenSwept() const { return m_swept; }

    void markAsSwept()
    {
        ASSERT(!m_swept);
        m_swept = true;
    }

    void markAsUnswept()
    {
        ASSERT(m_swept);
        m_swept = false;
    }

private:
    PageMemory* m_storage;
    BaseHeap* m_heap;
    BasePage* m_next;
    // Whether the page is part of a terminating thread or not.
    bool m_terminating;

    // Track the sweeping state of a page. Set to true once
    // the lazy sweep completes has processed it.
    //
    // Set to false at the start of a sweep, true  upon completion
    // of lazy sweeping.
    bool m_swept;
    friend class BaseHeap;
};

class NormalPage final : public BasePage {
public:
    NormalPage(PageMemory*, BaseHeap*);

    Address payload()
    {
        return address() + pageHeaderSize();
    }
    size_t payloadSize()
    {
        return (blinkPagePayloadSize() - pageHeaderSize()) & ~allocationMask;
    }
    Address payloadEnd() { return payload() + payloadSize(); }
    bool containedInObjectPayload(Address address)
    {
        return payload() <= address && address < payloadEnd();
    }

    virtual size_t objectPayloadSizeForTesting() override;
    virtual bool isEmpty() override;
    virtual void removeFromHeap() override;
    virtual void sweep() override;
    virtual void markUnmarkedObjectsDead() override;
    virtual void checkAndMarkPointer(Visitor*, Address) override;
    virtual void markOrphaned() override;
#if ENABLE(GC_PROFILING)
    const GCInfo* findGCInfo(Address) override;
    void snapshot(TracedValue*, ThreadState::SnapshotInfo*) override;
    void incrementMarkedObjectsAge() override;
    void countMarkedObjects(ClassAgeCountsMap&) override;
    void countObjectsToSweep(ClassAgeCountsMap&) override;
#endif
#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
    // Returns true for the whole blinkPageSize page that the page is on, even
    // for the header, and the unmapped guard page at the start. That ensures
    // the result can be used to populate the negative page cache.
    virtual bool contains(Address) override;
#endif
    virtual size_t size() override { return blinkPageSize; }
    static size_t pageHeaderSize()
    {
        // Compute the amount of padding we have to add to a header to make
        // the size of the header plus the padding a multiple of 8 bytes.
        size_t paddingSize = (sizeof(NormalPage) + allocationGranularity - (sizeof(HeapObjectHeader) % allocationGranularity)) % allocationGranularity;
        return sizeof(NormalPage) + paddingSize;
    }


    NormalPageHeap* heapForNormalPage();
    void clearObjectStartBitMap();

#if defined(ADDRESS_SANITIZER)
    void poisonUnmarkedObjects();
#endif

private:
    HeapObjectHeader* findHeaderFromAddress(Address);
    void populateObjectStartBitMap();
    bool isObjectStartBitMapComputed() { return m_objectStartBitMapComputed; }

    bool m_objectStartBitMapComputed;
    uint8_t m_objectStartBitMap[reservedForObjectBitMap];
};

// Large allocations are allocated as separate objects and linked in a list.
//
// In order to use the same memory allocation routines for everything allocated
// in the heap, large objects are considered heap pages containing only one
// object.
class LargeObjectPage final : public BasePage {
public:
    LargeObjectPage(PageMemory*, BaseHeap*, size_t);

    Address payload() { return heapObjectHeader()->payload(); }
    size_t payloadSize() { return m_payloadSize; }
    Address payloadEnd() { return payload() + payloadSize(); }
    bool containedInObjectPayload(Address address)
    {
        return payload() <= address && address < payloadEnd();
    }

    virtual size_t objectPayloadSizeForTesting() override;
    virtual bool isEmpty() override;
    virtual void removeFromHeap() override;
    virtual void sweep() override;
    virtual void markUnmarkedObjectsDead() override;
    virtual void checkAndMarkPointer(Visitor*, Address) override;
    virtual void markOrphaned() override;

#if ENABLE(GC_PROFILING)
    const GCInfo* findGCInfo(Address) override;
    void snapshot(TracedValue*, ThreadState::SnapshotInfo*) override;
    void incrementMarkedObjectsAge() override;
    void countMarkedObjects(ClassAgeCountsMap&) override;
    void countObjectsToSweep(ClassAgeCountsMap&) override;
#endif
#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
    // Returns true for any address that is on one of the pages that this
    // large object uses. That ensures that we can use a negative result to
    // populate the negative page cache.
    virtual bool contains(Address) override;
#endif
    virtual size_t size()
    {
        return pageHeaderSize() +  sizeof(HeapObjectHeader) + m_payloadSize;
    }
    static size_t pageHeaderSize()
    {
        // Compute the amount of padding we have to add to a header to make
        // the size of the header plus the padding a multiple of 8 bytes.
        size_t paddingSize = (sizeof(LargeObjectPage) + allocationGranularity - (sizeof(HeapObjectHeader) % allocationGranularity)) % allocationGranularity;
        return sizeof(LargeObjectPage) + paddingSize;
    }
    virtual bool isLargeObjectPage() override { return true; }

    HeapObjectHeader* heapObjectHeader()
    {
        Address headerAddress = address() + pageHeaderSize();
        return reinterpret_cast<HeapObjectHeader*>(headerAddress);
    }

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    void setIsVectorBackingPage() { m_isVectorBackingPage = true; }
    bool isVectorBackingPage() const { return m_isVectorBackingPage; }
#endif

private:

    size_t m_payloadSize;
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    bool m_isVectorBackingPage;
#endif
};

// A HeapDoesNotContainCache provides a fast way of taking an arbitrary
// pointer-sized word, and determining whether it cannot be interpreted as a
// pointer to an area that is managed by the garbage collected Blink heap.  This
// is a cache of 'pages' that have previously been determined to be wholly
// outside of the heap.  The size of these pages must be smaller than the
// allocation alignment of the heap pages.  We determine off-heap-ness by
// rounding down the pointer to the nearest page and looking up the page in the
// cache.  If there is a miss in the cache we can determine the status of the
// pointer precisely using the heap RegionTree.
//
// The HeapDoesNotContainCache is a negative cache, so it must be flushed when
// memory is added to the heap.
class HeapDoesNotContainCache {
public:
    HeapDoesNotContainCache()
        : m_entries(adoptArrayPtr(new Address[HeapDoesNotContainCache::numberOfEntries]))
        , m_hasEntries(false)
    {
        // Start by flushing the cache in a non-empty state to initialize all the cache entries.
        for (int i = 0; i < numberOfEntries; ++i)
            m_entries[i] = nullptr;
    }

    void flush();
    bool isEmpty() { return !m_hasEntries; }

    // Perform a lookup in the cache.
    //
    // If lookup returns false, the argument address was not found in
    // the cache and it is unknown if the address is in the Blink
    // heap.
    //
    // If lookup returns true, the argument address was found in the
    // cache which means the address is not in the heap.
    PLATFORM_EXPORT bool lookup(Address);

    // Add an entry to the cache.
    PLATFORM_EXPORT void addEntry(Address);

private:
    static const int numberOfEntriesLog2 = 12;
    static const int numberOfEntries = 1 << numberOfEntriesLog2;

    static size_t hash(Address);

    WTF::OwnPtr<Address[]> m_entries;
    bool m_hasEntries;
};

template<typename DataType>
class PagePool {
protected:
    PagePool();

    class PoolEntry {
    public:
        PoolEntry(DataType* data, PoolEntry* next)
            : data(data)
            , next(next)
        { }

        DataType* data;
        PoolEntry* next;
    };

    PoolEntry* m_pool[NumberOfHeaps];
};

// Once pages have been used for one type of thread heap they will never be
// reused for another type of thread heap.  Instead of unmapping, we add the
// pages to a pool of pages to be reused later by a thread heap of the same
// type. This is done as a security feature to avoid type confusion.  The
// heaps are type segregated by having separate thread heaps for different
// types of objects.  Holding on to pages ensures that the same virtual address
// space cannot be used for objects of another type than the type contained
// in this page to begin with.
class FreePagePool : public PagePool<PageMemory> {
public:
    ~FreePagePool();
    void addFreePage(int, PageMemory*);
    PageMemory* takeFreePage(int);

private:
    Mutex m_mutex[NumberOfHeaps];
};

class OrphanedPagePool : public PagePool<BasePage> {
public:
    ~OrphanedPagePool();
    void addOrphanedPage(int, BasePage*);
    void decommitOrphanedPages();
#if ENABLE(ASSERT)
    bool contains(void*);
#endif
private:
    void clearMemory(PageMemory*);
};

class FreeList {
public:
    FreeList();

    void addToFreeList(Address, size_t);
    void clear();

    // Returns a bucket number for inserting a FreeListEntry of a given size.
    // All FreeListEntries in the given bucket, n, have size >= 2^n.
    static int bucketIndexForSize(size_t);

#if ENABLE(GC_PROFILING)
    struct PerBucketFreeListStats {
        size_t entryCount;
        size_t freeSize;

        PerBucketFreeListStats() : entryCount(0), freeSize(0) { }
    };

    void getFreeSizeStats(PerBucketFreeListStats bucketStats[], size_t& totalSize) const;
#endif

private:
    int m_biggestFreeListIndex;

    // All FreeListEntries in the nth list have size >= 2^n.
    FreeListEntry* m_freeLists[blinkPageSizeLog2];

    friend class NormalPageHeap;
};

// Each thread has a number of thread heaps (e.g., Generic heaps,
// typed heaps for Node, heaps for collection backings etc)
// and BaseHeap represents each thread heap.
//
// BaseHeap is a parent class of NormalPageHeap and LargeObjectHeap.
// NormalPageHeap represents a heap that contains NormalPages
// and LargeObjectHeap represents a heap that contains LargeObjectPages.
class PLATFORM_EXPORT BaseHeap {
public:
    BaseHeap(ThreadState*, int);
    virtual ~BaseHeap();
    void cleanupPages();

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
    BasePage* findPageFromAddress(Address);
#endif
#if ENABLE(GC_PROFILING)
    void snapshot(TracedValue*, ThreadState::SnapshotInfo*);
    virtual void snapshotFreeList(TracedValue&) { };

    void countMarkedObjects(ClassAgeCountsMap&) const;
    void countObjectsToSweep(ClassAgeCountsMap&) const;
    void incrementMarkedObjectsAge();
#endif

    virtual void clearFreeLists() { }
    void makeConsistentForSweeping();
#if ENABLE(ASSERT)
    virtual bool isConsistentForSweeping() = 0;
#endif
    size_t objectPayloadSizeForTesting();
    void prepareHeapForTermination();
    void prepareForSweep();
    Address lazySweep(size_t, size_t gcInfoIndex);
    void completeSweep();

    ThreadState* threadState() { return m_threadState; }
    int heapIndex() const { return m_index; }

protected:
    BasePage* m_firstPage;
    BasePage* m_firstUnsweptPage;

private:
    virtual Address lazySweepPages(size_t, size_t gcInfoIndex) = 0;

    ThreadState* m_threadState;

    // Index into the page pools.  This is used to ensure that the pages of the
    // same type go into the correct page pool and thus avoid type confusion.
    int m_index;
};

class PLATFORM_EXPORT NormalPageHeap final : public BaseHeap {
public:
    NormalPageHeap(ThreadState*, int);
    void addToFreeList(Address address, size_t size)
    {
        ASSERT(findPageFromAddress(address));
        ASSERT(findPageFromAddress(address + size - 1));
        m_freeList.addToFreeList(address, size);
    }
    virtual void clearFreeLists() override;
#if ENABLE(ASSERT)
    virtual bool isConsistentForSweeping() override;
    bool pagesToBeSweptContains(Address);
#endif
#if ENABLE(GC_PROFILING)
    void snapshotFreeList(TracedValue&) override;
#endif

    Address allocateObject(size_t allocationSize, size_t gcInfoIndex);

    void freePage(NormalPage*);

    bool coalesce();
    void promptlyFreeObject(HeapObjectHeader*);
    bool expandObject(HeapObjectHeader*, size_t);
    bool shrinkObject(HeapObjectHeader*, size_t);
    void decreasePromptlyFreedSize(size_t size) { m_promptlyFreedSize -= size; }

private:
    void allocatePage();
    virtual Address lazySweepPages(size_t, size_t gcInfoIndex) override;
    Address outOfLineAllocate(size_t allocationSize, size_t gcInfoIndex);
    Address currentAllocationPoint() const { return m_currentAllocationPoint; }
    size_t remainingAllocationSize() const { return m_remainingAllocationSize; }
    bool hasCurrentAllocationArea() const { return currentAllocationPoint() && remainingAllocationSize(); }
    void setAllocationPoint(Address, size_t);
    void updateRemainingAllocationSize();
    Address allocateFromFreeList(size_t, size_t gcInfoIndex);

    FreeList m_freeList;
    Address m_currentAllocationPoint;
    size_t m_remainingAllocationSize;
    size_t m_lastRemainingAllocationSize;

    // The size of promptly freed objects in the heap.
    size_t m_promptlyFreedSize;

#if ENABLE(GC_PROFILING)
    size_t m_cumulativeAllocationSize;
    size_t m_allocationCount;
    size_t m_inlineAllocationCount;
#endif
};

class LargeObjectHeap final : public BaseHeap {
public:
    LargeObjectHeap(ThreadState*, int);
    Address allocateLargeObjectPage(size_t, size_t gcInfoIndex);
    void freeLargeObjectPage(LargeObjectPage*);
#if ENABLE(ASSERT)
    virtual bool isConsistentForSweeping() override { return true; }
#endif
private:
    Address doAllocateLargeObjectPage(size_t, size_t gcInfoIndex);
    virtual Address lazySweepPages(size_t, size_t gcInfoIndex) override;
};

// Mask an address down to the enclosing oilpan heap base page.  All oilpan heap
// pages are aligned at blinkPageBase plus an OS page size.
// FIXME: Remove PLATFORM_EXPORT once we get a proper public interface to our
// typed heaps.  This is only exported to enable tests in HeapTest.cpp.
PLATFORM_EXPORT inline BasePage* pageFromObject(const void* object)
{
    Address address = reinterpret_cast<Address>(const_cast<void*>(object));
    BasePage* page = reinterpret_cast<BasePage*>(blinkPageAddress(address) + WTF::kSystemPageSize);
    ASSERT(page->contains(address));
    return page;
}

class PLATFORM_EXPORT Heap {
public:
    static void init();
    static void shutdown();
    static void doShutdown();

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
    static BasePage* findPageFromAddress(Address);
    static BasePage* findPageFromAddress(void* pointer) { return findPageFromAddress(reinterpret_cast<Address>(pointer)); }
    static bool containedInHeapOrOrphanedPage(void*);
#endif

    // Is the finalizable GC object still alive, but slated for lazy sweeping?
    // If a lazy sweep is in progress, returns true if the object was found
    // to be not reachable during the marking phase, but it has yet to be swept
    // and finalized. The predicate returns false in all other cases.
    //
    // Holding a reference to an already-dead object is not a valid state
    // to be in; willObjectBeLazilySwept() has undefined behavior if passed
    // such a reference.
    template<typename T>
    static bool willObjectBeLazilySwept(const T* objectPointer)
    {
        static_assert(IsGarbageCollectedType<T>::value, "only objects deriving from GarbageCollected can be used.");
#if ENABLE(OILPAN)
        BasePage* page = pageFromObject(objectPointer);
        if (page->hasBeenSwept())
            return false;
        ASSERT(page->heap()->threadState()->isSweepingInProgress());

        return !ObjectAliveTrait<T>::isHeapObjectAlive(s_markingVisitor, const_cast<T*>(objectPointer));
#else
        // FIXME: remove when lazy sweeping is always on
        // (cf. ThreadState::postGCProcessing()).
        return false;
#endif
    }

    // Push a trace callback on the marking stack.
    static void pushTraceCallback(void* containerObject, TraceCallback);

    // Push a trace callback on the post-marking callback stack.  These
    // callbacks are called after normal marking (including ephemeron
    // iteration).
    static void pushPostMarkingCallback(void*, TraceCallback);

    // Add a weak pointer callback to the weak callback work list.  General
    // object pointer callbacks are added to a thread local weak callback work
    // list and the callback is called on the thread that owns the object, with
    // the closure pointer as an argument.  Most of the time, the closure and
    // the containerObject can be the same thing, but the containerObject is
    // constrained to be on the heap, since the heap is used to identify the
    // correct thread.
    static void pushWeakPointerCallback(void* closure, void* containerObject, WeakPointerCallback);

    // Similar to the more general pushWeakPointerCallback, but cell
    // pointer callbacks are added to a static callback work list and the weak
    // callback is performed on the thread performing garbage collection.  This
    // is OK because cells are just cleared and no deallocation can happen.
    static void pushWeakCellPointerCallback(void** cell, WeakPointerCallback);

    // Pop the top of a marking stack and call the callback with the visitor
    // and the object.  Returns false when there is nothing more to do.
    static bool popAndInvokeTraceCallback(Visitor*);

    // Remove an item from the post-marking callback stack and call
    // the callback with the visitor and the object pointer.  Returns
    // false when there is nothing more to do.
    static bool popAndInvokePostMarkingCallback(Visitor*);

    // Remove an item from the weak callback work list and call the callback
    // with the visitor and the closure pointer.  Returns false when there is
    // nothing more to do.
    static bool popAndInvokeWeakPointerCallback(Visitor*);

    // Register an ephemeron table for fixed-point iteration.
    static void registerWeakTable(void* containerObject, EphemeronCallback, EphemeronCallback);
#if ENABLE(ASSERT)
    static bool weakTableRegistered(const void*);
#endif

    static inline size_t allocationSizeFromSize(size_t size)
    {
        // Check the size before computing the actual allocation size.  The
        // allocation size calculation can overflow for large sizes and the check
        // therefore has to happen before any calculation on the size.
        RELEASE_ASSERT(size < maxHeapObjectSize);

        // Add space for header.
        size_t allocationSize = size + sizeof(HeapObjectHeader);
        // Align size with allocation granularity.
        allocationSize = (allocationSize + allocationMask) & ~allocationMask;
        return allocationSize;
    }
    static inline size_t roundedAllocationSize(size_t size)
    {
        return allocationSizeFromSize(size) - sizeof(HeapObjectHeader);
    }
    static Address allocateOnHeapIndex(ThreadState*, size_t, int heapIndex, size_t gcInfoIndex);
    template<typename T> static Address allocate(size_t);
    template<typename T> static Address reallocate(void* previous, size_t);

    enum GCReasonForTracing {
        IdleGC,
        PreciseGC,
        ConservativeGC,
        ForcedGCForTesting,
        NumberOfGCReasonForTracing
    };
    static const char* gcReasonForTracingString(GCReasonForTracing);
    static void collectGarbage(ThreadState::StackState, ThreadState::GCType, GCReasonForTracing);
    static void collectGarbageForTerminatingThread(ThreadState*);
    static void collectAllGarbage();

    static void processMarkingStack(Visitor*);
    static void postMarkingProcessing(Visitor*);
    static void globalWeakProcessing(Visitor*);
    static void setForcePreciseGCForTesting();

    static void preGC();
    static void postGC(ThreadState::GCType);

    // Conservatively checks whether an address is a pointer in any of the
    // thread heaps.  If so marks the object pointed to as live.
    static Address checkAndMarkPointer(Visitor*, Address);

#if ENABLE(GC_PROFILING)
    // Dump the path to specified object on the next GC.  This method is to be
    // invoked from GDB.
    static void dumpPathToObjectOnNextGC(void* p);

    // Forcibly find GCInfo of the object at Address.  This is slow and should
    // only be used for debug purposes.  It involves finding the heap page and
    // scanning the heap page for an object header.
    static const GCInfo* findGCInfo(Address);

    static String createBacktraceString();
#endif

    static size_t objectPayloadSizeForTesting();

    static void flushHeapDoesNotContainCache();

    // Return true if the last GC found a pointer into a heap page
    // during conservative scanning.
    static bool lastGCWasConservative() { return s_lastGCWasConservative; }

    static FreePagePool* freePagePool() { return s_freePagePool; }
    static OrphanedPagePool* orphanedPagePool() { return s_orphanedPagePool; }

    // This look-up uses the region search tree and a negative contains cache to
    // provide an efficient mapping from arbitrary addresses to the containing
    // heap-page if one exists.
    static BasePage* lookup(Address);
    static void addPageMemoryRegion(PageMemoryRegion*);
    static void removePageMemoryRegion(PageMemoryRegion*);

    static const GCInfo* gcInfo(size_t gcInfoIndex)
    {
        ASSERT(gcInfoIndex >= 1);
        ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
        ASSERT(s_gcInfoTable);
        const GCInfo* info = s_gcInfoTable[gcInfoIndex];
        ASSERT(info);
        return info;
    }

    static void increaseAllocatedObjectSize(size_t delta) { atomicAdd(&s_allocatedObjectSize, static_cast<long>(delta)); }
    static void decreaseAllocatedObjectSize(size_t delta) { atomicSubtract(&s_allocatedObjectSize, static_cast<long>(delta)); }
    static size_t allocatedObjectSize() { return acquireLoad(&s_allocatedObjectSize); }
    static void increaseMarkedObjectSize(size_t delta) { atomicAdd(&s_markedObjectSize, static_cast<long>(delta)); }
    static size_t markedObjectSize() { return acquireLoad(&s_markedObjectSize); }
    static void increaseAllocatedSpace(size_t delta) { atomicAdd(&s_allocatedSpace, static_cast<long>(delta)); }
    static void decreaseAllocatedSpace(size_t delta) { atomicSubtract(&s_allocatedSpace, static_cast<long>(delta)); }
    static size_t allocatedSpace() { return acquireLoad(&s_allocatedSpace); }

    static double estimatedMarkingTime();

    // On object allocation, register the object's externally allocated memory.
    static void increaseExternallyAllocatedBytes(size_t);
    static size_t externallyAllocatedBytes() { return acquireLoad(&s_externallyAllocatedBytes); }

    // On object tracing, register the object's externally allocated memory (as still live.)
    static void increaseExternallyAllocatedBytesAlive(size_t delta)
    {
        ASSERT(ThreadState::current()->isInGC());
        s_externallyAllocatedBytesAlive += delta;
    }
    static size_t externallyAllocatedBytesAlive() { return s_externallyAllocatedBytesAlive; }

    static void requestUrgentGC();
    static void clearUrgentGC() { releaseStore(&s_requestedUrgentGC, 0); }
    static bool isUrgentGCRequested() { return acquireLoad(&s_requestedUrgentGC); }

private:
    // A RegionTree is a simple binary search tree of PageMemoryRegions sorted
    // by base addresses.
    class RegionTree {
    public:
        explicit RegionTree(PageMemoryRegion* region) : m_region(region), m_left(nullptr), m_right(nullptr) { }
        ~RegionTree()
        {
            delete m_left;
            delete m_right;
        }
        PageMemoryRegion* lookup(Address);
        static void add(RegionTree*, RegionTree**);
        static void remove(PageMemoryRegion*, RegionTree**);
    private:
        PageMemoryRegion* m_region;
        RegionTree* m_left;
        RegionTree* m_right;
    };

    // Reset counters that track live and allocated-since-last-GC sizes.
    static void resetHeapCounters();

    static Visitor* s_markingVisitor;
    static CallbackStack* s_markingStack;
    static CallbackStack* s_postMarkingCallbackStack;
    static CallbackStack* s_weakCallbackStack;
    static CallbackStack* s_ephemeronStack;
    static HeapDoesNotContainCache* s_heapDoesNotContainCache;
    static bool s_shutdownCalled;
    static bool s_lastGCWasConservative;
    static FreePagePool* s_freePagePool;
    static OrphanedPagePool* s_orphanedPagePool;
    static RegionTree* s_regionTree;
    static size_t s_allocatedSpace;
    static size_t s_allocatedObjectSize;
    static size_t s_markedObjectSize;
    static size_t s_externallyAllocatedBytes;
    static size_t s_externallyAllocatedBytesAlive;
    static unsigned s_requestedUrgentGC;

    friend class ThreadState;
};

template<typename T>
struct HeapIndexTrait {
    static int index() { return NormalPageHeapIndex; };
};

// FIXME: The forward declaration is layering violation.
#define DEFINE_TYPED_HEAP_TRAIT(Type)                   \
    class Type;                                         \
    template <> struct HeapIndexTrait<class Type> {     \
        static int index() { return Type##HeapIndex; }; \
    };
FOR_EACH_TYPED_HEAP(DEFINE_TYPED_HEAP_TRAIT)
#undef DEFINE_TYPED_HEAP_TRAIT

// Base class for objects allocated in the Blink garbage-collected heap.
//
// Defines a 'new' operator that allocates the memory in the heap.  'delete'
// should not be called on objects that inherit from GarbageCollected.
//
// Instances of GarbageCollected will *NOT* get finalized.  Their destructor
// will not be called.  Therefore, only classes that have trivial destructors
// with no semantic meaning (including all their subclasses) should inherit from
// GarbageCollected.  If there are non-trival destructors in a given class or
// any of its subclasses, GarbageCollectedFinalized should be used which
// guarantees that the destructor is called on an instance when the garbage
// collector determines that it is no longer reachable.
template<typename T>
class GarbageCollected {
    WTF_MAKE_NONCOPYABLE(GarbageCollected);

    // For now direct allocation of arrays on the heap is not allowed.
    void* operator new[](size_t size);

#if OS(WIN) && COMPILER(MSVC)
    // Due to some quirkiness in the MSVC compiler we have to provide
    // the delete[] operator in the GarbageCollected subclasses as it
    // is called when a class is exported in a DLL.
protected:
    void operator delete[](void* p)
    {
        ASSERT_NOT_REACHED();
    }
#else
    void operator delete[](void* p);
#endif

public:
    using GarbageCollectedBase = T;

    void* operator new(size_t size)
    {
        return Heap::allocate<T>(size);
    }

    void operator delete(void* p)
    {
        ASSERT_NOT_REACHED();
    }

protected:
    GarbageCollected()
    {
    }
};

// Base class for objects allocated in the Blink garbage-collected heap.
//
// Defines a 'new' operator that allocates the memory in the heap.  'delete'
// should not be called on objects that inherit from GarbageCollected.
//
// Instances of GarbageCollectedFinalized will have their destructor called when
// the garbage collector determines that the object is no longer reachable.
template<typename T>
class GarbageCollectedFinalized : public GarbageCollected<T> {
    WTF_MAKE_NONCOPYABLE(GarbageCollectedFinalized);

protected:
    // finalizeGarbageCollectedObject is called when the object is freed from
    // the heap.  By default finalization means calling the destructor on the
    // object.  finalizeGarbageCollectedObject can be overridden to support
    // calling the destructor of a subclass.  This is useful for objects without
    // vtables that require explicit dispatching.  The name is intentionally a
    // bit long to make name conflicts less likely.
    void finalizeGarbageCollectedObject()
    {
        static_cast<T*>(this)->~T();
    }

    GarbageCollectedFinalized() { }
    ~GarbageCollectedFinalized() { }

    template<typename U> friend struct HasFinalizer;
    template<typename U, bool> friend struct FinalizerTraitImpl;
};

// Base class for objects that are in the Blink garbage-collected heap
// and are still reference counted.
//
// This class should be used sparingly and only to gradually move
// objects from being reference counted to being managed by the blink
// garbage collector.
//
// While the current reference counting keeps one of these objects
// alive it will have a Persistent handle to itself allocated so we
// will not reclaim the memory.  When the reference count reaches 0 the
// persistent handle will be deleted.  When the garbage collector
// determines that there are no other references to the object it will
// be reclaimed and the destructor of the reclaimed object will be
// called at that time.
template<typename T>
class RefCountedGarbageCollected : public GarbageCollectedFinalized<T> {
    WTF_MAKE_NONCOPYABLE(RefCountedGarbageCollected);

public:
    RefCountedGarbageCollected()
        : m_refCount(0)
    {
    }

    // Implement method to increase reference count for use with RefPtrs.
    //
    // In contrast to the normal WTF::RefCounted, the reference count can reach
    // 0 and increase again.  This happens in the following scenario:
    //
    // (1) The reference count becomes 0, but members, persistents, or
    //     on-stack pointers keep references to the object.
    //
    // (2) The pointer is assigned to a RefPtr again and the reference
    //     count becomes 1.
    //
    // In this case, we have to resurrect m_keepAlive.
    void ref()
    {
        if (UNLIKELY(!m_refCount)) {
            ASSERT(ThreadState::current()->findPageFromAddress(reinterpret_cast<Address>(this)));
            makeKeepAlive();
        }
        ++m_refCount;
    }

    // Implement method to decrease reference count for use with RefPtrs.
    //
    // In contrast to the normal WTF::RefCounted implementation, the
    // object itself is not deleted when the reference count reaches
    // 0.  Instead, the keep-alive persistent handle is deallocated so
    // that the object can be reclaimed when the garbage collector
    // determines that there are no other references to the object.
    void deref()
    {
        ASSERT(m_refCount > 0);
        if (!--m_refCount) {
            delete m_keepAlive;
            m_keepAlive = 0;
        }
    }

    bool hasOneRef()
    {
        return m_refCount == 1;
    }

protected:
    ~RefCountedGarbageCollected() { }

private:
    void makeKeepAlive()
    {
        ASSERT(!m_keepAlive);
        m_keepAlive = new Persistent<T>(static_cast<T*>(this));
    }

    int m_refCount;
    Persistent<T>* m_keepAlive;
};

// Classes that contain heap references but aren't themselves heap allocated,
// have some extra macros available which allows their use to be restricted to
// cases where the garbage collector is able to discover their heap references.
//
// STACK_ALLOCATED(): Use if the object is only stack allocated.  Heap objects
// should be in Members but you do not need the trace method as they are on the
// stack.  (Down the line these might turn in to raw pointers, but for now
// Members indicates that we have thought about them and explicitly taken care
// of them.)
//
// DISALLOW_ALLOCATION(): Cannot be allocated with new operators but can be a
// part object.  If it has Members you need a trace method and the containing
// object needs to call that trace method.
//
// ALLOW_ONLY_INLINE_ALLOCATION(): Allows only placement new operator.  This
// disallows general allocation of this object but allows to put the object as a
// value object in collections.  If these have Members you need to have a trace
// method. That trace method will be called automatically by the Heap
// collections.
//
#define DISALLOW_ALLOCATION()                                   \
    private:                                                    \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;

#define ALLOW_ONLY_INLINE_ALLOCATION()                                              \
    public:                                                                         \
        void* operator new(size_t, NotNullTag, void* location) { return location; } \
        void* operator new(size_t, void* location) { return location; }             \
    private:                                                                        \
        void* operator new(size_t) = delete;

#define STATIC_ONLY(Type) \
    private:              \
        Type() = delete;

// These macros insert annotations that the Blink GC plugin for clang uses for
// verification.  STACK_ALLOCATED is used to declare that objects of this type
// are always stack allocated.  GC_PLUGIN_IGNORE is used to make the plugin
// ignore a particular class or field when checking for proper usage.  When
// using GC_PLUGIN_IGNORE a bug-number should be provided as an argument where
// the bug describes what needs to happen to remove the GC_PLUGIN_IGNORE again.
#if COMPILER(CLANG)
#define STACK_ALLOCATED()                                       \
    private:                                                    \
        __attribute__((annotate("blink_stack_allocated")))      \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;

#define GC_PLUGIN_IGNORE(bug)                           \
    __attribute__((annotate("blink_gc_plugin_ignore")))
#else
#define STACK_ALLOCATED() DISALLOW_ALLOCATION()
#define GC_PLUGIN_IGNORE(bug)
#endif

NO_SANITIZE_ADDRESS inline
size_t HeapObjectHeader::size() const
{
    size_t result = m_encoded & headerSizeMask;
    // Large objects should not refer to header->size().
    // The actual size of a large object is stored in
    // LargeObjectPage::m_payloadSize.
    ASSERT(result != largeObjectSizeInHeader);
    ASSERT(!pageFromObject(this)->isLargeObjectPage());
    return result;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::checkHeader() const
{
    ASSERT(pageFromObject(this)->orphaned() || m_magic == magic);
}

inline Address HeapObjectHeader::payload()
{
    return reinterpret_cast<Address>(this) + sizeof(HeapObjectHeader);
}

inline Address HeapObjectHeader::payloadEnd()
{
    return reinterpret_cast<Address>(this) + size();
}

NO_SANITIZE_ADDRESS inline
size_t HeapObjectHeader::payloadSize()
{
    size_t size = m_encoded & headerSizeMask;
    if (UNLIKELY(size == largeObjectSizeInHeader)) {
        ASSERT(pageFromObject(this)->isLargeObjectPage());
        return static_cast<LargeObjectPage*>(pageFromObject(this))->payloadSize();
    }
    ASSERT(!pageFromObject(this)->isLargeObjectPage());
    return size - sizeof(HeapObjectHeader);
}

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isMarked() const
{
    checkHeader();
    return m_encoded & headerMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::mark()
{
    checkHeader();
    ASSERT(!isMarked());
    m_encoded = m_encoded | headerMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::unmark()
{
    checkHeader();
    ASSERT(isMarked());
    m_encoded &= ~headerMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isDead() const
{
    checkHeader();
    return m_encoded & headerDeadBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::markDead()
{
    checkHeader();
    ASSERT(!isMarked());
    m_encoded |= headerDeadBitMask;
}

inline Address NormalPageHeap::allocateObject(size_t allocationSize, size_t gcInfoIndex)
{
#if ENABLE(GC_PROFILING)
    m_cumulativeAllocationSize += allocationSize;
    ++m_allocationCount;
#endif

    if (LIKELY(allocationSize <= m_remainingAllocationSize)) {
#if ENABLE(GC_PROFILING)
        ++m_inlineAllocationCount;
#endif
        Address headerAddress = m_currentAllocationPoint;
        m_currentAllocationPoint += allocationSize;
        m_remainingAllocationSize -= allocationSize;
        ASSERT(gcInfoIndex > 0);
        new (NotNull, headerAddress) HeapObjectHeader(allocationSize, gcInfoIndex);
        Address result = headerAddress + sizeof(HeapObjectHeader);
        ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));

        // Unpoison the memory used for the object (payload).
        ASAN_UNPOISON_MEMORY_REGION(result, allocationSize - sizeof(HeapObjectHeader));
        FILL_ZERO_IF_NOT_PRODUCTION(result, allocationSize - sizeof(HeapObjectHeader));
        ASSERT(findPageFromAddress(headerAddress + allocationSize - 1));
        return result;
    }
    return outOfLineAllocate(allocationSize, gcInfoIndex);
}

inline Address Heap::allocateOnHeapIndex(ThreadState* state, size_t size, int heapIndex, size_t gcInfoIndex)
{
    ASSERT(state->isAllocationAllowed());
    ASSERT(heapIndex != LargeObjectHeapIndex);
    NormalPageHeap* heap = static_cast<NormalPageHeap*>(state->heap(heapIndex));
    return heap->allocateObject(allocationSizeFromSize(size), gcInfoIndex);
}

template<typename T>
Address Heap::allocate(size_t size)
{
    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    return Heap::allocateOnHeapIndex(state, size, HeapIndexTrait<T>::index(), GCInfoTrait<T>::index());
}

template<typename T>
Address Heap::reallocate(void* previous, size_t size)
{
    if (!size) {
        // If the new size is 0 this is equivalent to either free(previous) or
        // malloc(0).  In both cases we do nothing and return nullptr.
        return nullptr;
    }
    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    Address address = Heap::allocateOnHeapIndex(state, size, HeapIndexTrait<T>::index(), GCInfoTrait<T>::index());
    if (!previous) {
        // This is equivalent to malloc(size).
        return address;
    }
    HeapObjectHeader* previousHeader = HeapObjectHeader::fromPayload(previous);
    // FIXME: We don't support reallocate() for finalizable objects.
    ASSERT(!Heap::gcInfo(previousHeader->gcInfoIndex())->hasFinalizer());
    ASSERT(previousHeader->gcInfoIndex() == GCInfoTrait<T>::index());
    size_t copySize = previousHeader->payloadSize();
    if (copySize > size)
        copySize = size;
    memcpy(address, previous, copySize);
    return address;
}

inline void Heap::increaseExternallyAllocatedBytes(size_t delta)
{
    // Flag GC urgency on a 50% increase in external allocation
    // since the last GC, but not for less than 100M.
    //
    // FIXME: consider other, 'better' policies (e.g., have the count of
    // heap objects with external allocations be taken into
    // account, ...) The overall goal here is to trigger a
    // GC such that it considerably lessens memory pressure
    // for a renderer process, when absolutely needed.
    size_t externalBytesAllocatedSinceLastGC = atomicAdd(&s_externallyAllocatedBytes, static_cast<long>(delta));
    if (LIKELY(externalBytesAllocatedSinceLastGC < 100 * 1024 * 1024))
        return;

    if (UNLIKELY(isUrgentGCRequested()))
        return;

    size_t externalBytesAliveAtLastGC = externallyAllocatedBytesAlive();
    if (UNLIKELY(externalBytesAllocatedSinceLastGC > externalBytesAliveAtLastGC / 2))
        Heap::requestUrgentGC();
}

class HeapAllocatorQuantizer {
public:
    template<typename T>
    static size_t quantizedSize(size_t count)
    {
        RELEASE_ASSERT(count <= kMaxUnquantizedAllocation / sizeof(T));
        return Heap::roundedAllocationSize(count * sizeof(T));
    }
    static const size_t kMaxUnquantizedAllocation = maxHeapObjectSize;
};

// This is a static-only class used as a trait on collections to make them heap
// allocated.  However see also HeapListHashSetAllocator.
class HeapAllocator {
public:
    using Quantizer = HeapAllocatorQuantizer;
    using Visitor = blink::Visitor;
    static const bool isGarbageCollected = true;

    template <typename T>
    static T* allocateVectorBacking(size_t size)
    {
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        ASSERT(state->isAllocationAllowed());
        size_t gcInfoIndex = GCInfoTrait<HeapVectorBacking<T, VectorTraits<T>>>::index();
        NormalPageHeap* heap = static_cast<NormalPageHeap*>(state->vectorBackingHeap(gcInfoIndex));
        return reinterpret_cast<T*>(heap->allocateObject(Heap::allocationSizeFromSize(size), gcInfoIndex));
    }
    template <typename T>
    static T* allocateExpandedVectorBacking(size_t size)
    {
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        ASSERT(state->isAllocationAllowed());
        size_t gcInfoIndex = GCInfoTrait<HeapVectorBacking<T, VectorTraits<T>>>::index();
        NormalPageHeap* heap = static_cast<NormalPageHeap*>(state->vectorBackingHeap(gcInfoIndex));
        return reinterpret_cast<T*>(heap->allocateObject(Heap::allocationSizeFromSize(size), gcInfoIndex));
    }
    PLATFORM_EXPORT static void freeVectorBacking(void*);
    PLATFORM_EXPORT static bool expandVectorBacking(void*, size_t);
    static inline bool shrinkVectorBacking(void* address, size_t quantizedCurrentSize, size_t quantizedShrunkSize)
    {
        // Returns always true, so the inlining in turn enables call site simplifications.
        backingShrink(address, quantizedCurrentSize, quantizedShrunkSize);
        return true;
    }
    template <typename T>
    static T* allocateInlineVectorBacking(size_t size)
    {
        size_t gcInfoIndex = GCInfoTrait<HeapVectorBacking<T, VectorTraits<T>>>::index();
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        return reinterpret_cast<T*>(Heap::allocateOnHeapIndex(state, size, InlineVectorHeapIndex, gcInfoIndex));
    }
    PLATFORM_EXPORT static void freeInlineVectorBacking(void*);
    PLATFORM_EXPORT static bool expandInlineVectorBacking(void*, size_t);
    static inline bool shrinkInlineVectorBacking(void* address, size_t quantizedCurrentSize, size_t quantizedShrunkSize)
    {
        backingShrink(address, quantizedCurrentSize, quantizedShrunkSize);
        return true;
    }

    template <typename T, typename HashTable>
    static T* allocateHashTableBacking(size_t size)
    {
        size_t gcInfoIndex = GCInfoTrait<HeapHashTableBacking<HashTable>>::index();
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        return reinterpret_cast<T*>(Heap::allocateOnHeapIndex(state, size, HashTableHeapIndex, gcInfoIndex));
    }
    template <typename T, typename HashTable>
    static T* allocateZeroedHashTableBacking(size_t size)
    {
        return allocateHashTableBacking<T, HashTable>(size);
    }
    PLATFORM_EXPORT static void freeHashTableBacking(void* address);
    PLATFORM_EXPORT static bool expandHashTableBacking(void*, size_t);

    template <typename Return, typename Metadata>
    static Return malloc(size_t size)
    {
        return reinterpret_cast<Return>(Heap::allocate<Metadata>(size));
    }
    static void free(void* address) { }
    template<typename T>
    static void* newArray(size_t bytes)
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    static void deleteArray(void* ptr)
    {
        ASSERT_NOT_REACHED();
    }

    static bool isAllocationAllowed()
    {
        return ThreadState::current()->isAllocationAllowed();
    }

    template<typename VisitorDispatcher>
    static void markNoTracing(VisitorDispatcher visitor, const void* t) { visitor->markNoTracing(t); }

    template<typename VisitorDispatcher, typename T, typename Traits>
    static void trace(VisitorDispatcher visitor, T& t)
    {
        CollectionBackingTraceTrait<WTF::ShouldBeTraced<Traits>::value, Traits::weakHandlingFlag, WTF::WeakPointersActWeak, T, Traits>::trace(visitor, t);
    }

    template<typename VisitorDispatcher>
    static void registerDelayedMarkNoTracing(VisitorDispatcher visitor, const void* object)
    {
        visitor->registerDelayedMarkNoTracing(object);
    }

    template<typename VisitorDispatcher>
    static void registerWeakMembers(VisitorDispatcher visitor, const void* closure, const void* object, WeakPointerCallback callback)
    {
        visitor->registerWeakMembers(closure, object, callback);
    }

    template<typename VisitorDispatcher>
    static void registerWeakTable(VisitorDispatcher visitor, const void* closure, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
    {
        visitor->registerWeakTable(closure, iterationCallback, iterationDoneCallback);
    }

#if ENABLE(ASSERT)
    template<typename VisitorDispatcher>
    static bool weakTableRegistered(VisitorDispatcher visitor, const void* closure)
    {
        return visitor->weakTableRegistered(closure);
    }
#endif

    template<typename T>
    struct ResultType {
        using Type = T*;
    };

    template<typename T>
    struct OtherType {
        using Type = T*;
    };

    template<typename T>
    static T& getOther(T* other)
    {
        return *other;
    }

    static void enterNoAllocationScope()
    {
#if ENABLE(ASSERT)
        ThreadState::current()->enterNoAllocationScope();
#endif
    }

    static void leaveNoAllocationScope()
    {
#if ENABLE(ASSERT)
        ThreadState::current()->leaveNoAllocationScope();
#endif
    }

private:
    static void backingFree(void*);
    static bool backingExpand(void*, size_t);
    PLATFORM_EXPORT static void backingShrink(void*, size_t quantizedCurrentSize, size_t quantizedShrunkSize);

    template<typename T, size_t u, typename V> friend class WTF::Vector;
    template<typename T, typename U, typename V, typename W> friend class WTF::HashSet;
    template<typename T, typename U, typename V, typename W, typename X, typename Y> friend class WTF::HashMap;
};

template<typename VisitorDispatcher, typename Value>
static void traceListHashSetValue(VisitorDispatcher visitor, Value& value)
{
    // We use the default hash traits for the value in the node, because
    // ListHashSet does not let you specify any specific ones.
    // We don't allow ListHashSet of WeakMember, so we set that one false
    // (there's an assert elsewhere), but we have to specify some value for the
    // strongify template argument, so we specify WTF::WeakPointersActWeak,
    // arbitrarily.
    CollectionBackingTraceTrait<WTF::ShouldBeTraced<WTF::HashTraits<Value>>::value, WTF::NoWeakHandlingInCollections, WTF::WeakPointersActWeak, Value, WTF::HashTraits<Value>>::trace(visitor, value);
}

// The inline capacity is just a dummy template argument to match the off-heap
// allocator.
// This inherits from the static-only HeapAllocator trait class, but we do
// declare pointers to instances.  These pointers are always null, and no
// objects are instantiated.
template<typename ValueArg, size_t inlineCapacity>
struct HeapListHashSetAllocator : public HeapAllocator {
    using TableAllocator = HeapAllocator;
    using Node = WTF::ListHashSetNode<ValueArg, HeapListHashSetAllocator>;

public:
    class AllocatorProvider {
    public:
        // For the heap allocation we don't need an actual allocator object, so
        // we just return null.
        HeapListHashSetAllocator* get() const { return 0; }

        // No allocator object is needed.
        void createAllocatorIfNeeded() { }

        // There is no allocator object in the HeapListHashSet (unlike in the
        // regular ListHashSet) so there is nothing to swap.
        void swap(AllocatorProvider& other) { }
    };

    void deallocate(void* dummy) { }

    // This is not a static method even though it could be, because it needs to
    // match the one that the (off-heap) ListHashSetAllocator has.  The 'this'
    // pointer will always be null.
    void* allocateNode()
    {
        // Consider using a LinkedHashSet instead if this compile-time assert fails:
        static_assert(!WTF::IsWeak<ValueArg>::value, "weak pointers in a ListHashSet will result in null entries in the set");

        return malloc<void*, Node>(sizeof(Node));
    }

    template<typename VisitorDispatcher>
    static void traceValue(VisitorDispatcher visitor, Node* node)
    {
        traceListHashSetValue(visitor, node->m_value);
    }
};

// FIXME: These should just be template aliases:
//
// template<typename T, size_t inlineCapacity = 0>
// using HeapVector = Vector<T, inlineCapacity, HeapAllocator>;
//
// as soon as all the compilers we care about support that.
// MSVC supports it only in MSVC 2013.
template<
    typename KeyArg,
    typename MappedArg,
    typename HashArg = typename DefaultHash<KeyArg>::Hash,
    typename KeyTraitsArg = HashTraits<KeyArg>,
    typename MappedTraitsArg = HashTraits<MappedArg>>
class HeapHashMap : public HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HeapAllocator> { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>>
class HeapHashSet : public HashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>>
class HeapLinkedHashSet : public LinkedHashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> { };

template<
    typename ValueArg,
    size_t inlineCapacity = 0, // The inlineCapacity is just a dummy to match ListHashSet (off-heap).
    typename HashArg = typename DefaultHash<ValueArg>::Hash>
class HeapListHashSet : public ListHashSet<ValueArg, inlineCapacity, HashArg, HeapListHashSetAllocator<ValueArg, inlineCapacity>> { };

template<
    typename Value,
    typename HashFunctions = typename DefaultHash<Value>::Hash,
    typename Traits = HashTraits<Value>>
class HeapHashCountedSet : public HashCountedSet<Value, HashFunctions, Traits, HeapAllocator> { };

template<typename T, size_t inlineCapacity = 0>
class HeapVector : public Vector<T, inlineCapacity, HeapAllocator> {
public:
    HeapVector() { }

    explicit HeapVector(size_t size) : Vector<T, inlineCapacity, HeapAllocator>(size)
    {
    }

    HeapVector(size_t size, const T& val) : Vector<T, inlineCapacity, HeapAllocator>(size, val)
    {
    }

    template<size_t otherCapacity>
    HeapVector(const HeapVector<T, otherCapacity>& other)
        : Vector<T, inlineCapacity, HeapAllocator>(other)
    {
    }

    template<typename U>
    void append(const U* data, size_t dataSize)
    {
        Vector<T, inlineCapacity, HeapAllocator>::append(data, dataSize);
    }

    template<typename U>
    void append(const U& other)
    {
        Vector<T, inlineCapacity, HeapAllocator>::append(other);
    }

    template<typename U, size_t otherCapacity>
    void appendVector(const HeapVector<U, otherCapacity>& other)
    {
        const Vector<U, otherCapacity, HeapAllocator>& otherVector = other;
        Vector<T, inlineCapacity, HeapAllocator>::appendVector(otherVector);
    }
};

template<typename T, size_t inlineCapacity = 0>
class HeapDeque : public Deque<T, inlineCapacity, HeapAllocator> {
public:
    HeapDeque() { }

    explicit HeapDeque(size_t size) : Deque<T, inlineCapacity, HeapAllocator>(size)
    {
    }

    HeapDeque(size_t size, const T& val) : Deque<T, inlineCapacity, HeapAllocator>(size, val)
    {
    }

    // FIXME: Doesn't work if there is an inline buffer, due to crbug.com/360572
    HeapDeque<T, 0>& operator=(const HeapDeque& other)
    {
        HeapDeque<T> copy(other);
        swap(copy);
        return *this;
    }

    // FIXME: Doesn't work if there is an inline buffer, due to crbug.com/360572
    void swap(HeapDeque& other)
    {
        Deque<T, inlineCapacity, HeapAllocator>::swap(other);
    }

    template<size_t otherCapacity>
    HeapDeque(const HeapDeque<T, otherCapacity>& other)
        : Deque<T, inlineCapacity, HeapAllocator>(other)
    {
    }

    template<typename U>
    void append(const U& other)
    {
        Deque<T, inlineCapacity, HeapAllocator>::append(other);
    }
};

template<typename T, size_t i>
inline void swap(HeapVector<T, i>& a, HeapVector<T, i>& b) { a.swap(b); }
template<typename T, size_t i>
inline void swap(HeapDeque<T, i>& a, HeapDeque<T, i>& b) { a.swap(b); }
template<typename T, typename U, typename V>
inline void swap(HeapHashSet<T, U, V>& a, HeapHashSet<T, U, V>& b) { a.swap(b); }
template<typename T, typename U, typename V, typename W, typename X>
inline void swap(HeapHashMap<T, U, V, W, X>& a, HeapHashMap<T, U, V, W, X>& b) { a.swap(b); }
template<typename T, size_t i, typename U>
inline void swap(HeapListHashSet<T, i, U>& a, HeapListHashSet<T, i, U>& b) { a.swap(b); }
template<typename T, typename U, typename V>
inline void swap(HeapLinkedHashSet<T, U, V>& a, HeapLinkedHashSet<T, U, V>& b) { a.swap(b); }
template<typename T, typename U, typename V>
inline void swap(HeapHashCountedSet<T, U, V>& a, HeapHashCountedSet<T, U, V>& b) { a.swap(b); }

template<typename T>
struct ThreadingTrait<Member<T>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T>
struct ThreadingTrait<WeakMember<T>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename Key, typename Value, typename T, typename U, typename V>
struct ThreadingTrait<HashMap<Key, Value, T, U, V, HeapAllocator>> {
    static const ThreadAffinity Affinity =
        (ThreadingTrait<Key>::Affinity == MainThreadOnly)
        && (ThreadingTrait<Value>::Affinity == MainThreadOnly) ? MainThreadOnly : AnyThread;
};

template<typename First, typename Second>
struct ThreadingTrait<WTF::KeyValuePair<First, Second>> {
    static const ThreadAffinity Affinity =
        (ThreadingTrait<First>::Affinity == MainThreadOnly)
        && (ThreadingTrait<Second>::Affinity == MainThreadOnly) ? MainThreadOnly : AnyThread;
};

template<typename T, typename U, typename V>
struct ThreadingTrait<HashSet<T, U, V, HeapAllocator>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};


template<typename T, size_t inlineCapacity>
struct ThreadingTrait<Vector<T, inlineCapacity, HeapAllocator>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T, typename Traits>
struct ThreadingTrait<HeapVectorBacking<T, Traits>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T, size_t inlineCapacity>
struct ThreadingTrait<Deque<T, inlineCapacity, HeapAllocator>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T, typename U, typename V>
struct ThreadingTrait<HashCountedSet<T, U, V, HeapAllocator>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename Table>
struct ThreadingTrait<HeapHashTableBacking<Table>> {
    using Key = typename Table::KeyType;
    using Value = typename Table::ValueType;
    static const ThreadAffinity Affinity =
        (ThreadingTrait<Key>::Affinity == MainThreadOnly)
        && (ThreadingTrait<Value>::Affinity == MainThreadOnly) ? MainThreadOnly : AnyThread;
};

template<typename T, typename U, typename V, typename W, typename X>
struct ThreadingTrait<HeapHashMap<T, U, V, W, X>> : public ThreadingTrait<HashMap<T, U, V, W, X, HeapAllocator>> { };

template<typename T, typename U, typename V>
struct ThreadingTrait<HeapHashSet<T, U, V>> : public ThreadingTrait<HashSet<T, U, V, HeapAllocator>> { };

template<typename T, size_t inlineCapacity>
struct ThreadingTrait<HeapVector<T, inlineCapacity>> : public ThreadingTrait<Vector<T, inlineCapacity, HeapAllocator>> { };

template<typename T, size_t inlineCapacity>
struct ThreadingTrait<HeapDeque<T, inlineCapacity>> : public ThreadingTrait<Deque<T, inlineCapacity, HeapAllocator>> { };

template<typename T, typename U, typename V>
struct ThreadingTrait<HeapHashCountedSet<T, U, V>> : public ThreadingTrait<HashCountedSet<T, U, V, HeapAllocator>> { };

// The standard implementation of GCInfoTrait<T>::index() just returns a static
// from the class T, but we can't do that for HashMap, HashSet, Vector, etc.
// because they are in WTF and know nothing of GCInfos. Instead we have a
// specialization of GCInfoTrait for these four classes here.

template<typename Key, typename Value, typename T, typename U, typename V>
struct GCInfoTrait<HashMap<Key, Value, T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = HashMap<Key, Value, T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // HashMap needs no finalizer.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V>
struct GCInfoTrait<HashSet<T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = HashSet<T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // HashSet needs no finalizer.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V>
struct GCInfoTrait<LinkedHashSet<T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = LinkedHashSet<T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            LinkedHashSet<T, U, V, HeapAllocator>::finalize,
            true, // Needs finalization. The anchor needs to unlink itself from the chain.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename ValueArg, size_t inlineCapacity, typename U>
struct GCInfoTrait<ListHashSet<ValueArg, inlineCapacity, U, HeapListHashSetAllocator<ValueArg, inlineCapacity>>> {
    static size_t index()
    {
        using TargetType = WTF::ListHashSet<ValueArg, inlineCapacity, U, HeapListHashSetAllocator<ValueArg, inlineCapacity>>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // ListHashSet needs no finalization though its backing might.
            false, // no vtable.
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename Allocator>
struct GCInfoTrait<WTF::ListHashSetNode<T, Allocator>> {
    static size_t index()
    {
        using TargetType = WTF::ListHashSetNode<T, Allocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            TargetType::finalize,
            WTF::HashTraits<T>::needsDestruction, // The node needs destruction if its data does.
            false, // no vtable.
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T>
struct GCInfoTrait<Vector<T, 0, HeapAllocator>> {
    static size_t index()
    {
#if ENABLE(GC_PROFILING)
        using TargetType = Vector<T, 0, HeapAllocator>;
#endif
        static const GCInfo gcInfo = {
            TraceTrait<Vector<T, 0, HeapAllocator>>::trace,
            nullptr,
            false, // Vector needs no finalizer if it has no inline capacity.
            WTF::IsPolymorphic<Vector<T, 0, HeapAllocator>>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator>> : public FinalizerTraitImpl<Vector<T, inlineCapacity, HeapAllocator>, true> { };

template<typename T, size_t inlineCapacity>
struct GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = Vector<T, inlineCapacity, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            FinalizerTrait<TargetType>::finalize,
            // Finalizer is needed to destruct things stored in the inline capacity.
            inlineCapacity && VectorTraits<T>::needsDestruction,
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T>
struct GCInfoTrait<Deque<T, 0, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = Deque<T, 0, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // Deque needs no finalizer if it has no inline capacity.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V>
struct GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = HashCountedSet<T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // HashCountedSet is just a HashTable, and needs no finalizer.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator>> : public FinalizerTraitImpl<Deque<T, inlineCapacity, HeapAllocator>, true> { };

template<typename T, size_t inlineCapacity>
struct GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = Deque<T, inlineCapacity, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            FinalizerTrait<TargetType>::finalize,
            // Finalizer is needed to destruct things stored in the inline capacity.
            inlineCapacity && VectorTraits<T>::needsDestruction,
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename Traits>
struct GCInfoTrait<HeapVectorBacking<T, Traits>> {
    static size_t index()
    {
        using TargetType = HeapVectorBacking<T, Traits>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            FinalizerTrait<TargetType>::finalize,
            Traits::needsDestruction,
            false, // We don't support embedded objects in HeapVectors with vtables.
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename Table>
struct GCInfoTrait<HeapHashTableBacking<Table>> {
    static size_t index()
    {
        using TargetType = HeapHashTableBacking<Table>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            HeapHashTableBacking<Table>::finalize,
            Table::ValueTraits::needsDestruction,
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

} // namespace blink

namespace WTF {

// Catch-all for types that have a way to trace that don't have special
// handling for weakness in collections.  This means that if this type
// contains WeakMember fields, they will simply be zeroed, but the entry
// will not be removed from the collection.  This always happens for
// things in vectors, which don't currently support special handling of
// weak elements.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, T, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, T& t)
    {
        blink::TraceTrait<T>::trace(visitor, &t);
        return false;
    }
};

template<ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, blink::Member<T>, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, blink::Member<T>& t)
    {
        blink::TraceTrait<T>::mark(visitor, const_cast<typename RemoveConst<T>::Type*>(t.get()));
        return false;
    }
};

// Catch-all for things that have HashTrait support for tracing with weakness.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct TraceInCollectionTrait<WeakHandlingInCollections, strongify, T, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, T& t)
    {
        return Traits::traceInCollection(visitor, t, strongify);
    }
};

// Vector backing that needs marking. We don't support weak members in vectors.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, blink::HeapVectorBacking<T, Traits>, void> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, void* self)
    {
        // The allocator can oversize the allocation a little, according to
        // the allocation granularity.  The extra size is included in the
        // payloadSize call below, since there is nowhere to store the
        // originally allocated memory.  This assert ensures that visiting the
        // last bit of memory can't cause trouble.
        static_assert(!ShouldBeTraced<Traits>::value || sizeof(T) > blink::allocationGranularity || Traits::canInitializeWithMemset, "heap overallocation can cause spurious visits");

        T* array = reinterpret_cast<T*>(self);
        blink::HeapObjectHeader* header = blink::HeapObjectHeader::fromPayload(self);
        // Use the payload size as recorded by the heap to determine how many
        // elements to mark.
        size_t length = header->payloadSize() / sizeof(T);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
        // Have no option but to mark the whole container as accessible, but
        // this trace() is only used for backing stores that are identified
        // as roots independent from a vector.
        ANNOTATE_CHANGE_SIZE(array, length, 0, length);
#endif
        for (size_t i = 0; i < length; ++i)
            blink::CollectionBackingTraceTrait<ShouldBeTraced<Traits>::value, Traits::weakHandlingFlag, WeakPointersActStrong, T, Traits>::trace(visitor, array[i]);
        return false;
    }
};

// Almost all hash table backings are visited with this specialization.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename Table>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, blink::HeapHashTableBacking<Table>, void> {
    using Value = typename Table::ValueType;
    using Traits = typename Table::ValueTraits;

    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, void* self)
    {
        Value* array = reinterpret_cast<Value*>(self);
        blink::HeapObjectHeader* header = blink::HeapObjectHeader::fromPayload(self);
        size_t length = header->payloadSize() / sizeof(Value);
        for (size_t i = 0; i < length; ++i) {
            if (!HashTableHelper<Value, typename Table::ExtractorType, typename Table::KeyTraitsType>::isEmptyOrDeletedBucket(array[i]))
                blink::CollectionBackingTraceTrait<ShouldBeTraced<Traits>::value, Traits::weakHandlingFlag, strongify, Value, Traits>::trace(visitor, array[i]);
        }
        return false;
    }
};

// This specialization of TraceInCollectionTrait is for the backing of
// HeapListHashSet.  This is for the case that we find a reference to the
// backing from the stack.  That probably means we have a GC while we are in a
// ListHashSet method since normal API use does not put pointers to the backing
// on the stack.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename NodeContents, size_t inlineCapacity, typename T, typename U, typename V, typename W, typename X, typename Y>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, blink::HeapHashTableBacking<HashTable<ListHashSetNode<NodeContents, blink::HeapListHashSetAllocator<T, inlineCapacity>>*, U, V, W, X, Y, blink::HeapAllocator>>, void> {
    using Node = ListHashSetNode<NodeContents, blink::HeapListHashSetAllocator<T, inlineCapacity>>;
    using Table = HashTable<Node*, U, V, W, X, Y, blink::HeapAllocator>;

    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, void* self)
    {
        Node** array = reinterpret_cast<Node**>(self);
        blink::HeapObjectHeader* header = blink::HeapObjectHeader::fromPayload(self);
        size_t length = header->payloadSize() / sizeof(Node*);
        for (size_t i = 0; i < length; ++i) {
            if (!HashTableHelper<Node*, typename Table::ExtractorType, typename Table::KeyTraitsType>::isEmptyOrDeletedBucket(array[i])) {
                traceListHashSetValue(visitor, array[i]->m_value);
                // Just mark the node without tracing because we already traced
                // the contents, and there is no need to trace the next and
                // prev fields since iterating over the hash table backing will
                // find the whole chain.
                visitor->markNoTracing(array[i]);
            }
        }
        return false;
    }
};

// Key value pairs, as used in HashMap.  To disambiguate template choice we have
// to have two versions, first the one with no special weak handling, then the
// one with weak handling.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, KeyValuePair<Key, Value>, Traits>  {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, KeyValuePair<Key, Value>& self)
    {
        ASSERT(ShouldBeTraced<Traits>::value);
        blink::CollectionBackingTraceTrait<ShouldBeTraced<typename Traits::KeyTraits>::value, NoWeakHandlingInCollections, strongify, Key, typename Traits::KeyTraits>::trace(visitor, self.key);
        blink::CollectionBackingTraceTrait<ShouldBeTraced<typename Traits::ValueTraits>::value, NoWeakHandlingInCollections, strongify, Value, typename Traits::ValueTraits>::trace(visitor, self.value);
        return false;
    }
};

template<ShouldWeakPointersBeMarkedStrongly strongify, typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<WeakHandlingInCollections, strongify, KeyValuePair<Key, Value>, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, KeyValuePair<Key, Value>& self)
    {
        // This is the core of the ephemeron-like functionality.  If there is
        // weakness on the key side then we first check whether there are
        // dead weak pointers on that side, and if there are we don't mark the
        // value side (yet).  Conversely if there is weakness on the value side
        // we check that first and don't mark the key side yet if we find dead
        // weak pointers.
        // Corner case: If there is weakness on both the key and value side,
        // and there are also strong pointers on the both sides then we could
        // unexpectedly leak.  The scenario is that the weak pointer on the key
        // side is alive, which causes the strong pointer on the key side to be
        // marked.  If that then results in the object pointed to by the weak
        // pointer on the value side being marked live, then the whole
        // key-value entry is leaked.  To avoid unexpected leaking, we disallow
        // this case, but if you run into this assert, please reach out to Blink
        // reviewers, and we may relax it.
        const bool keyIsWeak = Traits::KeyTraits::weakHandlingFlag == WeakHandlingInCollections;
        const bool valueIsWeak = Traits::ValueTraits::weakHandlingFlag == WeakHandlingInCollections;
        const bool keyHasStrongRefs = ShouldBeTraced<typename Traits::KeyTraits>::value;
        const bool valueHasStrongRefs = ShouldBeTraced<typename Traits::ValueTraits>::value;
        static_assert(!keyIsWeak || !valueIsWeak || !keyHasStrongRefs || !valueHasStrongRefs, "this configuration is disallowed to avoid unexpected leaks");
        if ((valueIsWeak && !keyIsWeak) || (valueIsWeak && keyIsWeak && !valueHasStrongRefs)) {
            // Check value first.
            bool deadWeakObjectsFoundOnValueSide = blink::CollectionBackingTraceTrait<ShouldBeTraced<typename Traits::ValueTraits>::value, Traits::ValueTraits::weakHandlingFlag, strongify, Value, typename Traits::ValueTraits>::trace(visitor, self.value);
            if (deadWeakObjectsFoundOnValueSide)
                return true;
            return blink::CollectionBackingTraceTrait<ShouldBeTraced<typename Traits::KeyTraits>::value, Traits::KeyTraits::weakHandlingFlag, strongify, Key, typename Traits::KeyTraits>::trace(visitor, self.key);
        }
        // Check key first.
        bool deadWeakObjectsFoundOnKeySide = blink::CollectionBackingTraceTrait<ShouldBeTraced<typename Traits::KeyTraits>::value, Traits::KeyTraits::weakHandlingFlag, strongify, Key, typename Traits::KeyTraits>::trace(visitor, self.key);
        if (deadWeakObjectsFoundOnKeySide)
            return true;
        return blink::CollectionBackingTraceTrait<ShouldBeTraced<typename Traits::ValueTraits>::value, Traits::ValueTraits::weakHandlingFlag, strongify, Value, typename Traits::ValueTraits>::trace(visitor, self.value);
    }
};

// Nodes used by LinkedHashSet.  Again we need two versions to disambiguate the
// template.
template<ShouldWeakPointersBeMarkedStrongly strongify, typename Value, typename Allocator, typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, LinkedHashSetNode<Value, Allocator>, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, LinkedHashSetNode<Value, Allocator>& self)
    {
        ASSERT(ShouldBeTraced<Traits>::value);
        return TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, Value, typename Traits::ValueTraits>::trace(visitor, self.m_value);
    }
};

template<ShouldWeakPointersBeMarkedStrongly strongify, typename Value, typename Allocator, typename Traits>
struct TraceInCollectionTrait<WeakHandlingInCollections, strongify, LinkedHashSetNode<Value, Allocator>, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, LinkedHashSetNode<Value, Allocator>& self)
    {
        return TraceInCollectionTrait<WeakHandlingInCollections, strongify, Value, typename Traits::ValueTraits>::trace(visitor, self.m_value);
    }
};

// ListHashSetNode pointers (a ListHashSet is implemented as a hash table of
// these pointers).
template<ShouldWeakPointersBeMarkedStrongly strongify, typename Value, size_t inlineCapacity, typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections, strongify, ListHashSetNode<Value, blink::HeapListHashSetAllocator<Value, inlineCapacity>>*, Traits> {
    using Node = ListHashSetNode<Value, blink::HeapListHashSetAllocator<Value, inlineCapacity>>;

    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, Node* node)
    {
        traceListHashSetValue(visitor, node->m_value);
        // Just mark the node without tracing because we already traced the
        // contents, and there is no need to trace the next and prev fields
        // since iterating over the hash table backing will find the whole
        // chain.
        visitor->markNoTracing(node);
        return false;
    }
};

} // namespace WTF

namespace blink {

// CollectionBackingTraceTrait.  Do nothing for things in collections that don't
// need tracing, or call TraceInCollectionTrait for those that do.

// Specialization for things that don't need marking and have no weak pointers.
// We do nothing, even if WTF::WeakPointersActStrong.
template<WTF::ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct CollectionBackingTraceTrait<false, WTF::NoWeakHandlingInCollections, strongify, T, Traits> {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher, T&) { return false; }
};

template<typename T>
static void verifyGarbageCollectedIfMember(T*)
{
}

template<typename T>
static void verifyGarbageCollectedIfMember(Member<T>* t)
{
    STATIC_ASSERT_IS_GARBAGE_COLLECTED(T, "non garbage collected object in member");
}

// Specialization for things that either need marking or have weak pointers or
// both.
template<bool needsTracing, WTF::WeakHandlingFlag weakHandlingFlag, WTF::ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct CollectionBackingTraceTrait {
    template<typename VisitorDispatcher>
    static bool trace(VisitorDispatcher visitor, T&t)
    {
        verifyGarbageCollectedIfMember(reinterpret_cast<T*>(0));
        return WTF::TraceInCollectionTrait<weakHandlingFlag, strongify, T, Traits>::trace(visitor, t);
    }
};

template<typename T> struct WeakHandlingHashTraits : WTF::SimpleClassHashTraits<T> {
    // We want to treat the object as a weak object in the sense that it can
    // disappear from hash sets and hash maps.
    static const WTF::WeakHandlingFlag weakHandlingFlag = WTF::WeakHandlingInCollections;
    // Normally whether or not an object needs tracing is inferred
    // automatically from the presence of the trace method, but we don't
    // necessarily have a trace method, and we may not need one because T
    // can perhaps only be allocated inside collections, never as independent
    // objects.  Explicitly mark this as needing tracing and it will be traced
    // in collections using the traceInCollection method, which it must have.
    template<typename U = void> struct NeedsTracingLazily {
        static const bool value = true;
    };
    // The traceInCollection method traces differently depending on whether we
    // are strongifying the trace operation.  We strongify the trace operation
    // when there are active iterators on the object.  In this case all
    // WeakMembers are marked like strong members so that elements do not
    // suddenly disappear during iteration.  Returns true if weak pointers to
    // dead objects were found: In this case any strong pointers were not yet
    // traced and the entry should be removed from the collection.
    template<typename VisitorDispatcher>
    static bool traceInCollection(VisitorDispatcher visitor, T& t, WTF::ShouldWeakPointersBeMarkedStrongly strongify)
    {
        return t.traceInCollection(visitor, strongify);
    }
};

template<typename T, typename Traits>
struct TraceTrait<HeapVectorBacking<T, Traits>> {
    using Backing = HeapVectorBacking<T, Traits>;

    template<typename VisitorDispatcher>
    static void trace(VisitorDispatcher visitor, void* self)
    {
        static_assert(!WTF::IsWeak<T>::value, "weakness in HeapVectors and Deques are not supported");
        if (WTF::ShouldBeTraced<Traits>::value)
            WTF::TraceInCollectionTrait<WTF::NoWeakHandlingInCollections, WTF::WeakPointersActWeak, HeapVectorBacking<T, Traits>, void>::trace(visitor, self);
    }

    template<typename VisitorDispatcher>
    static void mark(VisitorDispatcher visitor, const Backing* backing)
    {
        visitor->mark(backing, &trace);
    }
    static void checkGCInfo(Visitor* visitor, const Backing* backing)
    {
#if ENABLE(ASSERT)
        assertObjectHasGCInfo(const_cast<Backing*>(backing), GCInfoTrait<Backing>::index());
#endif
    }
};

// The trace trait for the heap hashtable backing is used when we find a
// direct pointer to the backing from the conservative stack scanner.  This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries.  When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template<typename Table>
struct TraceTrait<HeapHashTableBacking<Table>> {
    using Backing = HeapHashTableBacking<Table>;
    using Traits = typename Table::ValueTraits;

    template<typename VisitorDispatcher>
    static void trace(VisitorDispatcher visitor, void* self)
    {
        if (WTF::ShouldBeTraced<Traits>::value || Traits::weakHandlingFlag == WTF::WeakHandlingInCollections)
            WTF::TraceInCollectionTrait<WTF::NoWeakHandlingInCollections, WTF::WeakPointersActStrong, Backing, void>::trace(visitor, self);
    }

    template<typename VisitorDispatcher>
    static void mark(VisitorDispatcher visitor, const Backing* backing)
    {
        if (WTF::ShouldBeTraced<Traits>::value || Traits::weakHandlingFlag == WTF::WeakHandlingInCollections)
            visitor->mark(backing, &trace);
        else
            visitor->markNoTracing(backing); // If we know the trace function will do nothing there is no need to call it.
    }
    static void checkGCInfo(Visitor* visitor, const Backing* backing)
    {
#if ENABLE(ASSERT)
        assertObjectHasGCInfo(const_cast<Backing*>(backing), GCInfoTrait<Backing>::index());
#endif
    }
};

template<typename Table>
void HeapHashTableBacking<Table>::finalize(void* pointer)
{
    using Value = typename Table::ValueType;
    ASSERT(Table::ValueTraits::needsDestruction);
    HeapObjectHeader* header = HeapObjectHeader::fromPayload(pointer);
    // Use the payload size as recorded by the heap to determine how many
    // elements to finalize.
    size_t length = header->payloadSize() / sizeof(Value);
    Value* table = reinterpret_cast<Value*>(pointer);
    for (unsigned i = 0; i < length; ++i) {
        if (!Table::isEmptyOrDeletedBucket(table[i]))
            table[i].~Value();
    }
}

template<typename T, typename U, typename V, typename W, typename X>
struct GCInfoTrait<HeapHashMap<T, U, V, W, X>> : public GCInfoTrait<HashMap<T, U, V, W, X, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashSet<T, U, V>> : public GCInfoTrait<HashSet<T, U, V, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapLinkedHashSet<T, U, V>> : public GCInfoTrait<LinkedHashSet<T, U, V, HeapAllocator>> { };
template<typename T, size_t inlineCapacity, typename U>
struct GCInfoTrait<HeapListHashSet<T, inlineCapacity, U>> : public GCInfoTrait<ListHashSet<T, inlineCapacity, U, HeapListHashSetAllocator<T, inlineCapacity>>> { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapVector<T, inlineCapacity>> : public GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapDeque<T, inlineCapacity>> : public GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashCountedSet<T, U, V>> : public GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> { };

} // namespace blink

#endif // Heap_h

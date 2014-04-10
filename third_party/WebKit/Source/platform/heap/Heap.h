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

#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"

#include <stdint.h>

namespace WebCore {

const size_t blinkPageSizeLog2 = 17;
const size_t blinkPageSize = 1 << blinkPageSizeLog2;
const size_t blinkPageOffsetMask = blinkPageSize - 1;
const size_t blinkPageBaseMask = ~blinkPageOffsetMask;
// Double precision floats are more efficient when 8 byte aligned, so we 8 byte
// align all allocations even on 32 bit.
const size_t allocationGranularity = 8;
const size_t allocationMask = allocationGranularity - 1;
const size_t objectStartBitMapSize = (blinkPageSize + ((8 * allocationGranularity) - 1)) / (8 * allocationGranularity);
const size_t reservedForObjectBitMap = ((objectStartBitMapSize + allocationMask) & ~allocationMask);
const size_t maxHeapObjectSize = 1 << 27;

const size_t markBitMask = 1;
const size_t freeListMask = 2;
const size_t debugBitMask = 4;
const size_t sizeMask = ~7;
const uint8_t freelistZapValue = 42;
const uint8_t finalizedZapValue = 24;

class HeapStats;
class PageMemory;
template<ThreadAffinity affinity> class ThreadLocalPersistents;
template<typename T, typename RootsAccessor = ThreadLocalPersistents<ThreadingTrait<T>::Affinity > > class Persistent;

PLATFORM_EXPORT size_t osPageSize();

// Blink heap pages are set up with a guard page before and after the
// payload.
inline size_t blinkPagePayloadSize()
{
    return blinkPageSize - 2 * osPageSize();
}

// Blink heap pages are aligned to the Blink heap page size.
// Therefore, the start of a Blink page can be obtained by
// rounding down to the Blink page size.
inline Address roundToBlinkPageStart(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) & blinkPageBaseMask);
}

// Compute the amount of padding we have to add to a header to make
// the size of the header plus the padding a multiple of 8 bytes.
template<typename Header>
inline size_t headerPadding()
{
    return (allocationGranularity - (sizeof(Header) % allocationGranularity)) % allocationGranularity;
}

// Masks an address down to the enclosing blink page base address.
inline Address blinkPageAddress(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) & blinkPageBaseMask);
}

#ifndef NDEBUG

// Sanity check for a page header address: the address of the page
// header should be OS page size away from being Blink page size
// aligned.
inline bool isPageHeaderAddress(Address address)
{
    return !((reinterpret_cast<uintptr_t>(address) & blinkPageOffsetMask) - osPageSize());
}
#endif

// Mask an address down to the enclosing oilpan heap page base address.
// All oilpan heap pages are aligned at blinkPageBase plus an OS page size.
// FIXME: Remove PLATFORM_EXPORT once we get a proper public interface to our typed heaps.
// This is only exported to enable tests in HeapTest.cpp.
PLATFORM_EXPORT inline Address pageHeaderAddress(Address address)
{
    return blinkPageAddress(address) + osPageSize();
}

// Common header for heap pages.
class BaseHeapPage {
public:
    BaseHeapPage(PageMemory* storage, const GCInfo* gcInfo, ThreadState* state)
        : m_storage(storage)
        , m_gcInfo(gcInfo)
        , m_threadState(state)
        , m_padding(0)
    {
        ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
    }

    // Check if the given address could point to an object in this
    // heap page. If so, find the start of that object and mark it
    // using the given Visitor.
    //
    // Returns true if the object was found and marked, returns false
    // otherwise.
    //
    // This is used during conservative stack scanning to
    // conservatively mark all objects that could be referenced from
    // the stack.
    virtual bool checkAndMarkPointer(Visitor*, Address) = 0;

    Address address() { return reinterpret_cast<Address>(this); }
    PageMemory* storage() const { return m_storage; }
    ThreadState* threadState() const { return m_threadState; }
    const GCInfo* gcInfo() { return m_gcInfo; }

private:
    // Accessor to silence unused warnings.
    void* padding() const { return m_padding; }

    PageMemory* m_storage;
    const GCInfo* m_gcInfo;
    ThreadState* m_threadState;
    // Free word only needed to ensure proper alignment of the
    // HeapPage header.
    void* m_padding;
};

// Large allocations are allocated as separate objects and linked in a
// list.
//
// In order to use the same memory allocation routines for everything
// allocated in the heap, large objects are considered heap pages
// containing only one object.
//
// The layout of a large heap object is as follows:
//
// | BaseHeapPage | next pointer | FinalizedHeapObjectHeader or HeapObjectHeader | payload |
template<typename Header>
class LargeHeapObject : public BaseHeapPage {
public:
    LargeHeapObject(PageMemory* storage, const GCInfo* gcInfo, ThreadState* state) : BaseHeapPage(storage, gcInfo, state)
    {
        COMPILE_ASSERT(!(sizeof(LargeHeapObject<Header>) & allocationMask), large_heap_object_header_misaligned);
    }

    virtual bool checkAndMarkPointer(Visitor*, Address);

    void link(LargeHeapObject<Header>** previousNext)
    {
        m_next = *previousNext;
        *previousNext = this;
    }

    void unlink(LargeHeapObject<Header>** previousNext)
    {
        *previousNext = m_next;
    }

    bool contains(Address object)
    {
        return (address() <= object) && (object <= (address() + size()));
    }

    LargeHeapObject<Header>* next()
    {
        return m_next;
    }

    size_t size()
    {
        return heapObjectHeader()->size() + sizeof(LargeHeapObject<Header>) + headerPadding<Header>();
    }

    Address payload() { return heapObjectHeader()->payload(); }
    size_t payloadSize() { return heapObjectHeader()->payloadSize(); }

    Header* heapObjectHeader()
    {
        Address headerAddress = address() + sizeof(LargeHeapObject<Header>) + headerPadding<Header>();
        return reinterpret_cast<Header*>(headerAddress);
    }

    bool isMarked();
    void unmark();
    void getStats(HeapStats&);
    void mark(Visitor*);
    void finalize();

private:
    friend class Heap;
    friend class ThreadHeap<Header>;

    LargeHeapObject<Header>* m_next;
};

// The BasicObjectHeader is the minimal object header. It is used when
// encountering heap space of size allocationGranularity to mark it as
// as freelist entry.
class PLATFORM_EXPORT BasicObjectHeader {
public:
    NO_SANITIZE_ADDRESS
    explicit BasicObjectHeader(size_t encodedSize)
        : m_size(encodedSize) { }

    static size_t freeListEncodedSize(size_t size) { return size | freeListMask; }

    NO_SANITIZE_ADDRESS
    bool isFree() { return m_size & freeListMask; }

    NO_SANITIZE_ADDRESS
    size_t size() const { return m_size & sizeMask; }

protected:
    size_t m_size;
};

// Our heap object layout is layered with the HeapObjectHeader closest
// to the payload, this can be wrapped in a FinalizedObjectHeader if the
// object is on the GeneralHeap and not on a specific TypedHeap.
// Finally if the object is a large object (> blinkPageSize/2) then it is
// wrapped with a LargeObjectHeader.
//
// Object memory layout:
// [ LargeObjectHeader | ] [ FinalizedObjectHeader | ] HeapObjectHeader | payload
// The [ ] notation denotes that the LargeObjectHeader and the FinalizedObjectHeader
// are independently optional.
class PLATFORM_EXPORT HeapObjectHeader : public BasicObjectHeader {
public:
    NO_SANITIZE_ADDRESS
    explicit HeapObjectHeader(size_t encodedSize)
        : BasicObjectHeader(encodedSize)
#ifndef NDEBUG
        , m_magic(magic)
#endif
    { }

    NO_SANITIZE_ADDRESS
    HeapObjectHeader(size_t encodedSize, const GCInfo*)
        : BasicObjectHeader(encodedSize)
#ifndef NDEBUG
        , m_magic(magic)
#endif
    { }

    inline void checkHeader() const;
    inline bool isMarked() const;

    inline void mark();
    inline void unmark();

    inline Address payload();
    inline size_t payloadSize();
    inline Address payloadEnd();

    inline void setDebugMark();
    inline void clearDebugMark();
    inline bool hasDebugMark() const;

    // Zap magic number with a new magic number that means there was once an
    // object allocated here, but it was freed because nobody marked it during
    // GC.
    void zapMagic();

    static void finalize(const GCInfo*, Address, size_t);
    static HeapObjectHeader* fromPayload(const void*);

    static const intptr_t magic = 0xc0de247;
    static const intptr_t zappedMagic = 0xC0DEdead;
    // The zap value for vtables should be < 4K to ensure it cannot be
    // used for dispatch.
    static const intptr_t zappedVTable = 0xd0d;

private:
#ifndef NDEBUG
    intptr_t m_magic;
#endif
};

const size_t objectHeaderSize = sizeof(HeapObjectHeader);

// Each object on the GeneralHeap needs to carry a pointer to its
// own GCInfo structure for tracing and potential finalization.
class PLATFORM_EXPORT FinalizedHeapObjectHeader : public HeapObjectHeader {
public:
    NO_SANITIZE_ADDRESS
    FinalizedHeapObjectHeader(size_t encodedSize, const GCInfo* gcInfo)
        : HeapObjectHeader(encodedSize)
        , m_gcInfo(gcInfo)
    {
    }

    inline Address payload();
    inline size_t payloadSize();

    NO_SANITIZE_ADDRESS
    const GCInfo* gcInfo() { return m_gcInfo; }

    NO_SANITIZE_ADDRESS
    TraceCallback traceCallback() { return m_gcInfo->m_trace; }

    void finalize();

    NO_SANITIZE_ADDRESS
    inline bool hasFinalizer() { return m_gcInfo->hasFinalizer(); }

    static FinalizedHeapObjectHeader* fromPayload(const void*);

    NO_SANITIZE_ADDRESS
    bool hasVTable() { return m_gcInfo->hasVTable(); }

private:
    const GCInfo* m_gcInfo;
};

const size_t finalizedHeaderSize = sizeof(FinalizedHeapObjectHeader);

class FreeListEntry : public HeapObjectHeader {
public:
    NO_SANITIZE_ADDRESS
    explicit FreeListEntry(size_t size)
        : HeapObjectHeader(freeListEncodedSize(size))
        , m_next(0)
    {
#if !defined(NDEBUG) && !ASAN
        // Zap free area with asterisks, aka 0x2a2a2a2a.
        // For ASAN don't zap since we keep accounting in the freelist entry.
        for (size_t i = sizeof(*this); i < size; i++)
            reinterpret_cast<Address>(this)[i] = freelistZapValue;
        ASSERT(size >= objectHeaderSize);
        zapMagic();
#endif
    }

    Address address() { return reinterpret_cast<Address>(this); }

    NO_SANITIZE_ADDRESS
    void unlink(FreeListEntry** prevNext)
    {
        *prevNext = m_next;
        m_next = 0;
    }

    NO_SANITIZE_ADDRESS
    void link(FreeListEntry** prevNext)
    {
        m_next = *prevNext;
        *prevNext = this;
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

// Representation of Blink heap pages.
//
// Pages are specialized on the type of header on the object they
// contain. If a heap page only contains a certain type of object all
// of the objects will have the same GCInfo pointer and therefore that
// pointer can be stored in the HeapPage instead of in the header of
// each object. In that case objects have only a HeapObjectHeader and
// not a FinalizedHeapObjectHeader saving a word per object.
template<typename Header>
class HeapPage : public BaseHeapPage {
public:
    HeapPage(PageMemory*, ThreadHeap<Header>*, const GCInfo*);

    void link(HeapPage**);
    static void unlink(HeapPage*, HeapPage**);

    bool isEmpty();

    bool contains(Address addr)
    {
        Address blinkPageStart = roundToBlinkPageStart(address());
        return blinkPageStart <= addr && (blinkPageStart + blinkPageSize) > addr;
    }

    HeapPage* next() { return m_next; }

    Address payload()
    {
        return address() + sizeof(*this) + headerPadding<Header>();
    }

    static size_t payloadSize()
    {
        return (blinkPagePayloadSize() - sizeof(HeapPage) - headerPadding<Header>()) & ~allocationMask;
    }

    Address end() { return payload() + payloadSize(); }

    void getStats(HeapStats&);
    void clearMarks();
    void sweep();
    void clearObjectStartBitMap();
    void finalize(Header*);
    virtual bool checkAndMarkPointer(Visitor*, Address);
    ThreadHeap<Header>* heap() { return m_heap; }
#if defined(ADDRESS_SANITIZER)
    void poisonUnmarkedObjects();
#endif

protected:
    void populateObjectStartBitMap();
    bool isObjectStartBitMapComputed() { return m_objectStartBitMapComputed; }
    TraceCallback traceCallback(Header*);
    bool hasVTable(Header*);

    HeapPage<Header>* m_next;
    ThreadHeap<Header>* m_heap;
    bool m_objectStartBitMapComputed;
    uint8_t m_objectStartBitMap[reservedForObjectBitMap];

    friend class ThreadHeap<Header>;
};

// A HeapContainsCache provides a fast way of taking an arbitrary
// pointer-sized word, and determining whether it can be interpreted
// as a pointer to an area that is managed by the garbage collected
// Blink heap. There is a cache of 'pages' that have previously been
// determined to be either wholly inside or wholly outside the
// heap. The size of these pages must be smaller than the allocation
// alignment of the heap pages. We determine on-heap-ness by rounding
// down the pointer to the nearest page and looking up the page in the
// cache. If there is a miss in the cache we ask the heap to determine
// the status of the pointer by iterating over all of the heap. The
// result is then cached in the two-way associative page cache.
//
// A HeapContainsCache is both a positive and negative
// cache. Therefore, it must be flushed both when new memory is added
// and when memory is removed from the Blink heap.
class HeapContainsCache {
public:
    HeapContainsCache();

    void flush();
    bool contains(Address);

    // Perform a lookup in the cache.
    //
    // If lookup returns false the argument address was not found in
    // the cache and it is unknown if the address is in the Blink
    // heap.
    //
    // If lookup returns true the argument address was found in the
    // cache. In that case, the address is in the heap if the base
    // heap page out parameter is different from 0 and is not in the
    // heap if the base heap page out parameter is 0.
    bool lookup(Address, BaseHeapPage**);

    // Add an entry to the cache. Use a 0 base heap page pointer to
    // add a negative entry.
    void addEntry(Address, BaseHeapPage*);

private:
    class Entry {
    public:
        Entry()
            : m_address(0)
            , m_containingPage(0)
        {
        }

        Entry(Address address, BaseHeapPage* containingPage)
            : m_address(address)
            , m_containingPage(containingPage)
        {
        }

        BaseHeapPage* containingPage() { return m_containingPage; }
        Address address() { return m_address; }

    private:
        Address m_address;
        BaseHeapPage* m_containingPage;
    };

    static const int numberOfEntriesLog2 = 12;
    static const int numberOfEntries = 1 << numberOfEntriesLog2;

    static size_t hash(Address);

    WTF::OwnPtr<HeapContainsCache::Entry[]> m_entries;

    friend class ThreadState;
};

// The CallbackStack contains all the visitor callbacks used to trace and mark
// objects. A specific CallbackStack instance contains at most bufferSize elements.
// If more space is needed a new CallbackStack instance is created and chained
// together with the former instance. I.e. a logical CallbackStack can be made of
// multiple chained CallbackStack object instances.
// There are two logical callback stacks. One containing all the marking callbacks and
// one containing the weak pointer callbacks.
class CallbackStack {
public:
    CallbackStack(CallbackStack** first)
        : m_limit(&(m_buffer[bufferSize]))
        , m_current(&(m_buffer[0]))
        , m_next(*first)
    {
#ifndef NDEBUG
        clearUnused();
#endif
        *first = this;
    }

    ~CallbackStack();
    void clearUnused();

    void assertIsEmpty();

    class Item {
    public:
        Item() { }
        Item(void* object, VisitorCallback callback)
            : m_object(object)
            , m_callback(callback)
        {
        }
        void* object() { return m_object; }
        VisitorCallback callback() { return m_callback; }

    private:
        void* m_object;
        VisitorCallback m_callback;
    };

    static void init(CallbackStack** first);
    static void shutdown(CallbackStack** first);
    bool popAndInvokeCallback(CallbackStack** first, Visitor*);

    Item* allocateEntry(CallbackStack** first)
    {
        if (m_current < m_limit)
            return m_current++;
        return (new CallbackStack(first))->allocateEntry(first);
    }

private:
    static const size_t bufferSize = 8000;
    Item m_buffer[bufferSize];
    Item* m_limit;
    Item* m_current;
    CallbackStack* m_next;
};

// Non-template super class used to pass a heap around to other classes.
class BaseHeap {
public:
    virtual ~BaseHeap() { }

    // Find the page in this thread heap containing the given
    // address. Returns 0 if the address is not contained in any
    // page in this thread heap.
    virtual BaseHeapPage* heapPageFromAddress(Address) = 0;

    // Find the large object in this thread heap containing the given
    // address. Returns 0 if the address is not contained in any
    // page in this thread heap.
    virtual BaseHeapPage* largeHeapObjectFromAddress(Address) = 0;

    // Check if the given address could point to an object in this
    // heap. If so, find the start of that object and mark it using
    // the given Visitor.
    //
    // Returns true if the object was found and marked, returns false
    // otherwise.
    //
    // This is used during conservative stack scanning to
    // conservatively mark all objects that could be referenced from
    // the stack.
    virtual bool checkAndMarkLargeHeapObject(Visitor*, Address) = 0;

    // Sweep this part of the Blink heap. This finalizes dead objects
    // and builds freelists for all the unused memory.
    virtual void sweep() = 0;

    // Forcefully finalize all objects in this part of the Blink heap
    // (potentially with the exception of one object). This is used
    // during thread termination to make sure that all objects for the
    // dying thread are finalized.
    virtual void assertEmpty() = 0;

    virtual void clearFreeLists() = 0;
    virtual void clearMarks() = 0;
#ifndef NDEBUG
    virtual void getScannedStats(HeapStats&) = 0;
#endif

    virtual void makeConsistentForGC() = 0;
    virtual bool isConsistentForGC() = 0;

    // Returns a bucket number for inserting a FreeListEntry of a
    // given size. All FreeListEntries in the given bucket, n, have
    // size >= 2^n.
    static int bucketIndexForSize(size_t);
};

// Thread heaps represent a part of the per-thread Blink heap.
//
// Each Blink thread has a number of thread heaps: one general heap
// that contains any type of object and a number of heaps specialized
// for specific object types (such as Node).
//
// Each thread heap contains the functionality to allocate new objects
// (potentially adding new pages to the heap), to find and mark
// objects during conservative stack scanning and to sweep the set of
// pages after a GC.
template<typename Header>
class ThreadHeap : public BaseHeap {
public:
    ThreadHeap(ThreadState*);
    virtual ~ThreadHeap();

    virtual BaseHeapPage* heapPageFromAddress(Address);
    virtual BaseHeapPage* largeHeapObjectFromAddress(Address);
    virtual bool checkAndMarkLargeHeapObject(Visitor*, Address);
    virtual void sweep();
    virtual void assertEmpty();
    virtual void clearFreeLists();
    virtual void clearMarks();
#ifndef NDEBUG
    virtual void getScannedStats(HeapStats&);
#endif

    virtual void makeConsistentForGC();
    virtual bool isConsistentForGC();

    ThreadState* threadState() { return m_threadState; }
    HeapStats& stats() { return m_threadState->stats(); }
    HeapContainsCache* heapContainsCache() { return m_threadState->heapContainsCache(); }

    inline Address allocate(size_t, const GCInfo*);
    void addToFreeList(Address, size_t);
    void addPageToPool(HeapPage<Header>*);
    inline static size_t roundedAllocationSize(size_t size)
    {
        return allocationSizeFromSize(size) - sizeof(Header);
    }

private:
    // Once pages have been used for one thread heap they will never
    // be reused for another thread heap. Instead of unmapping, we add
    // the pages to a pool of pages to be reused later by this thread
    // heap. This is done as a security feature to avoid type
    // confusion. The heap is type segregated by having separate
    // thread heaps for various types of objects. Holding on to pages
    // ensures that the same virtual address space cannot be used for
    // objects of another type than the type contained in this thread
    // heap.
    class PagePoolEntry {
    public:
        PagePoolEntry(PageMemory* storage, PagePoolEntry* next)
            : m_storage(storage)
            , m_next(next)
        { }

        PageMemory* storage() { return m_storage; }
        PagePoolEntry* next() { return m_next; }

    private:
        PageMemory* m_storage;
        PagePoolEntry* m_next;
    };

    PLATFORM_EXPORT Address outOfLineAllocate(size_t, const GCInfo*);
    static size_t allocationSizeFromSize(size_t);
    void addPageToHeap(const GCInfo*);
    PLATFORM_EXPORT Address allocateLargeObject(size_t, const GCInfo*);
    Address currentAllocationPoint() const { return m_currentAllocationPoint; }
    size_t remainingAllocationSize() const { return m_remainingAllocationSize; }
    bool ownsNonEmptyAllocationArea() const { return currentAllocationPoint() && remainingAllocationSize(); }
    void setAllocationPoint(Address point, size_t size)
    {
        ASSERT(!point || heapPageFromAddress(point));
        ASSERT(size <= HeapPage<Header>::payloadSize());
        m_currentAllocationPoint = point;
        m_remainingAllocationSize = size;
    }
    void ensureCurrentAllocation(size_t, const GCInfo*);
    bool allocateFromFreeList(size_t);

    void freeLargeObject(LargeHeapObject<Header>*, LargeHeapObject<Header>**);

    void allocatePage(const GCInfo*);
    PageMemory* takePageFromPool();
    void clearPagePool();
    void deletePages();

    Address m_currentAllocationPoint;
    size_t m_remainingAllocationSize;

    HeapPage<Header>* m_firstPage;
    LargeHeapObject<Header>* m_firstLargeHeapObject;

    int m_biggestFreeListIndex;
    ThreadState* m_threadState;

    // All FreeListEntries in the nth list have size >= 2^n.
    FreeListEntry* m_freeLists[blinkPageSizeLog2];

    // List of pages that have been previously allocated, but are now
    // unused.
    PagePoolEntry* m_pagePool;
};

class PLATFORM_EXPORT Heap {
public:
    static void init();
    static void shutdown();

    static BaseHeapPage* contains(Address);
    static BaseHeapPage* contains(void* pointer) { return contains(reinterpret_cast<Address>(pointer)); }
    static BaseHeapPage* contains(const void* pointer) { return contains(const_cast<void*>(pointer)); }

    // Push a trace callback on the marking stack.
    static void pushTraceCallback(void* containerObject, TraceCallback);

    // Add a weak pointer callback to the weak callback work list. General
    // object pointer callbacks are added to a thread local weak callback work
    // list and the callback is called on the thread that owns the object, with
    // the closure pointer as an argument. Most of the time, the closure and
    // the containerObject can be the same thing, but the containerObject is
    // constrained to be on the heap, since the heap is used to identify the
    // correct thread.
    static void pushWeakObjectPointerCallback(void* closure, void* containerObject, WeakPointerCallback);

    // Similar to the more general pushWeakObjectPointerCallback, but cell
    // pointer callbacks are added to a static callback work list and the weak
    // callback is performed on the thread performing garbage collection. This
    // is OK because cells are just cleared and no deallocation can happen.
    static void pushWeakCellPointerCallback(void** cell, WeakPointerCallback);

    // Pop the top of the marking stack and call the callback with the visitor
    // and the object. Returns false when there is nothing more to do.
    static bool popAndInvokeTraceCallback(Visitor*);

    // Remove an item from the weak callback work list and call the callback
    // with the visitor and the closure pointer. Returns false when there is
    // nothing more to do.
    static bool popAndInvokeWeakPointerCallback(Visitor*);

    template<typename T> static Address allocate(size_t);
    template<typename T> static Address reallocate(void* previous, size_t);

    static void collectGarbage(ThreadState::StackState);
    static void collectAllGarbage();
    static void setForcePreciseGCForTesting();

    static void prepareForGC();

    // Conservatively checks whether an address is a pointer in any of the thread
    // heaps. If so marks the object pointed to as live.
    static Address checkAndMarkPointer(Visitor*, Address);

    // Collect heap stats for all threads attached to the Blink
    // garbage collector. Should only be called during garbage
    // collection where threads are known to be at safe points.
    static void getStats(HeapStats*);

    static bool isConsistentForGC();
    static void makeConsistentForGC();

    static Visitor* s_markingVisitor;

    static CallbackStack* s_markingStack;
    static CallbackStack* s_weakCallbackStack;
};

// The NoAllocationScope class is used in debug mode to catch unwanted
// allocations. E.g. allocations during GC.
template<ThreadAffinity Affinity>
class NoAllocationScope {
public:
    NoAllocationScope() : m_active(true) { enter(); }

    explicit NoAllocationScope(bool active) : m_active(active) { enter(); }

    NoAllocationScope(const NoAllocationScope& other) : m_active(other.m_active) { enter(); }

    NoAllocationScope& operator=(const NoAllocationScope& other)
    {
        release();
        m_active = other.m_active;
        enter();
        return *this;
    }

    ~NoAllocationScope() { release(); }

    void release()
    {
        if (m_active) {
            ThreadStateFor<Affinity>::state()->leaveNoAllocationScope();
            m_active = false;
        }
    }

private:
    void enter() const
    {
        if (m_active)
            ThreadStateFor<Affinity>::state()->enterNoAllocationScope();
    }

    bool m_active;
};

// Base class for objects allocated in the Blink garbage-collected
// heap.
//
// Defines a 'new' operator that allocates the memory in the
// heap. 'delete' should not be called on objects that inherit from
// GarbageCollected.
//
// Instances of GarbageCollected will *NOT* get finalized. Their
// destructor will not be called. Therefore, only classes that have
// trivial destructors with no semantic meaning (including all their
// subclasses) should inherit from GarbageCollected. If there are
// non-trival destructors in a given class or any of its subclasses,
// GarbageCollectedFinalized should be used which guarantees that the
// destructor is called on an instance when the garbage collector
// determines that it is no longer reachable.
template<typename T>
class GarbageCollected {
    WTF_MAKE_NONCOPYABLE(GarbageCollected);

    // For now direct allocation of arrays on the heap is not allowed.
    void* operator new[](size_t size);
    void operator delete[](void* p);
public:
    typedef T GarbageCollectedBase;

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

// Base class for objects allocated in the Blink garbage-collected
// heap.
//
// Defines a 'new' operator that allocates the memory in the
// heap. 'delete' should not be called on objects that inherit from
// GarbageCollected.
//
// Instances of GarbageCollectedFinalized will have their destructor
// called when the garbage collector determines that the object is no
// longer reachable.
template<typename T>
class GarbageCollectedFinalized : public GarbageCollected<T> {
    WTF_MAKE_NONCOPYABLE(GarbageCollectedFinalized);

protected:
    // finalizeGarbageCollectedObject is called when the object is
    // freed from the heap. By default finalization means calling the
    // destructor on the object. finalizeGarbageCollectedObject can be
    // overridden to support calling the destructor of a
    // subclass. This is useful for objects without vtables that
    // require explicit dispatching. The name is intentionally a bit
    // long to make name conflicts less likely.
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
// will not reclaim the memory. When the reference count reaches 0 the
// persistent handle will be deleted. When the garbage collector
// determines that there are no other references to the object it will
// be reclaimed and the destructor of the reclaimed object will be
// called at that time.
template<typename T>
class RefCountedGarbageCollected : public GarbageCollectedFinalized<T> {
    WTF_MAKE_NONCOPYABLE(RefCountedGarbageCollected);

public:
    RefCountedGarbageCollected()
        : m_refCount(1)
    {
        m_keepAlive = new Persistent<T>(static_cast<T*>(this));
    }

    // Implement method to increase reference count for use with
    // RefPtrs.
    //
    // In contrast to the normal WTF::RefCounted, the reference count
    // can reach 0 and increase again. This happens in the following
    // scenario:
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
            ASSERT(!m_keepAlive);
            ASSERT(ThreadStateFor<ThreadingTrait<T>::Affinity>::state()->contains(reinterpret_cast<Address>(this)));
            m_keepAlive = new Persistent<T>(static_cast<T*>(this));
        }
        ++m_refCount;
    }

    // Implement method to decrease reference count for use with
    // RefPtrs.
    //
    // In contrast to the normal WTF::RefCounted implementation, the
    // object itself is not deleted when the reference count reaches
    // 0. Instead, the keep-alive persistent handle is deallocated so
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
    int m_refCount;
    Persistent<T>* m_keepAlive;
};

template<typename T>
T* adoptRefCountedGarbageCollected(T* ptr)
{
    ASSERT(ptr->hasOneRef());
    ptr->deref();
    WTF::adopted(ptr);
    return ptr;
}

// Classes that contain heap references but aren't themselves heap
// allocated, have some extra macros available which allows their use
// to be restricted to cases where the garbage collector is able
// to discover their heap references.
//
// STACK_ALLOCATED(): Use if the object is only stack allocated. Heap objects
// should be in Members but you do not need the trace method as they are on
// the stack. (Down the line these might turn in to raw pointers, but for
// now Members indicates that we have thought about them and explicitly
// taken care of them.)
//
// DISALLOW_ALLOCATION(): Cannot be allocated with new operators but can
// be a part object. If it has Members you need a trace method and the
// containing object needs to call that trace method.
//
// ALLOW_ONLY_INLINE_ALLOCATION(): Allows only placement new operator.
// This disallows general allocation of this object but allows to put
// the object as a value object in collections. If these have Members you
// need to have a trace method. That trace method will be called
// automatically by the Heap collections.
//
#if COMPILER_SUPPORTS(CXX_DELETED_FUNCTIONS)
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

#else

#define DISALLOW_ALLOCATION()                          \
    private:                                           \
        void* operator new(size_t);                    \
        void* operator new(size_t, NotNullTag, void*); \
        void* operator new(size_t, void*)

#define ALLOW_ONLY_INLINE_ALLOCATION()                                              \
    public:                                                                         \
        void* operator new(size_t, NotNullTag, void* location) { return location; } \
        void* operator new(size_t, void* location) { return location; }             \
    private:                                                                        \
        void* operator new(size_t);

#define STATIC_ONLY(Type)  \
    private:               \
        Type();

#endif


// These macros insert annotations that the Blink GC plugin for clang uses for
// verification. STACK_ALLOCATED is used to declare that objects of this type
// are always stack allocated. GC_PLUGIN_IGNORE is used to make the plugin
// ignore a particular class or field when checking for proper usage. When using
// GC_PLUGIN_IGNORE a bug-number should be provided as an argument where the
// bug describes what needs to happen to remove the GC_PLUGIN_IGNORE again.
#if COMPILER(CLANG) && !defined(ADDRESS_SANITIZER)
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

NO_SANITIZE_ADDRESS
void HeapObjectHeader::checkHeader() const
{
    ASSERT(m_magic == magic);
}

Address HeapObjectHeader::payload()
{
    return reinterpret_cast<Address>(this) + objectHeaderSize;
}

size_t HeapObjectHeader::payloadSize()
{
    return size() - objectHeaderSize;
}

Address HeapObjectHeader::payloadEnd()
{
    return reinterpret_cast<Address>(this) + size();
}

NO_SANITIZE_ADDRESS
void HeapObjectHeader::mark()
{
    checkHeader();
    m_size |= markBitMask;
}

Address FinalizedHeapObjectHeader::payload()
{
    return reinterpret_cast<Address>(this) + finalizedHeaderSize;
}

size_t FinalizedHeapObjectHeader::payloadSize()
{
    return size() - finalizedHeaderSize;
}

template<typename Header>
size_t ThreadHeap<Header>::allocationSizeFromSize(size_t size)
{
    // Check the size before computing the actual allocation size. The
    // allocation size calculation can overflow for large sizes and
    // the check therefore has to happen before any calculation on the
    // size.
    RELEASE_ASSERT(size < maxHeapObjectSize);

    // Add space for header.
    size_t allocationSize = size + sizeof(Header);
    // Align size with allocation granularity.
    allocationSize = (allocationSize + allocationMask) & ~allocationMask;
    return allocationSize;
}

template<typename Header>
Address ThreadHeap<Header>::allocate(size_t size, const GCInfo* gcInfo)
{
    size_t allocationSize = allocationSizeFromSize(size);
    bool isLargeObject = allocationSize > blinkPageSize / 2;
    if (isLargeObject)
        return allocateLargeObject(allocationSize, gcInfo);
    if (m_remainingAllocationSize < allocationSize)
        return outOfLineAllocate(size, gcInfo);
    Address headerAddress = m_currentAllocationPoint;
    m_currentAllocationPoint += allocationSize;
    m_remainingAllocationSize -= allocationSize;
    Header* header = new (NotNull, headerAddress) Header(allocationSize, gcInfo);
    size_t payloadSize = allocationSize - sizeof(Header);
    stats().increaseObjectSpace(payloadSize);
    Address result = headerAddress + sizeof(*header);
    ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));
    // Unpoison the memory used for the object (payload).
    ASAN_UNPOISON_MEMORY_REGION(result, payloadSize);
    memset(result, 0, payloadSize);
    ASSERT(heapPageFromAddress(headerAddress + allocationSize - 1));
    return result;
}

// FIXME: Allocate objects that do not need finalization separately
// and use separate sweeping to not have to check for finalizers.
template<typename T>
Address Heap::allocate(size_t size)
{
    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    ASSERT(state->isAllocationAllowed());
    BaseHeap* heap = state->heap(HeapTrait<T>::index);
    Address addr =
        static_cast<typename HeapTrait<T>::HeapType*>(heap)->allocate(size, GCInfoTrait<T>::get());
    return addr;
}

// FIXME: Allocate objects that do not need finalization separately
// and use separate sweeping to not have to check for finalizers.
template<typename T>
Address Heap::reallocate(void* previous, size_t size)
{
    if (!size) {
        // If the new size is 0 this is equivalent to either
        // free(previous) or malloc(0). In both cases we do
        // nothing and return 0.
        return 0;
    }
    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    ASSERT(state->isAllocationAllowed());
    // FIXME: Currently only supports raw allocation on the
    // GeneralHeap. Hence we assume the header is a
    // FinalizedHeapObjectHeader.
    ASSERT(HeapTrait<T>::index == GeneralHeap);
    BaseHeap* heap = state->heap(HeapTrait<T>::index);
    Address address = static_cast<typename HeapTrait<T>::HeapType*>(heap)->allocate(size, GCInfoTrait<T>::get());
    if (!previous) {
        // This is equivalent to malloc(size).
        return address;
    }
    FinalizedHeapObjectHeader* previousHeader = FinalizedHeapObjectHeader::fromPayload(previous);
    ASSERT(!previousHeader->hasFinalizer());
    ASSERT(previousHeader->gcInfo() == GCInfoTrait<T>::get());
    size_t copySize = previousHeader->payloadSize();
    if (copySize > size)
        copySize = size;
    memcpy(address, previous, copySize);
    return address;
}

class HeapAllocatorQuantizer {
public:
    template<typename T>
    static size_t quantizedSize(size_t count)
    {
        RELEASE_ASSERT(count <= kMaxUnquantizedAllocation / sizeof(T));
        return HeapTrait<T>::HeapType::roundedAllocationSize(count * sizeof(T));
    }
    static const size_t kMaxUnquantizedAllocation = maxHeapObjectSize;
};

class HeapAllocator {
public:
    typedef HeapAllocatorQuantizer Quantizer;
    typedef WebCore::Visitor Visitor;
    static const bool isGarbageCollected = true;

    template <typename Return, typename Metadata>
    static Return backingMalloc(size_t size)
    {
        return malloc<Return, Metadata>(size);
    }
    template <typename Return, typename Metadata>
    static Return zeroedBackingMalloc(size_t size)
    {
        return malloc<Return, Metadata>(size);
    }
    template <typename Return, typename Metadata>
    static Return malloc(size_t size)
    {
        return reinterpret_cast<Return>(Heap::allocate<Metadata>(size));
    }
    static void backingFree(void* address) { }
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

    static void markUsingGCInfo(Visitor* visitor, const void* buffer)
    {
        visitor->mark(buffer, FinalizedHeapObjectHeader::fromPayload(buffer)->traceCallback());
    }

    static void markNoTracing(Visitor* visitor, const void* t)
    {
        visitor->mark(t, reinterpret_cast<TraceCallback>(0));
    }

    template<typename T, typename Traits>
    static void trace(Visitor* visitor, T& t)
    {
        CollectionBackingTraceTrait<WTF::ShouldBeTraced<Traits>::value, Traits::isWeak, false, T, Traits>::mark(visitor, t);
    }

    template<typename T>
    static bool hasDeadMember(Visitor*, const T&)
    {
        return false;
    }

    template<typename T>
    static bool hasDeadMember(Visitor* visitor, const Member<T>& t)
    {
        ASSERT(visitor->isAlive(t));
        return false;
    }

    template<typename T>
    static bool hasDeadMember(Visitor* visitor, const WeakMember<T>& t)
    {
        return !visitor->isAlive(t);
    }

    template<typename T, typename U>
    static bool hasDeadMember(Visitor* visitor, const WTF::KeyValuePair<T, U>& t)
    {
        return hasDeadMember(visitor, t.key) || hasDeadMember(visitor, t.value);
    }

    static void registerWeakMembers(Visitor* visitor, const void* closure, const void* object, WeakPointerCallback callback)
    {
        visitor->registerWeakMembers(closure, object, callback);
    }

    template<typename T>
    struct ResultType {
        typedef T* Type;
    };

    // The WTF classes use Allocator::VectorBackingHelper in order to find a
    // class to template their backing allocation operation on. For off-heap
    // allocations the VectorBackingHelper is a dummy class, since the class is
    // not used during allocation of backing. For on-heap allocations this
    // typedef ensures that the allocation is done with the correct templated
    // instantiation of the allocation function. This ensures the correct GC
    // map is written when backing allocations take place.
    template<typename T, typename Traits>
    struct VectorBackingHelper {
        typedef HeapVectorBacking<T, Traits> Type;
    };

    // Like the VectorBackingHelper, but this type is used for HashSet and
    // HashMap, both of which are implemented using HashTable.
    template<typename Table>
    struct HashTableBackingHelper {
        typedef HeapHashTableBacking<Table> Type;
    };

    template<typename T>
    struct OtherType {
        typedef T* Type;
    };

    template<typename T>
    static T& getOther(T* other)
    {
        return *other;
    }

private:
    template<typename T, size_t u, typename V> friend class WTF::Vector;
    template<typename T, typename U, typename V, typename W> friend class WTF::HashSet;
    template<typename T, typename U, typename V, typename W, typename X, typename Y> friend class WTF::HashMap;
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
    typename MappedTraitsArg = HashTraits<MappedArg> >
class HeapHashMap : public HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HeapAllocator> { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg> >
class HeapHashSet : public HashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> { };

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
    inline void swap(HeapDeque& other)
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

template<typename T>
struct ThreadingTrait<Member<T> > {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T>
struct ThreadingTrait<WeakMember<T> > {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename Key, typename Value, typename T, typename U, typename V>
struct ThreadingTrait<HashMap<Key, Value, T, U, V, HeapAllocator> > {
    static const ThreadAffinity Affinity =
        (ThreadingTrait<Key>::Affinity == MainThreadOnly)
        && (ThreadingTrait<Value>::Affinity == MainThreadOnly) ? MainThreadOnly : AnyThread;
};

template<typename First, typename Second>
struct ThreadingTrait<WTF::KeyValuePair<First, Second> > {
    static const ThreadAffinity Affinity =
        (ThreadingTrait<First>::Affinity == MainThreadOnly)
        && (ThreadingTrait<Second>::Affinity == MainThreadOnly) ? MainThreadOnly : AnyThread;
};

template<typename T, typename U, typename V>
struct ThreadingTrait<HashSet<T, U, V, HeapAllocator> > {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};


template<typename T, size_t inlineCapacity>
struct ThreadingTrait<Vector<T, inlineCapacity, HeapAllocator> > {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T, typename Traits>
struct ThreadingTrait<HeapVectorBacking<T, Traits> > {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T, size_t inlineCapacity>
struct ThreadingTrait<Deque<T, inlineCapacity, HeapAllocator> > {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename Table>
struct ThreadingTrait<HeapHashTableBacking<Table> > {
    typedef typename Table::KeyType Key;
    typedef typename Table::ValueType Value;
    static const ThreadAffinity Affinity =
        (ThreadingTrait<Key>::Affinity == MainThreadOnly)
        && (ThreadingTrait<Value>::Affinity == MainThreadOnly) ? MainThreadOnly : AnyThread;
};

template<typename T, typename U, typename V, typename W, typename X>
struct ThreadingTrait<HeapHashMap<T, U, V, W, X> > : public ThreadingTrait<HashMap<T, U, V, W, X, HeapAllocator> > { };

template<typename T, typename U, typename V>
struct ThreadingTrait<HeapHashSet<T, U, V> > : public ThreadingTrait<HashSet<T, U, V, HeapAllocator> > { };

template<typename T, size_t inlineCapacity>
struct ThreadingTrait<HeapVector<T, inlineCapacity> > : public ThreadingTrait<Vector<T, inlineCapacity, HeapAllocator> > { };

template<typename T, size_t inlineCapacity>
struct ThreadingTrait<HeapDeque<T, inlineCapacity> > : public ThreadingTrait<Deque<T, inlineCapacity, HeapAllocator> > { };

// The standard implementation of GCInfoTrait<T>::get() just returns a static
// from the class T, but we can't do that for HashMap, HashSet, Deque and
// Vector because they are in WTF and know nothing of GCInfos. Instead we have
// a specialization of GCInfoTrait for these four classes here.

template<typename Key, typename Value, typename T, typename U, typename V>
struct GCInfoTrait<HashMap<Key, Value, T, U, V, HeapAllocator> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename Key, typename Value, typename T, typename U, typename V>
const GCInfo GCInfoTrait<HashMap<Key, Value, T, U, V, HeapAllocator> >::info = {
    TraceTrait<HashMap<Key, Value, T, U, V, HeapAllocator> >::trace,
    0,
    false, // HashMap needs no finalizer.
};

template<typename T, typename U, typename V>
struct GCInfoTrait<HashSet<T, U, V, HeapAllocator> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename T, typename U, typename V>
const GCInfo GCInfoTrait<HashSet<T, U, V, HeapAllocator> >::info = {
    TraceTrait<HashSet<T, U, V, HeapAllocator> >::trace,
    0,
    false, // HashSet needs no finalizer.
};

template<typename T>
struct GCInfoTrait<Vector<T, 0, HeapAllocator> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename T>
const GCInfo GCInfoTrait<Vector<T, 0, HeapAllocator> >::info = {
    TraceTrait<Vector<T, 0, HeapAllocator> >::trace,
    0,
    false, // Vector needs no finalizer if it has no inline capacity.
};

template<typename T, size_t inlineCapacity>
struct GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator> > : public FinalizerTraitImpl<Vector<T, inlineCapacity, HeapAllocator>, true> { };

template<typename T, size_t inlineCapacity>
const GCInfo GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator> >::info = {
    TraceTrait<Vector<T, inlineCapacity, HeapAllocator> >::trace,
    FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator> >::finalize,
    // Finalizer is needed to destruct things stored in the inline capacity.
    inlineCapacity && VectorTraits<T>::needsDestruction,
    VTableTrait<Vector<T, inlineCapacity, HeapAllocator> >::hasVTable,
};

template<typename T>
struct GCInfoTrait<Deque<T, 0, HeapAllocator> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename T>
const GCInfo GCInfoTrait<Deque<T, 0, HeapAllocator> >::info = {
    TraceTrait<Deque<T, 0, HeapAllocator> >::trace,
    0,
    false, // Deque needs no finalizer if it has no inline capacity.
};

template<typename T, size_t inlineCapacity>
struct GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator> > : public FinalizerTraitImpl<Deque<T, inlineCapacity, HeapAllocator>, true> { };

template<typename T, size_t inlineCapacity>
const GCInfo GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator> >::info = {
    TraceTrait<Deque<T, inlineCapacity, HeapAllocator> >::trace,
    FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator> >::finalize,
    // Finalizer is needed to destruct things stored in the inline capacity.
    inlineCapacity && VectorTraits<T>::needsDestruction,
};

template<typename T, typename Traits>
struct GCInfoTrait<HeapVectorBacking<T, Traits> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename T, typename Traits>
const GCInfo GCInfoTrait<HeapVectorBacking<T, Traits> >::info = {
    TraceTrait<HeapVectorBacking<T, Traits> >::trace,
    FinalizerTrait<HeapVectorBacking<T, Traits> >::finalize,
    Traits::needsDestruction,
    false, // We don't support embedded objects in HeapVectors with vtables.
};

template<typename Table>
struct GCInfoTrait<HeapHashTableBacking<Table> > {
    static const GCInfo* get() { return &info; }
    static const GCInfo info;
};

template<typename Table>
const GCInfo GCInfoTrait<HeapHashTableBacking<Table> >::info = {
    TraceTrait<HeapHashTableBacking<Table> >::trace,
    HeapHashTableBacking<Table>::finalize,
    Table::ValueTraits::needsDestruction,
};

template<bool markWeakMembersStrongly, typename T, typename Traits>
struct BaseVisitVectorBackingTrait {
    static void mark(WebCore::Visitor* visitor, void* self)
    {
        // The allocator can oversize the allocation a little, according to
        // the allocation granularity. The extra size is included in the
        // payloadSize call below, since there is nowhere to store the
        // originally allocated memory. This assert ensures that visiting the
        // last bit of memory can't cause trouble.
        COMPILE_ASSERT(!WTF::ShouldBeTraced<Traits>::value || sizeof(T) > allocationGranularity || Traits::canInitializeWithMemset, HeapOverallocationCanCauseSpuriousVisits);

        T* array = reinterpret_cast<T*>(self);
        WebCore::FinalizedHeapObjectHeader* header = WebCore::FinalizedHeapObjectHeader::fromPayload(self);
        // Use the payload size as recorded by the heap to determine how many
        // elements to mark.
        size_t length = header->payloadSize() / sizeof(T);
        for (size_t i = 0; i < length; i++)
            CollectionBackingTraceTrait<WTF::ShouldBeTraced<Traits>::value, Traits::isWeak, markWeakMembersStrongly, T, Traits>::mark(visitor, array[i]);
    }
};

template<bool markWeakMembersStrongly, typename Table>
struct BaseVisitHashTableBackingTrait {
    typedef typename Table::ValueType Value;
    typedef typename Table::ValueTraits Traits;
    static void mark(WebCore::Visitor* visitor, void* self)
    {
        Value* array = reinterpret_cast<Value*>(self);
        WebCore::FinalizedHeapObjectHeader* header = WebCore::FinalizedHeapObjectHeader::fromPayload(self);
        size_t length = header->payloadSize() / sizeof(Value);
        for (size_t i = 0; i < length; i++) {
            if (!WTF::HashTableHelper<Value, typename Table::ExtractorType, typename Table::KeyTraitsType>::isEmptyOrDeletedBucket(array[i]))
                CollectionBackingTraceTrait<WTF::ShouldBeTraced<Traits>::value, Traits::isWeak, markWeakMembersStrongly, Value, Traits>::mark(visitor, array[i]);
        }
    }
};

template<bool markWeakMembersStrongly, typename Key, typename Value, typename Traits>
struct BaseVisitKeyValuePairTrait {
    static void mark(WebCore::Visitor* visitor, WTF::KeyValuePair<Key, Value>& self)
    {
        ASSERT(WTF::ShouldBeTraced<Traits>::value || (Traits::isWeak && markWeakMembersStrongly));
        CollectionBackingTraceTrait<WTF::ShouldBeTraced<typename Traits::KeyTraits>::value, Traits::KeyTraits::isWeak, markWeakMembersStrongly, Key, typename Traits::KeyTraits>::mark(visitor, self.key);
        CollectionBackingTraceTrait<WTF::ShouldBeTraced<typename Traits::ValueTraits>::value, Traits::ValueTraits::isWeak, markWeakMembersStrongly, Value, typename Traits::ValueTraits>::mark(visitor, self.value);
    }
};

// FFX - Things that don't need marking and have no weak pointers.
template<bool markWeakMembersStrongly, typename T, typename U>
struct CollectionBackingTraceTrait<false, false, markWeakMembersStrongly, T, U> {
    static void mark(Visitor*, const T&) { }
    static void mark(Visitor*, const void*) { }
};

// FTF - Things that don't need marking. They have weak pointers, but we are
// not marking weak pointers in this object in this GC.
template<typename T, typename U>
struct CollectionBackingTraceTrait<false, true, false, T, U> {
    static void mark(Visitor*, const T&) { }
    static void mark(Visitor*, const void*) { }
};

// For each type that we understand we have the FTT case and the TXX case. The
// FTT case is where we would not normally need to mark it, but it has weak
// pointers, and we are marking them as strong. The TXX case is the regular
// case for things that need marking.

// FTT (vector)
template<typename T, typename Traits>
struct CollectionBackingTraceTrait<false, true, true, HeapVectorBacking<T, Traits>, void> : public BaseVisitVectorBackingTrait<true, T, Traits> {
};

// TXX (vector)
template<bool isWeak, bool markWeakMembersStrongly, typename T, typename Traits>
struct CollectionBackingTraceTrait<true, isWeak, markWeakMembersStrongly, HeapVectorBacking<T, Traits>, void> : public BaseVisitVectorBackingTrait<markWeakMembersStrongly, T, Traits> {
};

// FTT (hash table)
template<typename Table>
struct CollectionBackingTraceTrait<false, true, true, HeapHashTableBacking<Table>, void> : public BaseVisitHashTableBackingTrait<true, Table> {
};

// TXX (hash table)
template<bool isWeak, bool markWeakMembersStrongly, typename Table>
struct CollectionBackingTraceTrait<true, isWeak, markWeakMembersStrongly, HeapHashTableBacking<Table>, void> : public BaseVisitHashTableBackingTrait<markWeakMembersStrongly, Table> {
};

// FTT (key value pair)
template<typename Key, typename Value, typename Traits>
struct CollectionBackingTraceTrait<false, true, true, WTF::KeyValuePair<Key, Value>, Traits> : public BaseVisitKeyValuePairTrait<true, Key, Value, Traits> {
};

// TXX (key value pair)
template<bool isWeak, bool markWeakMembersStrongly, typename Key, typename Value, typename Traits>
struct CollectionBackingTraceTrait<true, isWeak, markWeakMembersStrongly, WTF::KeyValuePair<Key, Value>, Traits> : public BaseVisitKeyValuePairTrait<markWeakMembersStrongly, Key, Value, Traits> {
};

// TFX (member)
template<bool markWeakMembersStrongly, typename T, typename Traits>
struct CollectionBackingTraceTrait<true, false, markWeakMembersStrongly, Member<T>, Traits> {
    static void mark(WebCore::Visitor* visitor, Member<T> self)
    {
        visitor->mark(self.get());
    }
};

// FTT (weak member)
template<typename T, typename Traits>
struct CollectionBackingTraceTrait<false, true, true, WeakMember<T>, Traits> {
    static void mark(WebCore::Visitor* visitor, WeakMember<T> self)
    {
        // This can mark weak members as if they were strong. The reason we
        // need this is that we don't do weak processing unless we reach the
        // backing only through the hash table. Reaching it in any other way
        // makes it impossible to update the size and deleted slot count of the
        // table, and exposes us to weak processing during iteration issues.
        visitor->mark(self.get());
    }
};

// Catch-all for things that have a way to trace. For things that contain weak
// pointers they will generally be visited weakly even if
// markWeakMembersStrongly is true. This is what you want.
template<bool isWeak, bool markWeakMembersStrongly, typename T, typename Traits>
struct CollectionBackingTraceTrait<true, isWeak, markWeakMembersStrongly, T, Traits> {
    static void mark(WebCore::Visitor* visitor, T& t)
    {
        TraceTrait<T>::trace(visitor, &t);
    }
};

template<typename T, typename Traits>
struct TraceTrait<HeapVectorBacking<T, Traits> > {
    typedef HeapVectorBacking<T, Traits> Backing;
    static void trace(WebCore::Visitor* visitor, void* self)
    {
        COMPILE_ASSERT(!Traits::isWeak, WeDontSupportWeaknessInHeapVectorsOrDeques);
        if (WTF::ShouldBeTraced<Traits>::value)
            CollectionBackingTraceTrait<WTF::ShouldBeTraced<Traits>::value, false, false, HeapVectorBacking<T, Traits>, void>::mark(visitor, self);
    }
    static void mark(Visitor* visitor, const Backing* backing)
    {
        visitor->mark(backing, &trace);
    }
    static void checkGCInfo(Visitor* visitor, const Backing* backing)
    {
#ifndef NDEBUG
        visitor->checkGCInfo(const_cast<Backing*>(backing), GCInfoTrait<Backing>::get());
#endif
    }
};

// The trace trait for the heap hashtable backing is used when we find a
// direct pointer to the backing from the conservative stack scanner. This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries. When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template<typename Table>
struct TraceTrait<HeapHashTableBacking<Table> > {
    typedef HeapHashTableBacking<Table> Backing;
    typedef typename Table::ValueTraits Traits;
    static void trace(WebCore::Visitor* visitor, void* self)
    {
        if (WTF::ShouldBeTraced<Traits>::value || Traits::isWeak)
            CollectionBackingTraceTrait<WTF::ShouldBeTraced<Traits>::value, Traits::isWeak, true, Backing, void>::mark(visitor, self);
    }
    static void mark(Visitor* visitor, const Backing* backing)
    {
        if (WTF::ShouldBeTraced<Traits>::value || Traits::isWeak)
            visitor->mark(backing, &trace);
        else
            visitor->mark(backing, 0);
    }
    static void checkGCInfo(Visitor* visitor, const Backing* backing)
    {
#ifndef NDEBUG
        visitor->checkGCInfo(const_cast<Backing*>(backing), GCInfoTrait<Backing>::get());
#endif
    }
};

template<typename Table>
void HeapHashTableBacking<Table>::finalize(void* pointer)
{
    typedef typename Table::ValueType Value;
    ASSERT(Table::ValueTraits::needsDestruction);
    FinalizedHeapObjectHeader* header = FinalizedHeapObjectHeader::fromPayload(pointer);
    // Use the payload size as recorded by the heap to determine how many
    // elements to finalize.
    size_t length = header->payloadSize() / sizeof(Value);
    Value* table = reinterpret_cast<Value*>(pointer);
    for (unsigned i = 0; i < length; i++) {
        if (!Table::isEmptyOrDeletedBucket(table[i]))
            table[i].~Value();
    }
}

template<typename T, typename U, typename V, typename W, typename X>
struct GCInfoTrait<HeapHashMap<T, U, V, W, X> > : public GCInfoTrait<HashMap<T, U, V, W, X, HeapAllocator> > { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashSet<T, U, V> > : public GCInfoTrait<HashSet<T, U, V, HeapAllocator> > { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapVector<T, inlineCapacity> > : public GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator> > { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapDeque<T, inlineCapacity> > : public GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator> > { };

template<typename T>
struct IfWeakMember;

template<typename T>
struct IfWeakMember {
    template<typename U>
    static bool isDead(Visitor*, const U&) { return false; }
};

template<typename T>
struct IfWeakMember<WeakMember<T> > {
    static bool isDead(Visitor* visitor, const WeakMember<T>& t) { return !visitor->isAlive(t.get()); }
};

#if COMPILER(CLANG)
// Clang does not export the symbols that we have explicitly asked it
// to export. This forces it to export all the methods from ThreadHeap.
template<> void ThreadHeap<FinalizedHeapObjectHeader>::addPageToHeap(const GCInfo*);
template<> void ThreadHeap<HeapObjectHeader>::addPageToHeap(const GCInfo*);
extern template class PLATFORM_EXPORT ThreadHeap<FinalizedHeapObjectHeader>;
extern template class PLATFORM_EXPORT ThreadHeap<HeapObjectHeader>;
#endif

}

#endif // Heap_h

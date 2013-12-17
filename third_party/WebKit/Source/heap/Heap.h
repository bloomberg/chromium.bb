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

#include "heap/HeapExport.h"
#include "heap/Visitor.h"

#include "wtf/Assertions.h"

#include <stdint.h>

namespace WebCore {

// ASAN integration defintions
#if COMPILER(CLANG)
#define USE_ASAN (__has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__))
#else
#define USE_ASAN 0
#endif

#if USE_ASAN
extern "C" {
    // Marks memory region [addr, addr+size) as unaddressable.
    // This memory must be previously allocated by the user program. Accessing
    // addresses in this region from instrumented code is forbidden until
    // this region is unpoisoned. This function is not guaranteed to poison
    // the whole region - it may poison only subregion of [addr, addr+size) due
    // to ASan alignment restrictions.
    // Method is NOT thread-safe in the sense that no two threads can
    // (un)poison memory in the same memory region simultaneously.
    void __asan_poison_memory_region(void const volatile*, size_t);
    // Marks memory region [addr, addr+size) as addressable.
    // This memory must be previously allocated by the user program. Accessing
    // addresses in this region is allowed until this region is poisoned again.
    // This function may unpoison a superregion of [addr, addr+size) due to
    // ASan alignment restrictions.
    // Method is NOT thread-safe in the sense that no two threads can
    // (un)poison memory in the same memory region simultaneously.
    void __asan_unpoison_memory_region(void const volatile*, size_t);

    // User code should use macros instead of functions.
#define ASAN_POISON_MEMORY_REGION(addr, size)   \
    __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
    __asan_unpoison_memory_region((addr), (size))
#define NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
    const size_t asanMagic = 0xabefeed0;
    const size_t asanDeferMemoryReuseCount = 2;
    const size_t asanDeferMemoryReuseMask = 0x3;
}
#else
#define ASAN_POISON_MEMORY_REGION(addr, size)   \
    ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
    ((void)(addr), (void)(size))
#define NO_SANITIZE_ADDRESS
#endif

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

typedef uint8_t* Address;

class PageMemory;

size_t osPageSize();

#ifndef NDEBUG
// Sanity check for a page header address: the address of the page
// header should be OS page size away from being Blink page size
// aligned.
inline bool isPageHeaderAddress(Address address)
{
    return !((reinterpret_cast<uintptr_t>(address) & blinkPageOffsetMask) - osPageSize());
}
#endif

// Common header for heap pages.
class BaseHeapPage {
public:
    BaseHeapPage(PageMemory* storage, const GCInfo* gcInfo)
        : m_storage(storage)
        , m_gcInfo(gcInfo)
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
    const GCInfo* gcInfo() { return m_gcInfo; }

private:
    PageMemory* m_storage;
    // The BaseHeapPage contains three pointers (vtable, m_storage,
    // and m_gcInfo) and therefore not 8 byte aligned on 32 bit
    // architectures. Force 8 byte alignment with a union.
    union {
        const GCInfo* m_gcInfo;
        uint64_t m_ensureAligned;
    };
};

COMPILE_ASSERT(!(sizeof(BaseHeapPage) % allocationGranularity), BaseHeapPage_should_be_8_byte_aligned);

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
    LargeHeapObject(PageMemory* storage, const GCInfo* gcInfo) : BaseHeapPage(storage, gcInfo) { }

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
        return heapObjectHeader()->size() + sizeof(LargeHeapObject<Header>);
    }

    Address payload() { return heapObjectHeader()->payload(); }
    size_t payloadSize() { return heapObjectHeader()->payloadSize(); }

    Header* heapObjectHeader()
    {
        Address headerAddress = address() + sizeof(LargeHeapObject<Header>);
        return reinterpret_cast<Header*>(headerAddress);
    }

    bool isMarked();
    void unmark();
    // FIXME: Add back when HeapStats have been added.
    // void getStats(HeapStats&);
    void mark(Visitor*);
    void finalize();

private:
    friend class Heap;
    // FIXME: Add back when ThreadHeap has been added.
    // friend class ThreadHeap<Header>;

    LargeHeapObject<Header>* m_next;
};

// The BasicObjectHeader is the minimal object header. It is used when
// encountering heap space of size allocationGranularity to mark it as
// as freelist entry.
class BasicObjectHeader {
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
class HeapObjectHeader : public BasicObjectHeader {
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

NO_SANITIZE_ADDRESS
void HeapObjectHeader::checkHeader() const
{
    // FIXME: with ThreadLocalHeaps Heap::contains is not thread safe
    // but introducing locks in this place does not seem like a good
    // idea.
#ifndef NDEBUG
    ASSERT(m_magic == magic);
#endif
}

Address HeapObjectHeader::payload()
{
    return reinterpret_cast<Address>(this) + objectHeaderSize;
}

size_t HeapObjectHeader::payloadSize()
{
    return (size() - objectHeaderSize) & ~allocationMask;
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

// Each object on the GeneralHeap needs to carry a pointer to its
// own GCInfo structure for tracing and potential finalization.
class FinalizedHeapObjectHeader : public HeapObjectHeader {
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
    const char* typeMarker() { return m_gcInfo->m_typeMarker; }

    NO_SANITIZE_ADDRESS
    TraceCallback traceCallback() { return m_gcInfo->m_trace; }

    void finalize();

    NO_SANITIZE_ADDRESS
    inline bool hasFinalizer() { return m_gcInfo->hasFinalizer(); }

    static FinalizedHeapObjectHeader* fromPayload(const void*);

#if TRACE_GC_USING_CLASSOF
    const char* classOf()
    {
        const char* className = 0;
        if (m_gcInfo->m_classOf)
            className = m_gcInfo->m_classOf(payload());
        return className ? className : typeMarker();
    }
#endif

private:
    const GCInfo* m_gcInfo;
};

const size_t finalizedHeaderSize = sizeof(FinalizedHeapObjectHeader);

Address FinalizedHeapObjectHeader::payload()
{
    return reinterpret_cast<Address>(this) + finalizedHeaderSize;
}

size_t FinalizedHeapObjectHeader::payloadSize()
{
    return size() - finalizedHeaderSize;
}

class HEAP_EXPORT Heap {
public:
    static void init(intptr_t* startOfStack);
    static void shutdown();
};

}

#endif // Heap_h

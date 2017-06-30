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

#ifndef HeapPage_h
#define HeapPage_h

#include <stdint.h>
#include "base/trace_event/memory_allocator_dump.h"
#include "build/build_config.h"
#include "platform/PlatformExport.h"
#include "platform/heap/BlinkGC.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/AddressSanitizer.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/ContainerAnnotations.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/allocator/Partitions.h"

namespace blink {

const size_t kBlinkPageSizeLog2 = 17;
const size_t kBlinkPageSize = 1 << kBlinkPageSizeLog2;
const size_t kBlinkPageOffsetMask = kBlinkPageSize - 1;
const size_t kBlinkPageBaseMask = ~kBlinkPageOffsetMask;

// We allocate pages at random addresses but in groups of
// blinkPagesPerRegion at a given random address. We group pages to
// not spread out too much over the address space which would blow
// away the page tables and lead to bad performance.
const size_t kBlinkPagesPerRegion = 10;

// TODO(nya): Replace this with something like #if ENABLE_NACL.
#if 0
// NaCl's system page size is 64 KB. This causes a problem in Oilpan's heap
// layout because Oilpan allocates two guard pages for each blink page
// (whose size is 128 KB). So we don't use guard pages in NaCl.
const size_t blinkGuardPageSize = 0;
#else
const size_t kBlinkGuardPageSize = base::kSystemPageSize;
#endif

// Double precision floats are more efficient when 8 byte aligned, so we 8 byte
// align all allocations even on 32 bit.
const size_t kAllocationGranularity = 8;
const size_t kAllocationMask = kAllocationGranularity - 1;
const size_t kObjectStartBitMapSize =
    (kBlinkPageSize + ((8 * kAllocationGranularity) - 1)) /
    (8 * kAllocationGranularity);
const size_t kReservedForObjectBitMap =
    ((kObjectStartBitMapSize + kAllocationMask) & ~kAllocationMask);
const size_t kMaxHeapObjectSizeLog2 = 27;
const size_t kMaxHeapObjectSize = 1 << kMaxHeapObjectSizeLog2;
const size_t kLargeObjectSizeThreshold = kBlinkPageSize / 2;

// A zap value used for freed memory that is allowed to be added to the free
// list in the next addToFreeList().
const uint8_t kReuseAllowedZapValue = 0x2a;
// A zap value used for freed memory that is forbidden to be added to the free
// list in the next addToFreeList().
const uint8_t kReuseForbiddenZapValue = 0x2c;

// In non-production builds, memory is zapped when it's freed. The zapped
// memory is zeroed out when the memory is reused in
// ThreadHeap::allocateObject().
// In production builds, memory is not zapped (for performance). The memory
// is just zeroed out when it is added to the free list.
#if defined(MEMORY_SANITIZER)
// TODO(kojii): We actually need __msan_poison/unpoison here, but it'll be
// added later.
#define SET_MEMORY_INACCESSIBLE(address, size) \
  FreeList::ZapFreedMemory(address, size);
#define SET_MEMORY_ACCESSIBLE(address, size) memset((address), 0, (size))
#define CHECK_MEMORY_INACCESSIBLE(address, size)     \
  ASAN_UNPOISON_MEMORY_REGION(address, size);        \
  FreeList::CheckFreedMemoryIsZapped(address, size); \
  ASAN_POISON_MEMORY_REGION(address, size)
#elif DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define SET_MEMORY_INACCESSIBLE(address, size) \
  FreeList::ZapFreedMemory(address, size);     \
  ASAN_POISON_MEMORY_REGION(address, size)
#define SET_MEMORY_ACCESSIBLE(address, size)  \
  ASAN_UNPOISON_MEMORY_REGION(address, size); \
  memset((address), 0, (size))
#define CHECK_MEMORY_INACCESSIBLE(address, size)     \
  ASAN_UNPOISON_MEMORY_REGION(address, size);        \
  FreeList::CheckFreedMemoryIsZapped(address, size); \
  ASAN_POISON_MEMORY_REGION(address, size)
#else
#define SET_MEMORY_INACCESSIBLE(address, size) memset((address), 0, (size))
#define SET_MEMORY_ACCESSIBLE(address, size) \
  do {                                       \
  } while (false)
#define CHECK_MEMORY_INACCESSIBLE(address, size) \
  do {                                           \
  } while (false)
#endif

class NormalPageArena;
class PageMemory;

// HeapObjectHeader is a 64-bit object that has the following layout:
//
// | random magic value (32 bits) |
// | gcInfoIndex (14 bits)        |
// | DOM mark bit (1 bit)         |
// | size (14 bits)               |
// | dead bit (1 bit)             |
// | freed bit (1 bit)            |
// | mark bit (1 bit)             |
//
// - For non-large objects, 14 bits are enough for |size| because the Blink
//   page size is 2^17 bytes and each object is guaranteed to be aligned on a
//   2^3 byte boundary.
// - For large objects, |size| is 0. The actual size of a large object is
//   stored in |LargeObjectPage::m_payloadSize|.
// - 1 bit used to mark DOM trees for V8.
// - 14 bits are enough for |gcInfoIndex| because there are fewer than 2^14
//   types in Blink.
const size_t kHeaderWrapperMarkBitMask = 1u << 17;
const size_t kHeaderGCInfoIndexShift = 18;
const size_t kHeaderGCInfoIndexMask = (static_cast<size_t>((1 << 14) - 1))
                                      << kHeaderGCInfoIndexShift;
const size_t kHeaderSizeMask = (static_cast<size_t>((1 << 14) - 1)) << 3;
const size_t kHeaderMarkBitMask = 1;
const size_t kHeaderFreedBitMask = 2;
// TODO(haraken): Remove the dead bit. It is used only by a header of
// a promptly freed object.
const size_t kHeaderDeadBitMask = 4;
// On free-list entries we reuse the dead bit to distinguish a normal free-list
// entry from one that has been promptly freed.
const size_t kHeaderPromptlyFreedBitMask =
    kHeaderFreedBitMask | kHeaderDeadBitMask;
const size_t kLargeObjectSizeInHeader = 0;
const size_t kGcInfoIndexForFreeListHeader = 0;
const size_t kNonLargeObjectPageSizeMax = 1 << 17;

static_assert(
    kNonLargeObjectPageSizeMax >= kBlinkPageSize,
    "max size supported by HeapObjectHeader must at least be blinkPageSize");

class PLATFORM_EXPORT HeapObjectHeader {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  // If |gcInfoIndex| is 0, this header is interpreted as a free list header.
  NO_SANITIZE_ADDRESS
  HeapObjectHeader(size_t size, size_t gc_info_index) {
    // sizeof(HeapObjectHeader) must be equal to or smaller than
    // |allocationGranularity|, because |HeapObjectHeader| is used as a header
    // for a freed entry. Given that the smallest entry size is
    // |allocationGranurarity|, |HeapObjectHeader| must fit into the size.
    static_assert(
        sizeof(HeapObjectHeader) <= kAllocationGranularity,
        "size of HeapObjectHeader must be smaller than allocationGranularity");
#if defined(ARCH_CPU_64_BITS)
    static_assert(sizeof(HeapObjectHeader) == 8,
                  "sizeof(HeapObjectHeader) must be 8 bytes");
    magic_ = GetMagic();
#endif

    DCHECK(gc_info_index < GCInfoTable::kMaxIndex);
    DCHECK_LT(size, kNonLargeObjectPageSizeMax);
    DCHECK(!(size & kAllocationMask));
    encoded_ = static_cast<uint32_t>(
        (gc_info_index << kHeaderGCInfoIndexShift) | size |
        (gc_info_index == kGcInfoIndexForFreeListHeader ? kHeaderFreedBitMask
                                                        : 0));
  }

  NO_SANITIZE_ADDRESS bool IsFree() const {
    return encoded_ & kHeaderFreedBitMask;
  }

  NO_SANITIZE_ADDRESS bool IsPromptlyFreed() const {
    return (encoded_ & kHeaderPromptlyFreedBitMask) ==
           kHeaderPromptlyFreedBitMask;
  }

  NO_SANITIZE_ADDRESS void MarkPromptlyFreed() {
    encoded_ |= kHeaderPromptlyFreedBitMask;
  }

  size_t size() const;

  NO_SANITIZE_ADDRESS size_t GcInfoIndex() const {
    return (encoded_ & kHeaderGCInfoIndexMask) >> kHeaderGCInfoIndexShift;
  }

  NO_SANITIZE_ADDRESS void SetSize(size_t size) {
    DCHECK_LT(size, kNonLargeObjectPageSizeMax);
    CheckHeader();
    encoded_ = static_cast<uint32_t>(size) | (encoded_ & ~kHeaderSizeMask);
  }

  bool IsWrapperHeaderMarked() const;
  void MarkWrapperHeader();
  void UnmarkWrapperHeader();
  bool IsMarked() const;
  void Mark();
  void Unmark();

  // The payload starts directly after the HeapObjectHeader, and the payload
  // size does not include the sizeof(HeapObjectHeader).
  Address Payload();
  size_t PayloadSize();
  Address PayloadEnd();

  void Finalize(Address, size_t);
  static HeapObjectHeader* FromPayload(const void*);

  // Some callers formerly called |fromPayload| only for its side-effect of
  // calling |checkHeader| (which is now private). This function does that, but
  // its explanatory name makes the intention at the call sites easier to
  // understand, and is public.
  static void CheckFromPayload(const void*);

  // Returns true if magic number is valid.
  bool IsValid() const;

  static const uint32_t kZappedMagic = 0xDEAD4321;

 protected:
#if DCHECK_IS_ON() && defined(ARCH_CPU_64_BITS)
  // Zap |m_magic| with a new magic number that means there was once an object
  // allocated here, but it was freed because nobody marked it during GC.
  void ZapMagic();
#endif

 private:
  void CheckHeader() const;

#if defined(ARCH_CPU_64_BITS)
  // Returns a random value.
  //
  // The implementation gets its randomness from the locations of 2 independent
  // sources of address space layout randomization: a function in a Chrome
  // executable image, and a function in an external DLL/so. This implementation
  // should be fast and small, and should have the benefit of requiring
  // attackers to discover and use 2 independent weak infoleak bugs, or 1
  // arbitrary infoleak bug (used twice).
  uint32_t GetMagic() const;
  uint32_t magic_;
#endif  // defined(ARCH_CPU_64_BITS)

  uint32_t encoded_;
};

class FreeListEntry final : public HeapObjectHeader {
 public:
  NO_SANITIZE_ADDRESS
  explicit FreeListEntry(size_t size)
      : HeapObjectHeader(size, kGcInfoIndexForFreeListHeader), next_(nullptr) {
#if DCHECK_IS_ON() && defined(ARCH_CPU_64_BITS)
    DCHECK_GE(size, sizeof(HeapObjectHeader));
    ZapMagic();
#endif
  }

  Address GetAddress() { return reinterpret_cast<Address>(this); }

  NO_SANITIZE_ADDRESS
  void Unlink(FreeListEntry** prev_next) {
    *prev_next = next_;
    next_ = nullptr;
  }

  NO_SANITIZE_ADDRESS
  void Link(FreeListEntry** prev_next) {
    next_ = *prev_next;
    *prev_next = this;
  }

  NO_SANITIZE_ADDRESS
  FreeListEntry* Next() const { return next_; }

  NO_SANITIZE_ADDRESS
  void Append(FreeListEntry* next) {
    DCHECK(!next_);
    next_ = next;
  }

 private:
  FreeListEntry* next_;
};

// Blink heap pages are set up with a guard page before and after the payload.
inline size_t BlinkPagePayloadSize() {
  return kBlinkPageSize - 2 * kBlinkGuardPageSize;
}

// Blink heap pages are aligned to the Blink heap page size. Therefore, the
// start of a Blink page can be obtained by rounding down to the Blink page
// size.
inline Address RoundToBlinkPageStart(Address address) {
  return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) &
                                   kBlinkPageBaseMask);
}

inline Address RoundToBlinkPageEnd(Address address) {
  return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address - 1) &
                                   kBlinkPageBaseMask) +
         kBlinkPageSize;
}

// Masks an address down to the enclosing Blink page base address.
inline Address BlinkPageAddress(Address address) {
  return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) &
                                   kBlinkPageBaseMask);
}

inline bool VTableInitialized(void* object_pointer) {
  return !!(*reinterpret_cast<Address*>(object_pointer));
}

#if DCHECK_IS_ON()

// Sanity check for a page header address: the address of the page header should
// be 1 OS page size away from being Blink page size-aligned.
inline bool IsPageHeaderAddress(Address address) {
  return !((reinterpret_cast<uintptr_t>(address) & kBlinkPageOffsetMask) -
           kBlinkGuardPageSize);
}

// Callback used for unit testing the marking of conservative pointers
// (|checkAndMarkPointer|). For each pointer that has been discovered to point
// to a heap object, the callback is invoked with a pointer to its header. If
// the callback returns true, the object will not be marked.
using MarkedPointerCallbackForTesting = bool (*)(HeapObjectHeader*);
#endif

// |BasePage| is a base class for |NormalPage| and |LargeObjectPage|.
//
// - |NormalPage| is a page whose size is |blinkPageSize|. A |NormalPage| can
//   contain multiple objects. An object whose size is smaller than
//   |largeObjectSizeThreshold| is stored in a |NormalPage|.
//
// - |LargeObjectPage| is a page that contains only one object. The object size
//   is arbitrary. An object whose size is larger than |blinkPageSize| is stored
//   as a single project in |LargeObjectPage|.
//
// Note: An object whose size is between |largeObjectSizeThreshold| and
// |blinkPageSize| can go to either of |NormalPage| or |LargeObjectPage|.
class BasePage {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  BasePage(PageMemory*, BaseArena*);
  virtual ~BasePage() {}

  void Link(BasePage** previous_next) {
    next_ = *previous_next;
    *previous_next = this;
  }
  void Unlink(BasePage** previous_next) {
    *previous_next = next_;
    next_ = nullptr;
  }
  BasePage* Next() const { return next_; }

  // Virtual methods are slow. So performance-sensitive methods should be
  // defined as non-virtual methods on |NormalPage| and |LargeObjectPage|. The
  // following methods are not performance-sensitive.
  virtual size_t ObjectPayloadSizeForTesting() = 0;
  virtual bool IsEmpty() = 0;
  virtual void RemoveFromHeap() = 0;
  virtual void Sweep() = 0;
  virtual void MakeConsistentForMutator() = 0;
  virtual void InvalidateObjectStartBitmap() = 0;

#if defined(ADDRESS_SANITIZER)
  virtual void PoisonUnmarkedObjects() = 0;
#endif

  // Check if the given address points to an object in this heap page. If so,
  // find the start of that object and mark it using the given |Visitor|.
  // Otherwise do nothing. The pointer must be within the same aligned
  // |blinkPageSize| as |this|.
  //
  // This is used during conservative stack scanning to conservatively mark all
  // objects that could be referenced from the stack.
  virtual void CheckAndMarkPointer(Visitor*, Address) = 0;
#if DCHECK_IS_ON()
  virtual void CheckAndMarkPointer(Visitor*,
                                   Address,
                                   MarkedPointerCallbackForTesting) = 0;
#endif

  class HeapSnapshotInfo {
    STACK_ALLOCATED();

   public:
    size_t free_count = 0;
    size_t free_size = 0;
  };

  virtual void TakeSnapshot(base::trace_event::MemoryAllocatorDump*,
                            ThreadState::GCSnapshotInfo&,
                            HeapSnapshotInfo&) = 0;
#if DCHECK_IS_ON()
  virtual bool Contains(Address) = 0;
#endif
  virtual size_t size() = 0;
  virtual bool IsLargeObjectPage() { return false; }

  Address GetAddress() { return reinterpret_cast<Address>(this); }
  PageMemory* Storage() const { return storage_; }
  BaseArena* Arena() const { return arena_; }

  // Returns true if this page has been swept by the ongoing lazy sweep.
  bool HasBeenSwept() const { return swept_; }

  void MarkAsSwept() {
    DCHECK(!swept_);
    swept_ = true;
  }

  void MarkAsUnswept() {
    DCHECK(swept_);
    swept_ = false;
  }

 private:
  PageMemory* storage_;
  BaseArena* arena_;
  BasePage* next_;

  // Track the sweeping state of a page. Set to false at the start of a sweep,
  // true  upon completion of lazy sweeping.
  bool swept_;
  friend class BaseArena;
};

class NormalPage final : public BasePage {
 public:
  NormalPage(PageMemory*, BaseArena*);

  Address Payload() { return GetAddress() + PageHeaderSize(); }
  size_t PayloadSize() {
    return (BlinkPagePayloadSize() - PageHeaderSize()) & ~kAllocationMask;
  }
  Address PayloadEnd() { return Payload() + PayloadSize(); }
  bool ContainedInObjectPayload(Address address) {
    return Payload() <= address && address < PayloadEnd();
  }

  size_t ObjectPayloadSizeForTesting() override;
  bool IsEmpty() override;
  void RemoveFromHeap() override;
  void Sweep() override;
  void MakeConsistentForMutator() override;
  void InvalidateObjectStartBitmap() override {
    object_start_bit_map_computed_ = false;
  }
#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects() override;
#endif
  void CheckAndMarkPointer(Visitor*, Address) override;
#if DCHECK_IS_ON()
  void CheckAndMarkPointer(Visitor*,
                           Address,
                           MarkedPointerCallbackForTesting) override;
#endif

  void TakeSnapshot(base::trace_event::MemoryAllocatorDump*,
                    ThreadState::GCSnapshotInfo&,
                    HeapSnapshotInfo&) override;
#if DCHECK_IS_ON()
  // Returns true for the whole |blinkPageSize| page that the page is on, even
  // for the header, and the unmapped guard page at the start. That ensures the
  // result can be used to populate the negative page cache.
  bool Contains(Address) override;
#endif
  size_t size() override { return kBlinkPageSize; }
  static size_t PageHeaderSize() {
    // Compute the amount of padding we have to add to a header to make the size
    // of the header plus the padding a multiple of 8 bytes.
    size_t padding_size =
        (sizeof(NormalPage) + kAllocationGranularity -
         (sizeof(HeapObjectHeader) % kAllocationGranularity)) %
        kAllocationGranularity;
    return sizeof(NormalPage) + padding_size;
  }

  inline NormalPageArena* ArenaForNormalPage() const;

  // Context object holding the state of the arena page compaction pass, passed
  // in when compacting individual pages.
  class CompactionContext {
    STACK_ALLOCATED();

   public:
    // Page compacting into.
    NormalPage* current_page_ = nullptr;
    // Offset into |m_currentPage| to the next free address.
    size_t allocation_point_ = 0;
    // Chain of available pages to use for compaction. Page compaction picks the
    // next one when the current one is exhausted.
    BasePage* available_pages_ = nullptr;
    // Chain of pages that have been compacted. Page compaction will add
    // compacted pages once the current one becomes exhausted.
    BasePage** compacted_pages_ = nullptr;
  };

  void SweepAndCompact(CompactionContext&);

 private:
  HeapObjectHeader* FindHeaderFromAddress(Address);
  void PopulateObjectStartBitMap();

  bool object_start_bit_map_computed_;
  uint8_t object_start_bit_map_[kReservedForObjectBitMap];
};

// Large allocations are allocated as separate objects and linked in a list.
//
// In order to use the same memory allocation routines for everything allocated
// in the heap, large objects are considered heap pages containing only one
// object.
class LargeObjectPage final : public BasePage {
 public:
  LargeObjectPage(PageMemory*, BaseArena*, size_t);

  // LargeObjectPage has the following memory layout:
  //
  //     | metadata | HeapObjectHeader | ObjectPayload |
  //
  // LargeObjectPage::PayloadSize() returns the size of HeapObjectHeader and
  // ObjectPayload. HeapObjectHeader::PayloadSize() returns just the size of
  // ObjectPayload.
  Address Payload() { return GetHeapObjectHeader()->Payload(); }
  size_t PayloadSize() { return payload_size_; }
  Address PayloadEnd() { return Payload() + PayloadSize(); }
  bool ContainedInObjectPayload(Address address) {
    return Payload() <= address && address < PayloadEnd();
  }

  size_t ObjectPayloadSizeForTesting() override;
  bool IsEmpty() override;
  void RemoveFromHeap() override;
  void Sweep() override;
  void MakeConsistentForMutator() override;
  void InvalidateObjectStartBitmap() override {}
#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects() override;
#endif
  void CheckAndMarkPointer(Visitor*, Address) override;
#if DCHECK_IS_ON()
  void CheckAndMarkPointer(Visitor*,
                           Address,
                           MarkedPointerCallbackForTesting) override;
#endif

  void TakeSnapshot(base::trace_event::MemoryAllocatorDump*,
                    ThreadState::GCSnapshotInfo&,
                    HeapSnapshotInfo&) override;
#if DCHECK_IS_ON()
  // Returns true for any address that is on one of the pages that this large
  // object uses. That ensures that we can use a negative result to populate the
  // negative page cache.
  bool Contains(Address) override;
#endif
  virtual size_t size() {
    return PageHeaderSize() + sizeof(HeapObjectHeader) + payload_size_;
  }
  static size_t PageHeaderSize() {
    // Compute the amount of padding we have to add to a header to make the size
    // of the header plus the padding a multiple of 8 bytes.
    size_t padding_size =
        (sizeof(LargeObjectPage) + kAllocationGranularity -
         (sizeof(HeapObjectHeader) % kAllocationGranularity)) %
        kAllocationGranularity;
    return sizeof(LargeObjectPage) + padding_size;
  }
  bool IsLargeObjectPage() override { return true; }

  HeapObjectHeader* GetHeapObjectHeader() {
    Address header_address = GetAddress() + PageHeaderSize();
    return reinterpret_cast<HeapObjectHeader*>(header_address);
  }

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  void SetIsVectorBackingPage() { is_vector_backing_page_ = true; }
  bool IsVectorBackingPage() const { return is_vector_backing_page_; }
#endif

 private:
  size_t payload_size_;
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  bool is_vector_backing_page_;
#endif
};

// |HeapDoesNotContainCache| provides a fast way to determine whether an
// aribtrary pointer-sized word can be interpreted as a pointer to an area that
// is managed by the garbage collected Blink heap. This is a cache of 'pages'
// that have previously been determined to be wholly outside of the heap. The
// size of these pages must be smaller than the allocation alignment of the heap
// pages. We determine off-heap-ness by rounding down the pointer to the nearest
// page and looking up the page in the cache. If there is a miss in the cache we
// can determine the status of the pointer precisely using the heap
// |RegionTree|.
//
// This is a negative cache, so it must be flushed when memory is added to the
// heap.
class HeapDoesNotContainCache {
  USING_FAST_MALLOC(HeapDoesNotContainCache);

 public:
  HeapDoesNotContainCache() : has_entries_(false) {
    // Start by flushing the cache in a non-empty state to initialize all the
    // cache entries.
    for (int i = 0; i < kNumberOfEntries; ++i)
      entries_[i] = nullptr;
  }

  void Flush();
  bool IsEmpty() { return !has_entries_; }

  // Perform a lookup in the cache.
  //
  // If lookup returns false, the argument address was not found in the cache
  // and it is unknown if the address is in the Blink heap.
  //
  // If lookup returns true, the argument address was found in the cache which
  // means the address is not in the heap.
  PLATFORM_EXPORT bool Lookup(Address);

  // Add an entry to the cache.
  PLATFORM_EXPORT void AddEntry(Address);

 private:
  static const int kNumberOfEntriesLog2 = 12;
  static const int kNumberOfEntries = 1 << kNumberOfEntriesLog2;

  static size_t GetHash(Address);

  Address entries_[kNumberOfEntries];
  bool has_entries_;
};

class FreeList {
  DISALLOW_NEW();

 public:
  FreeList();

  void AddToFreeList(Address, size_t);
  void Clear();

  // Returns a bucket number for inserting a |FreeListEntry| of a given size.
  // All entries in the given bucket, n, have size >= 2^n.
  static int BucketIndexForSize(size_t);

  // Returns true if the freelist snapshot is captured.
  bool TakeSnapshot(const String& dump_base_name);

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  static void GetAllowedAndForbiddenCounts(Address, size_t, size_t&, size_t&);
  static void ZapFreedMemory(Address, size_t);
  static void CheckFreedMemoryIsZapped(Address, size_t);
#endif

 private:
  int biggest_free_list_index_;

  // All |FreeListEntry|s in the nth list have size >= 2^n.
  FreeListEntry* free_lists_[kBlinkPageSizeLog2];

  size_t FreeListSize() const;

  friend class NormalPageArena;
};

// Each thread has a number of thread arenas (e.g., Generic arenas, typed arenas
// for |Node|, arenas for collection backings, etc.) and |BaseArena| represents
// each thread arena.
//
// |BaseArena| is a parent class of |NormalPageArena| and |LargeObjectArena|.
// |NormalPageArena| represents a part of a heap that contains |NormalPage|s,
// and |LargeObjectArena| represents a part of a heap that contains
// |LargeObjectPage|s.
class PLATFORM_EXPORT BaseArena {
  USING_FAST_MALLOC(BaseArena);

 public:
  BaseArena(ThreadState*, int);
  virtual ~BaseArena();
  void RemoveAllPages();

  void TakeSnapshot(const String& dump_base_name, ThreadState::GCSnapshotInfo&);
#if DCHECK_IS_ON()
  BasePage* FindPageFromAddress(Address);
#endif
  virtual void TakeFreelistSnapshot(const String& dump_base_name) {}
  virtual void ClearFreeLists() {}
  void MakeConsistentForGC();
  void MakeConsistentForMutator();
#if DCHECK_IS_ON()
  virtual bool IsConsistentForGC() = 0;
#endif
  size_t ObjectPayloadSizeForTesting();
  void PrepareForSweep();
#if defined(ADDRESS_SANITIZER)
  void PoisonArena();
#endif
  Address LazySweep(size_t, size_t gc_info_index);
  void SweepUnsweptPage();
  // Returns true if we have swept all pages within the deadline. Returns false
  // otherwise.
  bool LazySweepWithDeadline(double deadline_seconds);
  void CompleteSweep();

  ThreadState* GetThreadState() { return thread_state_; }
  int ArenaIndex() const { return index_; }

  Address AllocateLargeObject(size_t allocation_size, size_t gc_info_index);

  bool WillObjectBeLazilySwept(BasePage*, void*) const;

 protected:
  BasePage* first_page_;
  BasePage* first_unswept_page_;

 private:
  virtual Address LazySweepPages(size_t, size_t gc_info_index) = 0;

  ThreadState* thread_state_;

  // Index into the page pools. This is used to ensure that the pages of the
  // same type go into the correct page pool and thus avoid type confusion.
  int index_;
};

class PLATFORM_EXPORT NormalPageArena final : public BaseArena {
 public:
  NormalPageArena(ThreadState*, int);
  void AddToFreeList(Address address, size_t size) {
#if DCHECK_IS_ON()
    DCHECK(FindPageFromAddress(address));
    DCHECK(FindPageFromAddress(address + size - 1));
#endif
    free_list_.AddToFreeList(address, size);
  }
  void ClearFreeLists() override;
#if DCHECK_IS_ON()
  bool IsConsistentForGC() override;
  bool PagesToBeSweptContains(Address);
#endif
  void TakeFreelistSnapshot(const String& dump_base_name) override;

  Address AllocateObject(size_t allocation_size, size_t gc_info_index);

  void FreePage(NormalPage*);

  bool Coalesce();
  void PromptlyFreeObject(HeapObjectHeader*);
  bool ExpandObject(HeapObjectHeader*, size_t);
  bool ShrinkObject(HeapObjectHeader*, size_t);
  void DecreasePromptlyFreedSize(size_t size) { promptly_freed_size_ -= size; }

  bool IsObjectAllocatedAtAllocationPoint(HeapObjectHeader* header) {
    return header->PayloadEnd() == current_allocation_point_;
  }

  bool IsLazySweeping() const { return is_lazy_sweeping_; }
  void SetIsLazySweeping(bool flag) { is_lazy_sweeping_ = flag; }

  size_t ArenaSize();
  size_t FreeListSize();

  void SweepAndCompact();

 private:
  void AllocatePage();

  Address OutOfLineAllocate(size_t allocation_size, size_t gc_info_index);
  Address AllocateFromFreeList(size_t, size_t gc_info_index);

  Address LazySweepPages(size_t, size_t gc_info_index) override;

  Address CurrentAllocationPoint() const { return current_allocation_point_; }
  bool HasCurrentAllocationArea() const {
    return CurrentAllocationPoint() && RemainingAllocationSize();
  }
  void SetAllocationPoint(Address, size_t);

  size_t RemainingAllocationSize() const { return remaining_allocation_size_; }
  void SetRemainingAllocationSize(size_t);
  void UpdateRemainingAllocationSize();

  FreeList free_list_;
  Address current_allocation_point_;
  size_t remaining_allocation_size_;
  size_t last_remaining_allocation_size_;

  // The size of promptly freed objects in the heap.
  size_t promptly_freed_size_;

  bool is_lazy_sweeping_;
};

class LargeObjectArena final : public BaseArena {
 public:
  LargeObjectArena(ThreadState*, int);
  Address AllocateLargeObjectPage(size_t, size_t gc_info_index);
  void FreeLargeObjectPage(LargeObjectPage*);
#if DCHECK_IS_ON()
  bool IsConsistentForGC() override { return true; }
#endif
 private:
  Address DoAllocateLargeObjectPage(size_t, size_t gc_info_index);
  Address LazySweepPages(size_t, size_t gc_info_index) override;
};

// Mask an address down to the enclosing oilpan heap base page. All Oilpan heap
// pages are aligned at |blinkPageBase| plus the size of a guard size.
//
// FIXME: Remove PLATFORM_EXPORT once we get a proper public interface to our
// typed arenas. This is only exported to enable tests in HeapTest.cpp.
PLATFORM_EXPORT inline BasePage* PageFromObject(const void* object) {
  Address address = reinterpret_cast<Address>(const_cast<void*>(object));
  BasePage* page = reinterpret_cast<BasePage*>(BlinkPageAddress(address) +
                                               kBlinkGuardPageSize);
#if DCHECK_IS_ON()
  DCHECK(page->Contains(address));
#endif
  return page;
}

NO_SANITIZE_ADDRESS inline size_t HeapObjectHeader::size() const {
  size_t result = encoded_ & kHeaderSizeMask;
  // Large objects should not refer to header->size(). The actual size of a
  // large object is stored in |LargeObjectPage::m_payloadSize|.
  DCHECK(result != kLargeObjectSizeInHeader);
  DCHECK(!PageFromObject(this)->IsLargeObjectPage());
  return result;
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsValid() const {
#if defined(ARCH_CPU_64_BITS)
  return GetMagic() == magic_;
#else
  return true;
#endif
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::CheckHeader() const {
#if defined(ARCH_CPU_64_BITS)
  DCHECK(IsValid());
#endif
}

inline Address HeapObjectHeader::Payload() {
  return reinterpret_cast<Address>(this) + sizeof(HeapObjectHeader);
}

inline Address HeapObjectHeader::PayloadEnd() {
  return reinterpret_cast<Address>(this) + size();
}

NO_SANITIZE_ADDRESS inline size_t HeapObjectHeader::PayloadSize() {
  CheckHeader();
  size_t size = encoded_ & kHeaderSizeMask;
  if (UNLIKELY(size == kLargeObjectSizeInHeader)) {
    DCHECK(PageFromObject(this)->IsLargeObjectPage());
    return static_cast<LargeObjectPage*>(PageFromObject(this))->PayloadSize() -
           sizeof(HeapObjectHeader);
  }
  DCHECK(!PageFromObject(this)->IsLargeObjectPage());
  return size - sizeof(HeapObjectHeader);
}

inline HeapObjectHeader* HeapObjectHeader::FromPayload(const void* payload) {
  Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
  HeapObjectHeader* header =
      reinterpret_cast<HeapObjectHeader*>(addr - sizeof(HeapObjectHeader));
  header->CheckHeader();
  return header;
}

inline void HeapObjectHeader::CheckFromPayload(const void* payload) {
  (void)FromPayload(payload);
}

#if defined(ARCH_CPU_64_BITS)
ALWAYS_INLINE uint32_t RotateLeft16(uint32_t x) {
#if COMPILER(MSVC)
  return _lrotr(x, 16);
#else
  // http://blog.regehr.org/archives/1063
  return (x << 16) | (x >> (-16 & 31));
#endif
}

inline uint32_t HeapObjectHeader::GetMagic() const {
// Ignore C4319: It is OK to 0-extend into the high-order bits of the uintptr_t
// on 64-bit, in this case.
#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable : 4319)
#endif

  const uintptr_t random1 = ~(RotateLeft16(reinterpret_cast<uintptr_t>(
      base::trace_event::MemoryAllocatorDump::kNameSize)));

#if OS(WIN)
  const uintptr_t random2 =
      ~(RotateLeft16(reinterpret_cast<uintptr_t>(::ReadFile)));
#elif OS(POSIX)
  const uintptr_t random2 =
      ~(RotateLeft16(reinterpret_cast<uintptr_t>(::read)));
#else
#error OS not supported
#endif

#if defined(ARCH_CPU_64_BITS)
  static_assert(sizeof(uintptr_t) == sizeof(uint64_t),
                "uintptr_t is not uint64_t");
  const uint32_t random = static_cast<uint32_t>(
      (random1 & 0x0FFFFULL) | ((random2 >> 32) & 0x0FFFF0000ULL));
#elif defined(ARCH_CPU_32_BITS)
  // Although we don't use heap metadata canaries on 32-bit due to memory
  // pressure, keep this code around just in case we do, someday.
  static_assert(sizeof(uintptr_t) == sizeof(uint32_t),
                "uintptr_t is not uint32_t");
  const uint32_t random = (random1 & 0x0FFFFUL) | (random2 & 0xFFFF0000UL);
#else
#error architecture not supported
#endif

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

  return random;
}
#endif  // defined(ARCH_CPU_64_BITS)

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsWrapperHeaderMarked()
    const {
  CheckHeader();
  return encoded_ & kHeaderWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::MarkWrapperHeader() {
  CheckHeader();
  DCHECK(!IsWrapperHeaderMarked());
  encoded_ |= kHeaderWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::UnmarkWrapperHeader() {
  CheckHeader();
  DCHECK(IsWrapperHeaderMarked());
  encoded_ &= ~kHeaderWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsMarked() const {
  CheckHeader();
  return encoded_ & kHeaderMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::Mark() {
  CheckHeader();
  DCHECK(!IsMarked());
  encoded_ = encoded_ | kHeaderMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::Unmark() {
  CheckHeader();
  DCHECK(IsMarked());
  encoded_ &= ~kHeaderMarkBitMask;
}

inline Address NormalPageArena::AllocateObject(size_t allocation_size,
                                               size_t gc_info_index) {
  if (LIKELY(allocation_size <= remaining_allocation_size_)) {
    Address header_address = current_allocation_point_;
    current_allocation_point_ += allocation_size;
    remaining_allocation_size_ -= allocation_size;
    DCHECK_GT(gc_info_index, 0u);
    new (NotNull, header_address)
        HeapObjectHeader(allocation_size, gc_info_index);
    Address result = header_address + sizeof(HeapObjectHeader);
    DCHECK(!(reinterpret_cast<uintptr_t>(result) & kAllocationMask));

    SET_MEMORY_ACCESSIBLE(result, allocation_size - sizeof(HeapObjectHeader));
#if DCHECK_IS_ON()
    DCHECK(FindPageFromAddress(header_address + allocation_size - 1));
#endif
    return result;
  }
  return OutOfLineAllocate(allocation_size, gc_info_index);
}

inline NormalPageArena* NormalPage::ArenaForNormalPage() const {
  return static_cast<NormalPageArena*>(Arena());
}

}  // namespace blink

#endif  // HeapPage_h

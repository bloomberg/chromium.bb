// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SparseHeapBitmap_h
#define SparseHeapBitmap_h

#include "platform/heap/BlinkGC.h"
#include "platform/heap/HeapPage.h"
#include "wtf/Alignment.h"
#include "wtf/PtrUtil.h"
#include <bitset>
#include <memory>

namespace blink {

// A sparse bitmap of heap addresses where the (very few) addresses that are
// set are likely to be in small clusters. The abstraction is tailored to
// support heap compaction, assuming the following:
//
//   - Addresses will be bitmap-marked from lower to higher addresses.
//   - Bitmap lookups are performed for each object that is compacted
//     and moved to some new location, supplying the (base, size)
//     pair of the object's heap allocation.
//   - If the sparse bitmap has any marked addresses in that range, it
//     returns a sub-bitmap that can be quickly iterated over to check which
//     addresses within the range are actually set.
//   - The bitmap is needed to support something that is very rarely done
//     by the current Blink codebase, which is to have nested collection
//     part objects. Consequently, it is safe to assume sparseness.
//
// Support the above by having a sparse bitmap organized as a binary
// tree with nodes covering fixed size ranges via a simple bitmap/set.
// That is, each SparseHeapBitmap node will contain a bitmap/set for
// some fixed size range, along with pointers to SparseHeapBitmaps
// for addresses on each side its range.
//
// This bitmap tree isn't kept balanced across the Address additions
// made.
//
class PLATFORM_EXPORT SparseHeapBitmap {
 public:
  static std::unique_ptr<SparseHeapBitmap> create(Address base) {
    return WTF::wrapUnique(new SparseHeapBitmap(base));
  }

  ~SparseHeapBitmap() {}

  // Return the sparse bitmap subtree that at least covers the
  // [address, address + size) range, or nullptr if none.
  //
  // The returned SparseHeapBitmap can be used to quickly lookup what
  // addresses in that range are set or not; see |isSet()|. Its
  // |isSet()| behavior outside that range is not defined.
  SparseHeapBitmap* hasRange(Address, size_t);

  // True iff |address| is set for this SparseHeapBitmap tree.
  bool isSet(Address);

  // Mark |address| as present/set.
  void add(Address);

  // The assumed minimum alignment of the pointers being added. Cannot
  // exceed |log2(allocationGranularity)|; having it be equal to
  // the platform pointer alignment is what's wanted.
  static const int s_pointerAlignmentInBits = WTF_ALIGN_OF(void*) == 8 ? 3 : 2;
  static const size_t s_pointerAlignmentMask =
      (0x1u << s_pointerAlignmentInBits) - 1;

  // Represent ranges in 0x100 bitset chunks; bit I is set iff Address
  // |m_base + I * (0x1 << s_pointerAlignmentInBits)| has been added to the
  // |SparseHeapBitmap|.
  static const size_t s_bitmapChunkSize = 0x100;

  // A SparseHeapBitmap either contains a single Address or a bitmap
  // recording the mapping for [m_base, m_base + s_bitmapChunkRange)
  static const size_t s_bitmapChunkRange = s_bitmapChunkSize
                                           << s_pointerAlignmentInBits;

  // Return the number of nodes; for debug stats.
  size_t intervalCount() const;

 private:
  explicit SparseHeapBitmap(Address base) : m_base(base), m_size(1) {
    DCHECK(!(reinterpret_cast<uintptr_t>(m_base) & s_pointerAlignmentMask));
    static_assert(s_pointerAlignmentMask <= allocationMask,
                  "address shift exceeds heap pointer alignment");
    // For now, only recognize 8 and 4.
    static_assert(WTF_ALIGN_OF(void*) == 8 || WTF_ALIGN_OF(void*) == 4,
                  "unsupported pointer alignment");
  }

  Address base() const { return m_base; }
  size_t size() const { return m_size; }
  Address end() const { return base() + (m_size - 1); }

  Address maxEnd() const { return base() + s_bitmapChunkRange; }

  Address minStart() const {
    // If this bitmap node represents the sparse [m_base, s_bitmapChunkRange)
    // range, do not allow it to be "left extended" as that would entail
    // having to shift down the contents of the std::bitset somehow.
    //
    // This shouldn't be a real problem as any clusters of set addresses
    // will be marked while iterating from lower to higher addresses, hence
    // "left extension" are unlikely to be common.
    if (m_bitmap)
      return base();
    return (m_base > reinterpret_cast<Address>(s_bitmapChunkRange))
               ? (base() - s_bitmapChunkRange + 1)
               : nullptr;
  }

  Address swapBase(Address address) {
    DCHECK(!(reinterpret_cast<uintptr_t>(address) & s_pointerAlignmentMask));
    Address oldBase = m_base;
    m_base = address;
    return oldBase;
  }

  void createBitmap();

  Address m_base;
  // Either 1 or |s_bitmapChunkRange|.
  size_t m_size;

  // If non-null, contains a bitmap for addresses within [m_base, m_size)
  std::unique_ptr<std::bitset<s_bitmapChunkSize>> m_bitmap;

  std::unique_ptr<SparseHeapBitmap> m_left;
  std::unique_ptr<SparseHeapBitmap> m_right;
};

}  // namespace blink

#endif  // SparseHeapBitmap_h

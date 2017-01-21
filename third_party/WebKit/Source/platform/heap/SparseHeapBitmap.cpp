// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/SparseHeapBitmap.h"

#include "platform/heap/Heap.h"
#include "wtf/PtrUtil.h"

namespace blink {

// Return the subtree/bitmap that covers the
// [address, address + size) range.  Null if there is none.
SparseHeapBitmap* SparseHeapBitmap::hasRange(Address address, size_t size) {
  DCHECK(!(reinterpret_cast<uintptr_t>(address) & s_pointerAlignmentMask));
  SparseHeapBitmap* bitmap = this;
  while (bitmap) {
    // Interval starts after, |m_right| handles.
    if (address > bitmap->end()) {
      bitmap = bitmap->m_right.get();
      continue;
    }
    // Interval starts within, |bitmap| is included in the resulting range.
    if (address >= bitmap->base())
      break;

    Address right = address + size - 1;
    // Interval starts before, but intersects with |bitmap|'s range.
    if (right >= bitmap->base())
      break;

    // Interval is before entirely, for |m_left| to handle.
    bitmap = bitmap->m_left.get();
  }
  return bitmap;
}

bool SparseHeapBitmap::isSet(Address address) {
  DCHECK(!(reinterpret_cast<uintptr_t>(address) & s_pointerAlignmentMask));
  SparseHeapBitmap* bitmap = this;
  while (bitmap) {
    if (address > bitmap->end()) {
      bitmap = bitmap->m_right.get();
      continue;
    }
    if (address >= bitmap->base()) {
      if (bitmap->m_bitmap) {
        return bitmap->m_bitmap->test((address - bitmap->base()) >>
                                      s_pointerAlignmentInBits);
      }
      DCHECK(address == bitmap->base());
      DCHECK_EQ(bitmap->size(), 1u);
      return true;
    }
    bitmap = bitmap->m_left.get();
  }
  return false;
}

void SparseHeapBitmap::add(Address address) {
  DCHECK(!(reinterpret_cast<uintptr_t>(address) & s_pointerAlignmentMask));
  // |address| is beyond the maximum that this SparseHeapBitmap node can
  // encompass.
  if (address >= maxEnd()) {
    if (!m_right) {
      m_right = SparseHeapBitmap::create(address);
      return;
    }
    m_right->add(address);
    return;
  }
  // Same on the other side.
  if (address < minStart()) {
    if (!m_left) {
      m_left = SparseHeapBitmap::create(address);
      return;
    }
    m_left->add(address);
    return;
  }
  if (address == base())
    return;
  // |address| can be encompassed by |this| by expanding its size.
  if (address > base()) {
    if (!m_bitmap)
      createBitmap();
    m_bitmap->set((address - base()) >> s_pointerAlignmentInBits);
    return;
  }
  // Use |address| as the new base for this interval.
  Address oldBase = swapBase(address);
  createBitmap();
  m_bitmap->set((oldBase - address) >> s_pointerAlignmentInBits);
}

void SparseHeapBitmap::createBitmap() {
  DCHECK(!m_bitmap && size() == 1);
  m_bitmap = WTF::makeUnique<std::bitset<s_bitmapChunkSize>>();
  m_size = s_bitmapChunkRange;
  m_bitmap->set(0);
}

size_t SparseHeapBitmap::intervalCount() const {
  size_t count = 1;
  if (m_left)
    count += m_left->intervalCount();
  if (m_right)
    count += m_right->intervalCount();
  return count;
}

}  // namespace blink

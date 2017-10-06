// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLogicalOffset_h
#define NGLogicalOffset_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextDirection.h"

namespace blink {

struct NGLogicalSize;
struct NGPhysicalOffset;
struct NGPhysicalSize;

// NGLogicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the logical coordinate system.
struct CORE_EXPORT NGLogicalOffset {
  NGLogicalOffset() {}
  NGLogicalOffset(LayoutUnit inline_offset, LayoutUnit block_offset)
      : inline_offset(inline_offset), block_offset(block_offset) {}

  LayoutUnit inline_offset;
  LayoutUnit block_offset;

  // Converts a logical offset to a physical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // PhysicalOffset will be the physical top left point of the rectangle
  // described by offset + inner_size. Setting inner_size to 0,0 will return
  // the same point.
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  NGPhysicalOffset ConvertToPhysical(NGWritingMode,
                                     TextDirection,
                                     NGPhysicalSize outer_size,
                                     NGPhysicalSize inner_size) const;

  bool operator==(const NGLogicalOffset& other) const;
  bool operator!=(const NGLogicalOffset& other) const;

  NGLogicalOffset operator+(const NGLogicalOffset& other) const;
  NGLogicalOffset operator+(const NGLogicalSize& size) const;
  NGLogicalOffset& operator+=(const NGLogicalOffset& other);

  NGLogicalSize DistanceTo(const NGLogicalOffset& other) const;
  NGLogicalOffset RelativeTo(const NGLogicalOffset& other) const;
  NGLogicalOffset operator-(const NGLogicalOffset& other) const {
    return RelativeTo(other);
  }
  NGLogicalOffset& operator-=(const NGLogicalOffset& other);

  bool operator>(const NGLogicalOffset& other) const;
  bool operator>=(const NGLogicalOffset& other) const;

  bool operator<(const NGLogicalOffset& other) const;
  bool operator<=(const NGLogicalOffset& other) const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGLogicalOffset&);

}  // namespace blink

#endif  // NGLogicalOffset_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLogicalOffset_h
#define NGLogicalOffset_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct NGLogicalDelta;
struct NGPhysicalOffset;
struct NGPhysicalSize;

// NGLogicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the logical coordinate system.
struct CORE_EXPORT NGLogicalOffset {
  NGLogicalOffset() = default;
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
  NGPhysicalOffset ConvertToPhysical(WritingMode,
                                     TextDirection,
                                     NGPhysicalSize outer_size,
                                     NGPhysicalSize inner_size) const;

  bool operator==(const NGLogicalOffset& other) const {
    return std::tie(other.inline_offset, other.block_offset) ==
           std::tie(inline_offset, block_offset);
  }
  bool operator!=(const NGLogicalOffset& other) const {
    return !operator==(other);
  }

  NGLogicalOffset operator+(const NGLogicalOffset& other) const {
    return {inline_offset + other.inline_offset,
            block_offset + other.block_offset};
  }

  NGLogicalOffset& operator+=(const NGLogicalOffset& other) {
    *this = *this + other;
    return *this;
  }

  NGLogicalOffset& operator-=(const NGLogicalOffset& other) {
    inline_offset -= other.inline_offset;
    block_offset -= other.block_offset;
    return *this;
  }

  // We also have +, - operators for NGLogicalDelta, NGLogicalSize and
  // NGLogicalOffset defined in ng_logical_size.h

  bool operator>(const NGLogicalOffset& other) const {
    return inline_offset > other.inline_offset &&
           block_offset > other.block_offset;
  }
  bool operator>=(const NGLogicalOffset& other) const {
    return inline_offset >= other.inline_offset &&
           block_offset >= other.block_offset;
  }
  bool operator<(const NGLogicalOffset& other) const {
    return inline_offset < other.inline_offset &&
           block_offset < other.block_offset;
  }
  bool operator<=(const NGLogicalOffset& other) const {
    return inline_offset <= other.inline_offset &&
           block_offset <= other.block_offset;
  }

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGLogicalOffset&);

}  // namespace blink

#endif  // NGLogicalOffset_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLogicalRect_h
#define NGLogicalRect_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "platform/LayoutUnit.h"

namespace blink {

// NGLogicalRect is the position and size of a rect (typically a fragment)
// relative to its parent rect in the logical coordinate system.
struct CORE_EXPORT NGLogicalRect {
  NGLogicalRect() {}
  NGLogicalRect(const NGLogicalOffset& offset, const NGLogicalSize& size)
      : offset(offset), size(size) {}
  NGLogicalRect(LayoutUnit inline_offset,
                LayoutUnit block_offset,
                LayoutUnit inline_size,
                LayoutUnit block_size)
      : offset(inline_offset, block_offset), size(inline_size, block_size) {}

  bool IsEmpty() const;

  // Whether this rectangle is contained by the provided rectangle.
  bool IsContained(const NGLogicalRect& other) const;

  LayoutUnit InlineStartOffset() const { return offset.inline_offset; }
  LayoutUnit InlineEndOffset() const {
    return offset.inline_offset + size.inline_size;
  }
  LayoutUnit BlockStartOffset() const { return offset.block_offset; }
  LayoutUnit BlockEndOffset() const {
    return offset.block_offset + size.block_size;
  }

  NGLogicalOffset InlineStartBlockStartOffset() const {
    return {InlineStartOffset(), BlockStartOffset()};
  }

  NGLogicalOffset InlineEndBlockStartOffset() const {
    return {InlineEndOffset(), BlockStartOffset()};
  }

  LayoutUnit BlockSize() const { return size.block_size; }
  LayoutUnit InlineSize() const { return size.inline_size; }

  String ToString() const;
  bool operator==(const NGLogicalRect& other) const;

  NGLogicalOffset offset;
  NGLogicalSize size;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGLogicalRect&);

}  // namespace blink

#endif  // NGLogicalRect_h

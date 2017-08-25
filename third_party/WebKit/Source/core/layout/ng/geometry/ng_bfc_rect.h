// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBfcRect_h
#define NGBfcRect_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_bfc_offset.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "platform/LayoutUnit.h"

namespace blink {

// NGBfcRect is the position and size of a rect (typically a fragment)
// relative to a block formatting context.
struct CORE_EXPORT NGBfcRect {
  NGBfcRect() {}
  NGBfcRect(const NGBfcOffset& offset, const NGLogicalSize& size)
      : offset(offset), size(size) {}

  bool IsEmpty() const;

  // Whether this rectangle is contained by the provided rectangle.
  bool IsContained(const NGBfcRect& other) const;

  LayoutUnit LineStartOffset() const { return offset.line_offset; }
  LayoutUnit LineEndOffset() const {
    return offset.line_offset + size.inline_size;
  }
  LayoutUnit BlockStartOffset() const { return offset.block_offset; }
  LayoutUnit BlockEndOffset() const {
    return offset.block_offset + size.block_size;
  }

  NGBfcOffset LineEndBlockStartOffset() const {
    return {LineEndOffset(), BlockStartOffset()};
  }

  LayoutUnit BlockSize() const { return size.block_size; }
  LayoutUnit InlineSize() const { return size.inline_size; }

  String ToString() const;
  bool operator==(const NGBfcRect& other) const;
  bool operator!=(const NGBfcRect& other) const { return !(*this == other); }

  NGBfcOffset offset;
  NGLogicalSize size;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGBfcRect&);

}  // namespace blink

#endif  // NGBfcRect_h

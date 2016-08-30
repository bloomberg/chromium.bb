// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGUnits_h
#define NGUnits_h

#include "platform/LayoutUnit.h"

namespace blink {

class LayoutUnit;

struct NGLogicalSize {
  NGLogicalSize() {}
  NGLogicalSize(LayoutUnit inline_size, LayoutUnit block_size)
      : inline_size(inline_size), block_size(block_size) {}

  LayoutUnit inline_size;
  LayoutUnit block_size;
};

struct NGLogicalOffset {
  LayoutUnit inline_offset;
  LayoutUnit block_offset;
};

struct NGPhysicalSize {
  NGPhysicalSize() {}
  NGPhysicalSize(LayoutUnit width, LayoutUnit height)
      : width(width), height(height) {}

  LayoutUnit width;
  LayoutUnit height;
};

struct NGPhysicalLocation {
  LayoutUnit top;
  LayoutUnit left;
};

struct NGPhysicalRect {
  NGPhysicalSize size;
  NGPhysicalLocation location;
};

struct NGPixelSnappedPhysicalRect {
  int top;
  int left;
  int width;
  int height;
};

// This struct is used for storing margins, borders or padding of a box on all
// four edges.
struct NGBoxStrut {
  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return block_start + block_end; }

  // The following two operators exist primarily to have an easy way to access
  // the sum of border and padding.
  NGBoxStrut& operator+=(const NGBoxStrut& other) {
    inline_start += other.inline_start;
    inline_end += other.inline_end;
    block_start += other.block_start;
    block_end += other.block_end;
    return *this;
  }

  NGBoxStrut operator+(const NGBoxStrut& other) {
    NGBoxStrut result(*this);
    result += other;
    return result;
  }
};

}  // namespace blink

#endif  // NGUnits_h

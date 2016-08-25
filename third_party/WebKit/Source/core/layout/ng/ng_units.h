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

}  // namespace blink

#endif  // NGUnits_h

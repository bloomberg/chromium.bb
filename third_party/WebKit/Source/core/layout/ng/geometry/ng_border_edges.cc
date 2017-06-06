// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_border_edges.h"

namespace blink {

NGBorderEdges NGBorderEdges::FromPhysical(unsigned physical_edges,
                                          NGWritingMode writing_mode) {
  if (writing_mode == kHorizontalTopBottom) {
    return NGBorderEdges(physical_edges & kTop, physical_edges & kRight,
                         physical_edges & kBottom, physical_edges & kLeft);
  }
  if (writing_mode != kSidewaysLeftRight) {
    return NGBorderEdges(physical_edges & kRight, physical_edges & kBottom,
                         physical_edges & kLeft, physical_edges & kTop);
  }
  return NGBorderEdges(physical_edges & kLeft, physical_edges & kTop,
                       physical_edges & kRight, physical_edges & kBottom);
}

unsigned NGBorderEdges::ToPhysical(NGWritingMode writing_mode) const {
  if (writing_mode == kHorizontalTopBottom) {
    return (block_start ? kTop : 0) | (line_right ? kRight : 0) |
           (block_end ? kBottom : 0) | (line_left ? kLeft : 0);
  }
  if (writing_mode != kSidewaysLeftRight) {
    return (block_start ? kRight : 0) | (line_right ? kBottom : 0) |
           (block_end ? kLeft : 0) | (line_left ? kTop : 0);
  }
  return (block_start ? kLeft : 0) | (line_right ? kTop : 0) |
         (block_end ? kRight : 0) | (line_left ? kBottom : 0);
}

}  // namespace blink

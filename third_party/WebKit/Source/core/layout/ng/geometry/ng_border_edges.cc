// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_border_edges.h"

namespace blink {

namespace NGBorderEdges {

Physical ToPhysical(Logical logical_edges, NGWritingMode writing_mode) {
  static_assert(kBlockStart == static_cast<Logical>(kTop) &&
                    kBlockEnd == static_cast<Logical>(kBottom) &&
                    kLineLeft == static_cast<Logical>(kLeft) &&
                    kLineRight == static_cast<Logical>(kRight),
                "Physical and Logical must match");

  if (writing_mode == kHorizontalTopBottom || logical_edges == kAll)
    return static_cast<Physical>(logical_edges);

  if (writing_mode != kSidewaysLeftRight) {
    return static_cast<Physical>((logical_edges & kBlockStart ? kRight : 0) |
                                 (logical_edges & kBlockEnd ? kLeft : 0) |
                                 (logical_edges & kLineLeft ? kTop : 0) |
                                 (logical_edges & kLineRight ? kBottom : 0));
  }
  return static_cast<Physical>((logical_edges & kBlockStart ? kLeft : 0) |
                               (logical_edges & kBlockEnd ? kRight : 0) |
                               (logical_edges & kLineLeft ? kBottom : 0) |
                               (logical_edges & kLineRight ? kTop : 0));
}

Logical ToLogical(Physical physical_edges, NGWritingMode writing_mode) {
  if (writing_mode == kHorizontalTopBottom ||
      physical_edges == static_cast<Physical>(kAll))
    return static_cast<Logical>(physical_edges);

  if (writing_mode != kSidewaysLeftRight) {
    return static_cast<Logical>((physical_edges & kTop ? kLineLeft : 0) |
                                (physical_edges & kBottom ? kLineRight : 0) |
                                (physical_edges & kLeft ? kBlockEnd : 0) |
                                (physical_edges & kRight ? kBlockStart : 0));
  }
  return static_cast<Logical>((physical_edges & kTop ? kLineRight : 0) |
                              (physical_edges & kBottom ? kLineLeft : 0) |
                              (physical_edges & kLeft ? kBlockStart : 0) |
                              (physical_edges & kRight ? kBlockEnd : 0));
}

}  // namespace NGBorderEdges

}  // namespace blink

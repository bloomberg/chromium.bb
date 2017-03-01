// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_static_position.h"

namespace blink {

NGStaticPosition NGStaticPosition::Create(NGWritingMode writing_mode,
                                          TextDirection direction,
                                          NGPhysicalOffset offset) {
  NGStaticPosition position;
  position.offset = offset;
  switch (writing_mode) {
    case kHorizontalTopBottom:
      position.type = (direction == TextDirection::kLtr) ? kTopLeft : kTopRight;
      break;
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      position.type =
          (direction == TextDirection::kLtr) ? kTopRight : kBottomRight;
      break;
    case kVerticalLeftRight:
      position.type =
          (direction == TextDirection::kLtr) ? kTopLeft : kBottomLeft;
      break;
    case kSidewaysLeftRight:
      position.type =
          (direction == TextDirection::kLtr) ? kBottomLeft : kTopLeft;
      break;
  }
  return position;
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGStaticPosition_h
#define NGStaticPosition_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextDirection.h"

namespace blink {

// Represents static position of an out of flow descendant.
struct CORE_EXPORT NGStaticPosition {
  enum Type { kTopLeft, kTopRight, kBottomLeft, kBottomRight };

  Type type;  // Logical corner that corresponds to physical top left.
  NGPhysicalOffset offset;

  // Creates a position with proper type wrt writing mode and direction.
  static NGStaticPosition Create(NGWritingMode,
                                 TextDirection,
                                 NGPhysicalOffset);
  // Left/Right/TopPosition functions map static position to
  // left/right/top edge wrt container space.
  // The function arguments are required to solve the equation:
  // contaner_size = left + margin_left + width + margin_right + right
  LayoutUnit LeftPosition(LayoutUnit container_size,
                          LayoutUnit width,
                          LayoutUnit margin_left,
                          LayoutUnit margin_right) const {
    return GenericPosition(HasLeft(), offset.left, container_size, width,
                           margin_left, margin_right);
  }
  LayoutUnit RightPosition(LayoutUnit container_size,
                           LayoutUnit width,
                           LayoutUnit margin_left,
                           LayoutUnit margin_right) const {
    return GenericPosition(!HasLeft(), offset.left, container_size, width,
                           margin_left, margin_right);
  }
  LayoutUnit TopPosition(LayoutUnit container_size,
                         LayoutUnit height,
                         LayoutUnit margin_top,
                         LayoutUnit margin_bottom) const {
    return GenericPosition(HasTop(), offset.top, container_size, height,
                           margin_top, margin_bottom);
  }

 private:
  bool HasTop() const { return type == kTopLeft || type == kTopRight; }
  bool HasLeft() const { return type == kTopLeft || type == kBottomLeft; }
  LayoutUnit GenericPosition(bool position_matches,
                             LayoutUnit position,
                             LayoutUnit container_size,
                             LayoutUnit length,
                             LayoutUnit margin_start,
                             LayoutUnit margin_end) const {
    DCHECK_GE(container_size, LayoutUnit());
    DCHECK_GE(length, LayoutUnit());
    if (position_matches)
      return position;
    else
      return container_size - position - length - margin_start - margin_end;
  }
};

}  // namespace blink

#endif  // NGStaticPosition_h

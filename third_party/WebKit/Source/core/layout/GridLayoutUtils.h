// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridLayoutUtils_h
#define GridLayoutUtils_h

#include "core/layout/LayoutGrid.h"
#include "platform/LayoutUnit.h"

namespace blink {

class GridLayoutUtils {
 public:
  static LayoutUnit MarginLogicalWidthForChild(const LayoutGrid&,
                                               const LayoutBox&);
  static LayoutUnit MarginLogicalHeightForChild(const LayoutGrid&,
                                                const LayoutBox&);
  static bool IsOrthogonalChild(const LayoutGrid&, const LayoutBox&);
  static GridTrackSizingDirection FlowAwareDirectionForChild(
      const LayoutGrid&,
      const LayoutBox&,
      GridTrackSizingDirection);
  static bool HasOverrideContainingBlockContentSizeForChild(
      const LayoutBox&,
      GridTrackSizingDirection);
  static LayoutUnit OverrideContainingBlockContentSizeForChild(
      const LayoutBox&,
      GridTrackSizingDirection);
};
}  // namespace blink

#endif  // GridLayoutUtils_h

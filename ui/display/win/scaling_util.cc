// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/scaling_util.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/range/range.h"

namespace {

bool InRange(int target, int lower_bound, int upper_bound) {
  return lower_bound <= target && target <= upper_bound;
}

// Scaled |unscaled_offset| to the same relative position on |unscaled_length|
// based off of |unscaled_length|'s |scale_factor|.
int ScaleOffset(int unscaled_length, float scale_factor, int unscaled_offset) {
  float scaled_length = static_cast<float>(unscaled_length) / scale_factor;
  float percent =
      static_cast<float>(unscaled_offset) / static_cast<float>(unscaled_length);
  return gfx::ToFlooredInt(scaled_length * percent);
}

}  // namespace

namespace display {
namespace win {

bool DisplayInfosTouch(const DisplayInfo& a, const DisplayInfo& b) {
  const gfx::Rect& a_rect = a.screen_rect();
  const gfx::Rect& b_rect = b.screen_rect();
  int max_left = std::max(a_rect.x(), b_rect.x());
  int max_top = std::max(a_rect.y(), b_rect.y());
  int min_right = std::min(a_rect.right(), b_rect.right());
  int min_bottom = std::min(a_rect.bottom(), b_rect.bottom());
  return (max_left == min_right &&
             a_rect.y() <= b_rect.bottom() &&
             b_rect.y() <= a_rect.bottom()) ||
         (max_top == min_bottom &&
             a_rect.x() <= b_rect.right() &&
             b_rect.x() <= a_rect.right());
}

DisplayPlacement::Position CalculateDisplayPosition(
    const DisplayInfo& parent,
    const DisplayInfo& current) {
  const gfx::Rect& parent_rect = parent.screen_rect();
  const gfx::Rect& current_rect = current.screen_rect();
  int max_left = std::max(parent_rect.x(), current_rect.x());
  int max_top = std::max(parent_rect.y(), current_rect.y());
  int min_right = std::min(parent_rect.right(), current_rect.right());
  int min_bottom = std::min(parent_rect.bottom(), current_rect.bottom());
  if (max_left == min_right && max_top == min_bottom) {
    // Corner touching.
    if (parent_rect.bottom() == max_top)
      return DisplayPlacement::Position::BOTTOM;
    if (parent_rect.x() == max_left)
      return DisplayPlacement::Position::LEFT;

    return DisplayPlacement::Position::TOP;
  }
  if (max_left == min_right &&
      parent_rect.y() <= current_rect.bottom() &&
      current_rect.y() <= parent_rect.bottom()) {
    // Vertical edge touching.
    return parent_rect.x() == max_left
        ? DisplayPlacement::Position::LEFT
        : DisplayPlacement::Position::RIGHT;
  }
  if (max_top == min_bottom &&
      parent_rect.x() <= current_rect.right() &&
      current_rect.x() <= parent_rect.right()) {
    // Horizontal edge touching.
    return parent_rect.y() == max_top
        ? DisplayPlacement::Position::TOP
        : DisplayPlacement::Position::BOTTOM;
  }
  NOTREACHED() << "CalculateDisplayPosition relies on touching DisplayInfos.";
  return DisplayPlacement::Position::RIGHT;
}

DisplayPlacement CalculateDisplayPlacement(const DisplayInfo& parent,
                                           const DisplayInfo& current) {
  DCHECK(DisplayInfosTouch(parent, current)) << "DisplayInfos must touch.";

  DisplayPlacement placement;
  placement.parent_display_id = parent.id();
  placement.display_id = current.id();
  placement.position = CalculateDisplayPosition(parent, current);

  int parent_begin = 0;
  int parent_end = 0;
  int current_begin = 0;
  int current_end = 0;

  switch (placement.position) {
    case DisplayPlacement::Position::TOP:
    case DisplayPlacement::Position::BOTTOM:
      parent_begin = parent.screen_rect().x();
      parent_end = parent.screen_rect().right();
      current_begin = current.screen_rect().x();
      current_end = current.screen_rect().right();
      break;
    case DisplayPlacement::Position::LEFT:
    case DisplayPlacement::Position::RIGHT:
      parent_begin = parent.screen_rect().y();
      parent_end = parent.screen_rect().bottom();
      current_begin = current.screen_rect().y();
      current_end = current.screen_rect().bottom();
      break;
  }

  // Since we're talking offsets, make everything relative to parent_begin.
  parent_end -= parent_begin;
  current_begin -= parent_begin;
  current_end -= parent_begin;
  parent_begin = 0;

  // There are a few ways lines can intersect:
  // End Aligned
  // CURRENT's offset is relative to the end (in our world, BOTTOM_RIGHT).
  //                 +-PARENT----------------+
  //                    +-CURRENT------------+
  //
  // Positioning based off of |current_begin|.
  // CURRENT's offset is simply a percentage of its position on PARENT.
  //                 +-PARENT----------------+
  //                        +-CURRENT------------+
  //
  // Positioning based off of |current_end|.
  // CURRENT's offset is dependent on the percentage of its end position on
  // PARENT.
  //                 +-PARENT----------------+
  //           +-CURRENT------------+
  //
  // Positioning based off of |parent_begin| on current.
  // CURRENT's offset is dependent on the percentage of its end position on
  // PARENT.
  //                 +-PARENT----------------+
  //           +-CURRENT--------------------------+
  if (parent_end == current_end) {
    // End aligned.
    placement.offset_reference =
        DisplayPlacement::OffsetReference::BOTTOM_RIGHT;
    placement.offset = 0;
  } else if (InRange(current_begin, parent_begin, parent_end)) {
    placement.offset = ScaleOffset(parent_end,
                                   parent.device_scale_factor(),
                                   current_begin);
  } else if (InRange(current_end, parent_begin, parent_end)) {
    placement.offset_reference =
        DisplayPlacement::OffsetReference::BOTTOM_RIGHT;
    placement.offset = ScaleOffset(parent_end,
                                   parent.device_scale_factor(),
                                   parent_end - current_end);
  } else {
    DCHECK(InRange(parent_begin, current_begin, current_end));
    placement.offset = ScaleOffset(current_end - current_begin,
                                   current.device_scale_factor(),
                                   current_begin);
  }

  return placement;
}

}  // namespace win
}  // namespace display

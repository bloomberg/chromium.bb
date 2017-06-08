// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/StickyPositionScrollingConstraints.h"
#include "core/paint/PaintLayer.h"

namespace blink {

FloatSize StickyPositionScrollingConstraints::ComputeStickyOffset(
    const FloatRect& viewport_rect,
    const StickyPositionScrollingConstraints* ancestor_sticky_box_constraints,
    const StickyPositionScrollingConstraints*
        ancestor_containing_block_constraints) {
  // Adjust the constraint rect locations based on our ancestor sticky elements
  // These adjustments are necessary to avoid double offsetting in the case of
  // nested sticky elements.
  FloatSize ancestor_sticky_box_offset =
      ancestor_sticky_box_constraints
          ? ancestor_sticky_box_constraints->GetTotalStickyBoxStickyOffset()
          : FloatSize();
  FloatSize ancestor_containing_block_offset =
      ancestor_containing_block_constraints
          ? ancestor_containing_block_constraints
                ->GetTotalContainingBlockStickyOffset()
          : FloatSize();
  FloatRect sticky_box_rect = scroll_container_relative_sticky_box_rect_;
  FloatRect containing_block_rect =
      scroll_container_relative_containing_block_rect_;
  sticky_box_rect.Move(ancestor_sticky_box_offset +
                       ancestor_containing_block_offset);
  containing_block_rect.Move(ancestor_containing_block_offset);

  FloatRect box_rect = sticky_box_rect;

  if (HasAnchorEdge(kAnchorEdgeRight)) {
    float right_limit = viewport_rect.MaxX() - right_offset_;
    float right_delta =
        std::min<float>(0, right_limit - sticky_box_rect.MaxX());
    float available_space =
        std::min<float>(0, containing_block_rect.X() - sticky_box_rect.X());
    if (right_delta < available_space)
      right_delta = available_space;

    box_rect.Move(right_delta, 0);
  }

  if (HasAnchorEdge(kAnchorEdgeLeft)) {
    float left_limit = viewport_rect.X() + left_offset_;
    float left_delta = std::max<float>(0, left_limit - sticky_box_rect.X());
    float available_space = std::max<float>(
        0, containing_block_rect.MaxX() - sticky_box_rect.MaxX());
    if (left_delta > available_space)
      left_delta = available_space;

    box_rect.Move(left_delta, 0);
  }

  if (HasAnchorEdge(kAnchorEdgeBottom)) {
    float bottom_limit = viewport_rect.MaxY() - bottom_offset_;
    float bottom_delta =
        std::min<float>(0, bottom_limit - sticky_box_rect.MaxY());
    float available_space =
        std::min<float>(0, containing_block_rect.Y() - sticky_box_rect.Y());
    if (bottom_delta < available_space)
      bottom_delta = available_space;

    box_rect.Move(0, bottom_delta);
  }

  if (HasAnchorEdge(kAnchorEdgeTop)) {
    float top_limit = viewport_rect.Y() + top_offset_;
    float top_delta = std::max<float>(0, top_limit - sticky_box_rect.Y());
    float available_space = std::max<float>(
        0, containing_block_rect.MaxY() - sticky_box_rect.MaxY());
    if (top_delta > available_space)
      top_delta = available_space;

    box_rect.Move(0, top_delta);
  }

  FloatSize sticky_offset = box_rect.Location() - sticky_box_rect.Location();

  total_sticky_box_sticky_offset_ = ancestor_sticky_box_offset + sticky_offset;
  total_containing_block_sticky_offset_ = ancestor_sticky_box_offset +
                                          ancestor_containing_block_offset +
                                          sticky_offset;

  return sticky_offset;
}

FloatSize StickyPositionScrollingConstraints::GetOffsetForStickyPosition(
    const StickyConstraintsMap& constraints_map) const {
  FloatSize nearest_sticky_box_shifting_sticky_box_constraints_offset;
  if (nearest_sticky_box_shifting_sticky_box_) {
    nearest_sticky_box_shifting_sticky_box_constraints_offset =
        constraints_map.at(nearest_sticky_box_shifting_sticky_box_->Layer())
            .GetTotalStickyBoxStickyOffset();
  }
  return total_sticky_box_sticky_offset_ -
         nearest_sticky_box_shifting_sticky_box_constraints_offset;
}

}  // namespace blink

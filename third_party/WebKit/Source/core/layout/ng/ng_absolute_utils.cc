// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_absolute_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

namespace {

// Extends Length::valueForLength with default value if length type is auto.
LayoutUnit CustomValueForLength(const Length& length,
                                LayoutUnit max_value,
                                LayoutUnit default_value = NGSizeIndefinite) {
  if (length.isAuto())
    return default_value;
  DCHECK(!length.isPercentOrCalc() || max_value != NGSizeIndefinite);
  return valueForLength(length, max_value);
}

bool AbsoluteHorizontalNeedsEstimate(const ComputedStyle& style) {
  return style.width().isAuto() &&
         (style.left().isAuto() || style.right().isAuto());
}

bool AbsoluteVerticalNeedsEstimate(const ComputedStyle& style) {
  return style.height().isAuto() &&
         (style.top().isAuto() || style.bottom().isAuto());
}

// Implement absolute horizontal size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
void ComputeAbsoluteHorizontal(const NGConstraintSpace& space,
                               const ComputedStyle& style,
                               const NGStaticPosition& static_position,
                               const Optional<LayoutUnit>& child_auto_width,
                               NGAbsolutePhysicalPosition* position) {
  // Always use inline_size for percent resolution.
  // TODO(atotic) test whether inline_size is correct in vertical modes.
  LayoutUnit percentage_resolution_size =
      space.PercentageResolutionSize().inline_size;
  LayoutUnit border_left(style.borderLeftWidth());
  LayoutUnit border_right(style.borderRightWidth());
  LayoutUnit padding_left = CustomValueForLength(
      style.paddingLeft(), percentage_resolution_size, LayoutUnit());
  LayoutUnit padding_right = CustomValueForLength(
      style.paddingRight(), percentage_resolution_size, LayoutUnit());
  LayoutUnit margin_left =
      CustomValueForLength(style.marginLeft(), percentage_resolution_size);
  LayoutUnit margin_right =
      CustomValueForLength(style.marginRight(), percentage_resolution_size);
  LayoutUnit left =
      CustomValueForLength(style.left(), percentage_resolution_size);
  LayoutUnit right =
      CustomValueForLength(style.right(), percentage_resolution_size);
  LayoutUnit border_padding =
      border_left + border_right + padding_left + padding_right;
  // TODO(atotic): This should use Resolve{Inline,Block}Length
  LayoutUnit width =
      CustomValueForLength(style.width(), percentage_resolution_size);

  // Normalize width to border-box sizing.
  if (style.boxSizing() == EBoxSizing::BoxSizingContentBox &&
      width != NGSizeIndefinite)
    width += border_padding;

  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.WritingMode());
  DCHECK(container_size.width != NGSizeIndefinite);

  // Solving the equation:
  // left + marginLeft + width + marginRight + right  = container width
  if (left == NGSizeIndefinite && right == NGSizeIndefinite &&
      width == NGSizeIndefinite) {
    // Standard: "If all three of left, width, and right are auto:"
    if (margin_left == NGSizeIndefinite)
      margin_left = LayoutUnit();
    if (margin_right == NGSizeIndefinite)
      margin_right = LayoutUnit();
    DCHECK(child_auto_width.has_value());
    width = *child_auto_width;
    if (space.Direction() == TextDirection::Ltr) {
      left = static_position.LeftPosition(container_size.width, width,
                                          margin_left, margin_right);
    } else {
      right = static_position.RightPosition(container_size.width, width,
                                            margin_left, margin_right);
    }
  } else if (left != NGSizeIndefinite && right != NGSizeIndefinite &&
             width != NGSizeIndefinite) {
    // Standard: "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.width - left - right - width;
    // When both margins are auto.
    if (margin_left == NGSizeIndefinite && margin_right == NGSizeIndefinite) {
      if (margin_space > 0) {
        margin_left = margin_space / 2;
        margin_right = margin_space / 2;
      } else {
        // Margins are negative.
        if (space.Direction() == TextDirection::Ltr) {
          margin_left = LayoutUnit();
          margin_right = margin_space;
        } else {
          margin_right = LayoutUnit();
          margin_left = margin_space;
        }
      }
    } else if (margin_left == NGSizeIndefinite) {
      margin_left = margin_space;
    } else if (margin_right == NGSizeIndefinite) {
      margin_right = margin_space;
    } else {
      // Are values overconstrained?
      if (margin_left + margin_right != margin_space) {
        // Relax the end.
        if (space.Direction() == TextDirection::Ltr)
          right -= margin_left + margin_right - margin_space;
        else
          left -= margin_left + margin_right - margin_space;
      }
    }
  }

  // Set unknown margins.
  if (margin_left == NGSizeIndefinite)
    margin_left = LayoutUnit();
  if (margin_right == NGSizeIndefinite)
    margin_right = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown.
  if (left == NGSizeIndefinite && width == NGSizeIndefinite) {
    // Rule 1: left/width are unknown.
    DCHECK_NE(right, NGSizeIndefinite);
    DCHECK(child_auto_width.has_value());
    width = *child_auto_width;
  } else if (left == NGSizeIndefinite && right == NGSizeIndefinite) {
    // Rule 2.
    DCHECK_NE(width, NGSizeIndefinite);
    if (space.Direction() == TextDirection::Ltr)
      left = static_position.LeftPosition(container_size.width, width,
                                          margin_left, margin_right);
    else
      right = static_position.RightPosition(container_size.width, width,
                                            margin_left, margin_right);
  } else if (width == NGSizeIndefinite && right == NGSizeIndefinite) {
    // Rule 3.
    DCHECK(child_auto_width.has_value());
    width = *child_auto_width;
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (left == NGSizeIndefinite) {
    left = container_size.width - right - width - margin_left - margin_right;
  } else if (right == NGSizeIndefinite) {
    right = container_size.width - left - width - margin_left - margin_right;
  } else if (width == NGSizeIndefinite) {
    width = container_size.width - left - right - margin_left - margin_right;
  }
  DCHECK_EQ(container_size.width,
            left + right + margin_left + margin_right + width);

  // Negative widths are not allowed.
  width = std::max(width, border_padding);

  position->inset.left = left + margin_left;
  position->inset.right = right + margin_right;
  position->size.width = width;
}

// Implements absolute vertical size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
void ComputeAbsoluteVertical(const NGConstraintSpace& space,
                             const ComputedStyle& style,
                             const NGStaticPosition& static_position,
                             const Optional<LayoutUnit>& child_auto_height,
                             NGAbsolutePhysicalPosition* position) {
  // TODO(atotic) check percentage resolution for vertical writing modes.
  LayoutUnit percentage_inline_size =
      space.PercentageResolutionSize().inline_size;
  LayoutUnit percentage_block_size =
      space.PercentageResolutionSize().block_size;
  LayoutUnit border_top(style.borderTopWidth());
  LayoutUnit border_bottom(style.borderBottomWidth());
  LayoutUnit padding_top = CustomValueForLength(
      style.paddingTop(), percentage_inline_size, LayoutUnit());
  LayoutUnit padding_bottom = CustomValueForLength(
      style.paddingBottom(), percentage_inline_size, LayoutUnit());
  LayoutUnit margin_top =
      CustomValueForLength(style.marginTop(), percentage_inline_size);
  LayoutUnit margin_bottom =
      CustomValueForLength(style.marginBottom(), percentage_inline_size);
  LayoutUnit top = CustomValueForLength(style.top(), percentage_block_size);
  LayoutUnit bottom =
      CustomValueForLength(style.bottom(), percentage_block_size);
  LayoutUnit border_padding =
      border_top + border_bottom + padding_top + padding_bottom;
  // TODO(atotic): This should use Resolve{Inline,Block}Length.
  LayoutUnit height =
      CustomValueForLength(style.height(), percentage_block_size);

  if (style.boxSizing() == EBoxSizing::BoxSizingContentBox &&
      height != NGSizeIndefinite)
    height += border_padding;

  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.WritingMode());
  DCHECK(container_size.height != NGSizeIndefinite);

  // Solving the equation:
  // top + marginTop + height + marginBottom + bottom
  // + border_padding = container height
  if (top == NGSizeIndefinite && bottom == NGSizeIndefinite &&
      height == NGSizeIndefinite) {
    // Standard: "If all three of top, height, and bottom are auto:"
    if (margin_top == NGSizeIndefinite)
      margin_top = LayoutUnit();
    if (margin_bottom == NGSizeIndefinite)
      margin_bottom = LayoutUnit();
    DCHECK(child_auto_height.has_value());
    height = *child_auto_height;
    top = static_position.TopPosition(container_size.height, height, margin_top,
                                      margin_bottom);
  } else if (top != NGSizeIndefinite && bottom != NGSizeIndefinite &&
             height != NGSizeIndefinite) {
    // Standard: "If top, bottom, and height are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.height - top - bottom - height;
    // When both margins are auto.
    if (margin_top == NGSizeIndefinite && margin_bottom == NGSizeIndefinite) {
      if (margin_space > 0) {
        margin_top = margin_space / 2;
        margin_bottom = margin_space / 2;
      } else {
        // Margin space is over-constrained.
        margin_top = LayoutUnit();
        margin_bottom = margin_space;
      }
    } else if (margin_top == NGSizeIndefinite) {
      margin_top = margin_space;
    } else if (margin_bottom == NGSizeIndefinite) {
      margin_bottom = margin_space;
    } else {
      // Are values overconstrained?
      if (margin_top + margin_bottom != margin_space) {
        // Relax the end.
        bottom -= margin_top + margin_bottom - margin_space;
      }
    }
  }

  // Set unknown margins.
  if (margin_top == NGSizeIndefinite)
    margin_top = LayoutUnit();
  if (margin_bottom == NGSizeIndefinite)
    margin_bottom = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown, fix 1.
  if (top == NGSizeIndefinite && height == NGSizeIndefinite) {
    // Rule 1.
    DCHECK_NE(bottom, NGSizeIndefinite);
    DCHECK(child_auto_height.has_value());
    height = *child_auto_height;
  } else if (top == NGSizeIndefinite && bottom == NGSizeIndefinite) {
    // Rule 2.
    DCHECK_NE(height, NGSizeIndefinite);
    top = static_position.TopPosition(container_size.height, height, margin_top,
                                      margin_bottom);
  } else if (height == NGSizeIndefinite && bottom == NGSizeIndefinite) {
    // Rule 3.
    DCHECK(child_auto_height.has_value());
    height = *child_auto_height;
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (top == NGSizeIndefinite) {
    top = container_size.height - bottom - height - margin_top - margin_bottom;
  } else if (bottom == NGSizeIndefinite) {
    bottom = container_size.height - top - height - margin_top - margin_bottom;
  } else if (height == NGSizeIndefinite) {
    height = container_size.height - top - bottom - margin_top - margin_bottom;
  }
  DCHECK_EQ(container_size.height,
            top + bottom + margin_top + margin_bottom + height);

  // Negative heights are not allowed.
  height = std::max(height, border_padding);

  position->inset.top = top + margin_top;
  position->inset.bottom = bottom + margin_bottom;
  position->size.height = height;
}

}  // namespace

String NGAbsolutePhysicalPosition::ToString() const {
  return String::format("INSET(LRTB):%d,%d,%d,%d SIZE:%dx%d",
                        inset.left.toInt(), inset.right.toInt(),
                        inset.top.toInt(), inset.bottom.toInt(),
                        size.width.toInt(), size.height.toInt());
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  if (style.isHorizontalWritingMode())
    return AbsoluteVerticalNeedsEstimate(style);
  else
    return AbsoluteHorizontalNeedsEstimate(style);
}

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  if (style.isHorizontalWritingMode())
    return AbsoluteHorizontalNeedsEstimate(style);
  else
    return AbsoluteVerticalNeedsEstimate(style);
}

NGAbsolutePhysicalPosition ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<LayoutUnit>& child_inline_size) {
  NGAbsolutePhysicalPosition position;
  if (style.isHorizontalWritingMode())
    ComputeAbsoluteHorizontal(space, style, static_position, child_inline_size,
                              &position);
  else
    ComputeAbsoluteVertical(space, style, static_position, child_inline_size,
                            &position);
  return position;
}

void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<LayoutUnit>& child_block_size,
    NGAbsolutePhysicalPosition* position) {
  if (style.isHorizontalWritingMode())
    ComputeAbsoluteVertical(space, style, static_position, child_block_size,
                            position);
  else
    ComputeAbsoluteHorizontal(space, style, static_position, child_block_size,
                              position);
}

}  // namespace blink

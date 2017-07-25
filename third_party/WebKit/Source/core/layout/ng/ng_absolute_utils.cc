// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_absolute_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

namespace {

bool AbsoluteHorizontalNeedsEstimate(const ComputedStyle& style) {
  Length width = style.Width();
  return width.IsIntrinsic() ||
         (width.IsAuto() && (style.Left().IsAuto() || style.Right().IsAuto()));
}

bool AbsoluteVerticalNeedsEstimate(const ComputedStyle& style) {
  Length height = style.Height();
  return height.IsIntrinsic() ||
         (height.IsAuto() && (style.Top().IsAuto() || style.Bottom().IsAuto()));
}

LayoutUnit ResolveWidth(const Length& width,
                        const NGConstraintSpace& space,
                        const ComputedStyle& style,
                        const Optional<MinMaxSize>& child_minmax,
                        LengthResolveType resolve_type) {
  if (space.WritingMode() == kHorizontalTopBottom)
    return ResolveInlineLength(space, style, child_minmax, width, resolve_type);
  LayoutUnit computed_width =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveBlockLength(space, style, style.Width(), computed_width,
                            resolve_type);
}

LayoutUnit ResolveHeight(const Length& height,
                         const NGConstraintSpace& space,
                         const ComputedStyle& style,
                         const Optional<MinMaxSize>& child_minmax,
                         LengthResolveType resolve_type) {
  if (space.WritingMode() != kHorizontalTopBottom)
    return ResolveInlineLength(space, style, child_minmax, height,
                               resolve_type);
  LayoutUnit computed_height =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveBlockLength(space, style, height, computed_height,
                            resolve_type);
}

// Implement absolute horizontal size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
void ComputeAbsoluteHorizontal(const NGConstraintSpace& space,
                               const ComputedStyle& style,
                               const Optional<LayoutUnit>& incoming_width,
                               const NGStaticPosition& static_position,
                               const Optional<MinMaxSize>& child_minmax,
                               NGAbsolutePhysicalPosition* position) {
  NGLogicalSize percentage_logical = space.PercentageResolutionSize();
  NGPhysicalSize percentage_physical =
      percentage_logical.ConvertToPhysical(space.WritingMode());
  LayoutUnit border_left(style.BorderLeftWidth());
  LayoutUnit border_right(style.BorderRightWidth());
  LayoutUnit padding_left =
      ValueForLength(style.PaddingLeft(), percentage_logical.inline_size);
  LayoutUnit padding_right =
      ValueForLength(style.PaddingRight(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_left;
  if (!style.MarginLeft().IsAuto())
    margin_left =
        ValueForLength(style.MarginLeft(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_right;
  if (!style.MarginRight().IsAuto())
    margin_right =
        ValueForLength(style.MarginRight(), percentage_logical.inline_size);
  Optional<LayoutUnit> left;
  if (!style.Left().IsAuto())
    left = ValueForLength(style.Left(), percentage_physical.width);
  Optional<LayoutUnit> right;
  if (!style.Right().IsAuto())
    right = ValueForLength(style.Right(), percentage_physical.width);
  LayoutUnit border_padding =
      border_left + border_right + padding_left + padding_right;
  Optional<LayoutUnit> width = incoming_width;
  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.WritingMode());
  DCHECK_NE(container_size.width, NGSizeIndefinite);

  // Solving the equation:
  // left + marginLeft + width + marginRight + right  = container width
  if (!left && !right && !width) {
    // Standard: "If all three of left, width, and right are auto:"
    if (!margin_left)
      margin_left = LayoutUnit();
    if (!margin_right)
      margin_right = LayoutUnit();
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(container_size.width);
    if (space.Direction() == TextDirection::kLtr) {
      left = static_position.LeftPosition(container_size.width, *width,
                                          *margin_left, *margin_right);
    } else {
      right = static_position.RightPosition(container_size.width, *width,
                                            *margin_left, *margin_right);
    }
  } else if (left && right && width) {
    // Standard: "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.width - *left - *right - *width;
    // When both margins are auto.
    if (!margin_left && !margin_right) {
      if (margin_space > 0) {
        margin_left = margin_space / 2;
        margin_right = margin_space / 2;
      } else {
        // Margins are negative.
        if (space.Direction() == TextDirection::kLtr) {
          margin_left = LayoutUnit();
          margin_right = margin_space;
        } else {
          margin_right = LayoutUnit();
          margin_left = margin_space;
        }
      }
    } else if (!margin_left) {
      margin_left = margin_space - *margin_right;
    } else if (!margin_right) {
      margin_right = margin_space - *margin_left;
    } else {
      // Are values overconstrained?
      LayoutUnit margin_extra = margin_space - *margin_left - *margin_right;
      if (margin_extra) {
        // Relax the end.
        if (space.Direction() == TextDirection::kLtr)
          right = *right + margin_extra;
        else
          left = *left + margin_extra;
      }
    }
  }

  // Set unknown margins.
  if (!margin_left)
    margin_left = LayoutUnit();
  if (!margin_right)
    margin_right = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown.
  if (!left && !width) {
    // Rule 1: left/width are unknown.
    DCHECK(right.has_value());
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(container_size.width);
  } else if (!left && !right) {
    // Rule 2.
    DCHECK(width.has_value());
    if (space.Direction() == TextDirection::kLtr)
      left = static_position.LeftPosition(container_size.width, *width,
                                          *margin_left, *margin_right);
    else
      right = static_position.RightPosition(container_size.width, *width,
                                            *margin_left, *margin_right);
  } else if (!width && !right) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(container_size.width);
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!left) {
    left =
        container_size.width - *right - *width - *margin_left - *margin_right;
  } else if (!right) {
    right =
        container_size.width - *left - *width - *margin_left - *margin_right;
  } else if (!width) {
    width =
        container_size.width - *left - *right - *margin_left - *margin_right;
  }
  DCHECK_EQ(container_size.width,
            *left + *right + *margin_left + *margin_right + *width);

  // If calculated width is outside of min/max constraints,
  // rerun the algorithm with constrained width.
  Optional<LayoutUnit> min_width;
  if (!style.MinWidth().IsAuto())
    min_width = ResolveWidth(style.MinWidth(), space, style, child_minmax,
                             LengthResolveType::kMinSize);
  Optional<LayoutUnit> max_width;
  if (!style.MaxWidth().IsMaxSizeNone())
    max_width = ResolveWidth(style.MaxWidth(), space, style, child_minmax,
                             LengthResolveType::kMaxSize);
  if (width != ConstrainByMinMax(*width, min_width, max_width)) {
    width = ConstrainByMinMax(*width, min_width, max_width);
    // Because this function only changes "width" when it's not already
    // set, it is safe to recursively call ourselves here because on the
    // second call it is guaranteed to be within min..max.
    ComputeAbsoluteHorizontal(space, style, width, static_position,
                              child_minmax, position);
    return;
  }

  // Negative widths are not allowed.
  width = std::max(*width, border_padding);

  position->inset.left = *left + *margin_left;
  position->inset.right = *right + *margin_right;
  position->size.width = *width;
}

// Implements absolute vertical size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
void ComputeAbsoluteVertical(const NGConstraintSpace& space,
                             const ComputedStyle& style,
                             const Optional<LayoutUnit>& incoming_height,
                             const NGStaticPosition& static_position,
                             const Optional<MinMaxSize>& child_minmax,
                             NGAbsolutePhysicalPosition* position) {
  NGLogicalSize percentage_logical = space.PercentageResolutionSize();
  NGPhysicalSize percentage_physical =
      percentage_logical.ConvertToPhysical(space.WritingMode());

  LayoutUnit border_top(style.BorderTopWidth());
  LayoutUnit border_bottom(style.BorderBottomWidth());
  LayoutUnit padding_top =
      ValueForLength(style.PaddingTop(), percentage_logical.inline_size);
  LayoutUnit padding_bottom =
      ValueForLength(style.PaddingBottom(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_top;
  if (!style.MarginTop().IsAuto())
    margin_top =
        ValueForLength(style.MarginTop(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_bottom;
  if (!style.MarginBottom().IsAuto())
    margin_bottom =
        ValueForLength(style.MarginBottom(), percentage_logical.inline_size);
  Optional<LayoutUnit> top;
  if (!style.Top().IsAuto())
    top = ValueForLength(style.Top(), percentage_physical.height);
  Optional<LayoutUnit> bottom;
  if (!style.Bottom().IsAuto())
    bottom = ValueForLength(style.Bottom(), percentage_physical.height);
  LayoutUnit border_padding =
      border_top + border_bottom + padding_top + padding_bottom;
  Optional<LayoutUnit> height = incoming_height;

  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.WritingMode());
  DCHECK_NE(container_size.height, NGSizeIndefinite);

  // Solving the equation:
  // top + marginTop + height + marginBottom + bottom
  // + border_padding = container height
  if (!top && !bottom && !height) {
    // Standard: "If all three of top, height, and bottom are auto:"
    if (!margin_top)
      margin_top = LayoutUnit();
    if (!margin_bottom)
      margin_bottom = LayoutUnit();
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(container_size.height);
    top = static_position.TopPosition(container_size.height, *height,
                                      *margin_top, *margin_bottom);
  } else if (top && bottom && height) {
    // Standard: "If top, bottom, and height are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.height - *top - *bottom - *height;
    // When both margins are auto.
    if (!margin_top && !margin_bottom) {
      if (margin_space > 0) {
        margin_top = margin_space / 2;
        margin_bottom = margin_space / 2;
      } else {
        // Margin space is over-constrained.
        margin_top = LayoutUnit();
        margin_bottom = margin_space;
      }
    } else if (!margin_top) {
      margin_top = margin_space - *margin_bottom;
    } else if (!margin_bottom) {
      margin_bottom = margin_space - *margin_top;
    } else {
      LayoutUnit margin_extra = margin_space - *margin_top - *margin_bottom;
      if (margin_extra)
        bottom = *bottom + margin_extra;
    }
  }

  // Set unknown margins.
  if (!margin_top)
    margin_top = LayoutUnit();
  if (!margin_bottom)
    margin_bottom = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown, fix 1.
  if (!top && !height) {
    // Rule 1.
    DCHECK(bottom.has_value());
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(container_size.height);
  } else if (!top && !bottom) {
    // Rule 2.
    DCHECK(height.has_value());
    top = static_position.TopPosition(container_size.height, *height,
                                      *margin_top, *margin_bottom);
  } else if (!height && !bottom) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(container_size.height);
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!top) {
    top = container_size.height - *bottom - *height - *margin_top -
          *margin_bottom;
  } else if (!bottom) {
    bottom =
        container_size.height - *top - *height - *margin_top - *margin_bottom;
  } else if (!height) {
    height =
        container_size.height - *top - *bottom - *margin_top - *margin_bottom;
  }
  DCHECK_EQ(container_size.height,
            *top + *bottom + *margin_top + *margin_bottom + *height);

  // If calculated height is outside of min/max constraints,
  // rerun the algorithm with constrained width.
  Optional<LayoutUnit> min_height;
  if (!style.MinHeight().IsAuto())
    min_height = ResolveHeight(style.MinHeight(), space, style, child_minmax,
                               LengthResolveType::kMinSize);
  Optional<LayoutUnit> max_height;
  if (!style.MaxHeight().IsMaxSizeNone())
    max_height = ResolveHeight(style.MaxHeight(), space, style, child_minmax,
                               LengthResolveType::kMaxSize);
  if (height != ConstrainByMinMax(*height, min_height, max_height)) {
    height = ConstrainByMinMax(*height, min_height, max_height);
    // Because this function only changes "height" when it's not already
    // set, it is safe to recursively call ourselves here because on the
    // second call it is guaranteed to be within min..max.
    ComputeAbsoluteVertical(space, style, height, static_position, child_minmax,
                            position);
    return;
  }
  // Negative heights are not allowed.
  height = std::max(*height, border_padding);

  position->inset.top = *top + *margin_top;
  position->inset.bottom = *bottom + *margin_bottom;
  position->size.height = *height;
}

}  // namespace

String NGAbsolutePhysicalPosition::ToString() const {
  return String::Format("INSET(LRTB):%d,%d,%d,%d SIZE:%dx%d",
                        inset.left.ToInt(), inset.right.ToInt(),
                        inset.top.ToInt(), inset.bottom.ToInt(),
                        size.width.ToInt(), size.height.ToInt());
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return AbsoluteVerticalNeedsEstimate(style);
  else
    return AbsoluteHorizontalNeedsEstimate(style);
}

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return AbsoluteHorizontalNeedsEstimate(style);
  else
    return AbsoluteVerticalNeedsEstimate(style);
}

NGAbsolutePhysicalPosition ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<MinMaxSize>& child_minmax) {
  NGAbsolutePhysicalPosition position;
  if (style.IsHorizontalWritingMode()) {
    Optional<LayoutUnit> width;
    if (!style.Width().IsAuto()) {
      width = ResolveWidth(style.Width(), space, style, child_minmax,
                           LengthResolveType::kContentSize);
    }
    ComputeAbsoluteHorizontal(space, style, width, static_position,
                              child_minmax, &position);
  } else {
    Optional<LayoutUnit> height;
    if (!style.Height().IsAuto()) {
      height = ResolveHeight(style.Height(), space, style, child_minmax,
                             LengthResolveType::kContentSize);
    }
    ComputeAbsoluteVertical(space, style, height, static_position, child_minmax,
                            &position);
  }
  return position;
}

void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<LayoutUnit>& child_block_size,
    NGAbsolutePhysicalPosition* position) {
  // After partial size has been computed, child block size is either
  // unknown, or fully computed, there is no minmax.
  // To express this, a 'fixed' minmax is created where
  // min and max are the same.
  Optional<MinMaxSize> child_minmax;
  if (child_block_size.has_value()) {
    child_minmax = MinMaxSize{*child_block_size, *child_block_size};
  }
  if (style.IsHorizontalWritingMode()) {
    Optional<LayoutUnit> height;
    if (!style.Height().IsAuto()) {
      height = ResolveHeight(style.Height(), space, style, child_minmax,
                             LengthResolveType::kContentSize);
    }
    ComputeAbsoluteVertical(space, style, height, static_position, child_minmax,
                            position);
  } else {
    Optional<LayoutUnit> width;
    if (!style.Width().IsAuto()) {
      width = ResolveWidth(style.Width(), space, style, child_minmax,
                           LengthResolveType::kContentSize);
    }
    ComputeAbsoluteHorizontal(space, style, width, static_position,
                              child_minmax, position);
  }
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"

namespace blink {
// TODO(layout-ng):
// - positioned and/or replaced calculations
// - Take scrollbars into account

namespace {

// Converts physical dimensions to logical ones per
// https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
// For now it's only used to calculate abstract values for margins.
NGBoxStrut ToLogicalDimensions(const NGPhysicalDimensions& physical_dim,
                               const NGWritingMode writing_mode,
                               const NGDirection direction) {
  bool is_ltr = direction == LeftToRight;
  NGBoxStrut logical_dim;
  switch (writing_mode) {
    case VerticalRightLeft:
    case SidewaysRightLeft:
      logical_dim = {is_ltr ? physical_dim.top : physical_dim.bottom,
                     is_ltr ? physical_dim.bottom : physical_dim.top,
                     physical_dim.right, physical_dim.left};
      break;
    case VerticalLeftRight:
      logical_dim = {is_ltr ? physical_dim.top : physical_dim.bottom,
                     is_ltr ? physical_dim.bottom : physical_dim.top,
                     physical_dim.left, physical_dim.right};
      break;
    case SidewaysLeftRight:
      logical_dim = {is_ltr ? physical_dim.bottom : physical_dim.top,
                     is_ltr ? physical_dim.top : physical_dim.bottom,
                     physical_dim.left, physical_dim.right};
      break;
    default:
      NOTREACHED();
    /* FALLTHROUGH */
    case HorizontalTopBottom:
      logical_dim = {is_ltr ? physical_dim.left : physical_dim.right,
                     is_ltr ? physical_dim.right : physical_dim.left,
                     physical_dim.top, physical_dim.bottom};
      break;
  }
  return logical_dim;
}

}  // namespace

LayoutUnit ResolveInlineLength(const NGConstraintSpace& constraintSpace,
                               const ComputedStyle& style,
                               const Length& length,
                               LengthResolveType type) {
  // TODO(layout-ng): Handle min/max/fit-content
  DCHECK(!length.isMaxSizeNone());
  DCHECK_GE(constraintSpace.ContainerSize().inline_size, LayoutUnit());

  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();

  if (type == LengthResolveType::MarginBorderPaddingSize && length.isAuto())
    return LayoutUnit();

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::MarginBorderPaddingSize) {
    border_and_padding =
        ComputeBorders(style) + ComputePadding(constraintSpace, style);
  }
  LayoutUnit container_size = constraintSpace.ContainerSize().inline_size;
  switch (length.type()) {
    case Auto:
    case FillAvailable: {
      NGBoxStrut margins =
          ComputeMargins(constraintSpace, style,
                         FromPlatformWritingMode(style.getWritingMode()),
                         FromPlatformDirection(style.direction()));
      return std::max(border_and_padding.InlineSum(),
                      container_size - margins.InlineSum());
    }
    case Percent:
    case Fixed:
    case Calculated: {
      LayoutUnit value = valueForLength(length, container_size);
      if (style.boxSizing() == BoxSizingContentBox) {
        value += border_and_padding.InlineSum();
      } else {
        value = std::max(border_and_padding.InlineSum(), value);
      }
      return value;
    }
    case MinContent:
    case MaxContent:
    case FitContent:
      // TODO(layout-ng): implement
      return border_and_padding.InlineSum();
    case DeviceWidth:
    case DeviceHeight:
    case ExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
    case MaxSizeNone:
    default:
      NOTREACHED();
      return border_and_padding.InlineSum();
  }
}

LayoutUnit ResolveBlockLength(const NGConstraintSpace& constraintSpace,
                              const ComputedStyle& style,
                              const Length& length,
                              LayoutUnit contentSize,
                              LengthResolveType type) {
  DCHECK(!length.isMaxSizeNone());
  DCHECK(type != LengthResolveType::MarginBorderPaddingSize);

  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();

  // Make sure that indefinite percentages resolve to NGSizeIndefinite, not to
  // a random negative number.
  if (length.isPercentOrCalc() &&
      constraintSpace.ContainerSize().block_size == NGSizeIndefinite)
    return contentSize;

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::MarginBorderPaddingSize) {
    border_and_padding =
        ComputeBorders(style) + ComputePadding(constraintSpace, style);
  }
  LayoutUnit container_size = constraintSpace.ContainerSize().block_size;
  switch (length.type()) {
    case FillAvailable: {
      NGBoxStrut margins =
          ComputeMargins(constraintSpace, style,
                         FromPlatformWritingMode(style.getWritingMode()),
                         FromPlatformDirection(style.direction()));
      return std::max(border_and_padding.BlockSum(),
                      container_size - margins.BlockSum());
    }
    case Percent:
    case Fixed:
    case Calculated: {
      LayoutUnit value = valueForLength(length, container_size);
      if (style.boxSizing() == BoxSizingContentBox) {
        value += border_and_padding.BlockSum();
      } else {
        value = std::max(border_and_padding.BlockSum(), value);
      }
      return value;
    }
    case Auto:
    case MinContent:
    case MaxContent:
    case FitContent:
      // Due to how contentSize is calculated, it should always include border
      // and padding.
      if (contentSize != LayoutUnit(-1))
        DCHECK_GE(contentSize, border_and_padding.BlockSum());
      return contentSize;
    case DeviceWidth:
    case DeviceHeight:
    case ExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
    case MaxSizeNone:
    default:
      NOTREACHED();
      return border_and_padding.BlockSum();
  }
}

LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace& constraintSpace,
    const ComputedStyle& style) {
  if (constraintSpace.FixedInlineSize())
    return constraintSpace.ContainerSize().inline_size;

  LayoutUnit extent =
      ResolveInlineLength(constraintSpace, style, style.logicalWidth(),
                          LengthResolveType::ContentSize);

  Length maxLength = style.logicalMaxWidth();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max = ResolveInlineLength(constraintSpace, style, maxLength,
                                         LengthResolveType::MaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min =
      ResolveInlineLength(constraintSpace, style, style.logicalMinWidth(),
                          LengthResolveType::MinSize);
  extent = std::max(extent, min);
  return extent;
}

LayoutUnit ComputeBlockSizeForFragment(const NGConstraintSpace& constraintSpace,
                                       const ComputedStyle& style,
                                       LayoutUnit contentSize) {
  if (constraintSpace.FixedBlockSize())
    return constraintSpace.ContainerSize().block_size;

  LayoutUnit extent =
      ResolveBlockLength(constraintSpace, style, style.logicalHeight(),
                         contentSize, LengthResolveType::ContentSize);
  if (extent == NGSizeIndefinite) {
    DCHECK_EQ(contentSize, NGSizeIndefinite);
    return extent;
  }

  Length maxLength = style.logicalMaxHeight();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max =
        ResolveBlockLength(constraintSpace, style, maxLength, contentSize,
                           LengthResolveType::MaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min =
      ResolveBlockLength(constraintSpace, style, style.logicalMinHeight(),
                         contentSize, LengthResolveType::MinSize);
  extent = std::max(extent, min);
  return extent;
}

NGBoxStrut ComputeMargins(const NGConstraintSpace& constraintSpace,
                          const ComputedStyle& style,
                          const NGWritingMode writing_mode,
                          const NGDirection direction) {
  // Margins always get computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-margin-width
  NGPhysicalDimensions physical_dim;
  physical_dim.left =
      ResolveInlineLength(constraintSpace, style, style.marginLeft(),
                          LengthResolveType::MarginBorderPaddingSize);
  physical_dim.right =
      ResolveInlineLength(constraintSpace, style, style.marginRight(),
                          LengthResolveType::MarginBorderPaddingSize);
  physical_dim.top =
      ResolveInlineLength(constraintSpace, style, style.marginTop(),
                          LengthResolveType::MarginBorderPaddingSize);
  physical_dim.bottom =
      ResolveInlineLength(constraintSpace, style, style.marginBottom(),
                          LengthResolveType::MarginBorderPaddingSize);
  return ToLogicalDimensions(physical_dim, writing_mode, direction);
}

NGBoxStrut ComputeBorders(const ComputedStyle& style) {
  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.borderStartWidth());
  borders.inline_end = LayoutUnit(style.borderEndWidth());
  borders.block_start = LayoutUnit(style.borderBeforeWidth());
  borders.block_end = LayoutUnit(style.borderAfterWidth());
  return borders;
}

NGBoxStrut ComputePadding(const NGConstraintSpace& constraintSpace,
                          const ComputedStyle& style) {
  // Padding always gets computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-padding-width
  NGBoxStrut padding;
  padding.inline_start =
      ResolveInlineLength(constraintSpace, style, style.paddingStart(),
                          LengthResolveType::MarginBorderPaddingSize);
  padding.inline_end =
      ResolveInlineLength(constraintSpace, style, style.paddingEnd(),
                          LengthResolveType::MarginBorderPaddingSize);
  padding.block_start =
      ResolveInlineLength(constraintSpace, style, style.paddingBefore(),
                          LengthResolveType::MarginBorderPaddingSize);
  padding.block_end =
      ResolveInlineLength(constraintSpace, style, style.paddingAfter(),
                          LengthResolveType::MarginBorderPaddingSize);
  return padding;
}

void ApplyAutoMargins(const NGConstraintSpace& constraint_space,
                      const ComputedStyle& style,
                      const NGFragment& fragment,
                      NGBoxStrut* margins) {
  DCHECK(margins) << "Margins cannot be NULL here";
  const LayoutUnit used_space = fragment.InlineSize() + margins->InlineSum();
  const LayoutUnit available_space =
      constraint_space.ContainerSize().inline_size - used_space;
  if (available_space < LayoutUnit())
    return;
  if (style.marginStart().isAuto() && style.marginEnd().isAuto()) {
    margins->inline_start = available_space / 2;
    margins->inline_end = available_space - margins->inline_start;
  } else if (style.marginStart().isAuto()) {
    margins->inline_start = available_space;
  } else if (style.marginEnd().isAuto()) {
    margins->inline_end = available_space;
  }
}

}  // namespace blink

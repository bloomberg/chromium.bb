// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"
#include "wtf/Optional.h"

namespace blink {
// TODO(layout-ng):
// - positioned and/or replaced calculations
// - Take scrollbars into account

bool NeedMinAndMaxContentSizes(const ComputedStyle& style) {
  // TODO(layout-ng): In the future we may pass a shrink-to-fit flag through the
  // constraint space; if so, this function needs to take a constraint space
  // as well to take that into account.
  // This check is technically too broad (fill-available does not need intrinsic
  // size computation) but that's a rare case and only affects performance, not
  // correctness.
  return style.logicalWidth().isIntrinsic() ||
         style.logicalMinWidth().isIntrinsic() ||
         style.logicalMaxWidth().isIntrinsic();
}

LayoutUnit ResolveInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const WTF::Optional<MinAndMaxContentSizes>& min_and_max,
    const Length& length,
    LengthResolveType type) {
  // TODO(layout-ng): Handle min/max/fit-content
  DCHECK(!length.isMaxSizeNone());
  DCHECK_GE(constraint_space.AvailableSize().inline_size, LayoutUnit());

  if (type == LengthResolveType::kMinSize && length.isAuto())
    return LayoutUnit();

  if (type == LengthResolveType::kMarginBorderPaddingSize && length.isAuto())
    return LayoutUnit();

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::kMarginBorderPaddingSize) {
    border_and_padding =
        ComputeBorders(style) + ComputePadding(constraint_space, style);
  }
  switch (length.type()) {
    case Auto:
    case FillAvailable: {
      LayoutUnit content_size = constraint_space.AvailableSize().inline_size;
      NGBoxStrut margins = ComputeMargins(
          constraint_space, style,
          FromPlatformWritingMode(style.getWritingMode()), style.direction());
      return std::max(border_and_padding.InlineSum(),
                      content_size - margins.InlineSum());
    }
    case Percent:
    case Fixed:
    case Calculated: {
      LayoutUnit percentage_resolution_size =
          constraint_space.PercentageResolutionSize().inline_size;
      LayoutUnit value = valueForLength(length, percentage_resolution_size);
      if (style.boxSizing() == BoxSizingContentBox) {
        value += border_and_padding.InlineSum();
      } else {
        value = std::max(border_and_padding.InlineSum(), value);
      }
      return value;
    }
    case MinContent:
    case MaxContent:
    case FitContent: {
      DCHECK(min_and_max.has_value());
      LayoutUnit available_size = constraint_space.AvailableSize().inline_size;
      LayoutUnit value;
      if (length.isMinContent()) {
        value = min_and_max->min_content;
      } else if (length.isMaxContent() || available_size == LayoutUnit::max()) {
        // If the available space is infinite, fit-content resolves to
        // max-content. See css-sizing section 2.1.
        value = min_and_max->max_content;
      } else {
        NGBoxStrut margins = ComputeMargins(
            constraint_space, style,
            FromPlatformWritingMode(style.getWritingMode()), style.direction());
        LayoutUnit fill_available =
            std::max(LayoutUnit(), available_size - margins.InlineSum() -
                                       border_and_padding.InlineSum());
        value = min_and_max->ShrinkToFit(fill_available);
      }
      return value + border_and_padding.InlineSum();
    }
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

LayoutUnit ResolveBlockLength(const NGConstraintSpace& constraint_space,
                              const ComputedStyle& style,
                              const Length& length,
                              LayoutUnit content_size,
                              LengthResolveType type) {
  DCHECK(!length.isMaxSizeNone());
  DCHECK(type != LengthResolveType::kMarginBorderPaddingSize);

  if (type == LengthResolveType::kMinSize && length.isAuto())
    return LayoutUnit();

  // Make sure that indefinite percentages resolve to NGSizeIndefinite, not to
  // a random negative number.
  if (length.isPercentOrCalc() &&
      constraint_space.PercentageResolutionSize().block_size ==
          NGSizeIndefinite)
    return content_size;

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::kMarginBorderPaddingSize) {
    border_and_padding =
        ComputeBorders(style) + ComputePadding(constraint_space, style);
  }
  switch (length.type()) {
    case FillAvailable: {
      LayoutUnit content_size = constraint_space.AvailableSize().block_size;
      NGBoxStrut margins = ComputeMargins(
          constraint_space, style,
          FromPlatformWritingMode(style.getWritingMode()), style.direction());
      return std::max(border_and_padding.BlockSum(),
                      content_size - margins.BlockSum());
    }
    case Percent:
    case Fixed:
    case Calculated: {
      LayoutUnit percentage_resolution_size =
          constraint_space.PercentageResolutionSize().block_size;
      LayoutUnit value = valueForLength(length, percentage_resolution_size);
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
      // Due to how content_size is calculated, it should always include border
      // and padding.
      if (content_size != LayoutUnit(-1))
        DCHECK_GE(content_size, border_and_padding.BlockSum());
      return content_size;
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
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const WTF::Optional<MinAndMaxContentSizes>& min_and_max) {
  if (constraint_space.FixedInlineSize())
    return constraint_space.AvailableSize().inline_size;

  LayoutUnit extent = ResolveInlineLength(constraint_space, style, min_and_max,
                                          style.logicalWidth(),
                                          LengthResolveType::kContentSize);

  Length max_length = style.logicalMaxWidth();
  if (!max_length.isMaxSizeNone()) {
    LayoutUnit max =
        ResolveInlineLength(constraint_space, style, min_and_max, max_length,
                            LengthResolveType::kMaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min =
      ResolveInlineLength(constraint_space, style, min_and_max,
                          style.logicalMinWidth(), LengthResolveType::kMinSize);
  extent = std::max(extent, min);
  return extent;
}

LayoutUnit ComputeBlockSizeForFragment(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    LayoutUnit content_size) {
  if (constraint_space.FixedBlockSize())
    return constraint_space.AvailableSize().block_size;

  LayoutUnit extent =
      ResolveBlockLength(constraint_space, style, style.logicalHeight(),
                         content_size, LengthResolveType::kContentSize);
  if (extent == NGSizeIndefinite) {
    DCHECK_EQ(content_size, NGSizeIndefinite);
    return extent;
  }

  Length max_length = style.logicalMaxHeight();
  if (!max_length.isMaxSizeNone()) {
    LayoutUnit max =
        ResolveBlockLength(constraint_space, style, max_length, content_size,
                           LengthResolveType::kMaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min =
      ResolveBlockLength(constraint_space, style, style.logicalMinHeight(),
                         content_size, LengthResolveType::kMinSize);
  extent = std::max(extent, min);
  return extent;
}

int ResolveUsedColumnCount(int computed_count,
                           LayoutUnit computed_size,
                           LayoutUnit used_gap,
                           LayoutUnit available_size) {
  if (computed_size == NGSizeIndefinite) {
    DCHECK(computed_count);
    return computed_count;
  }
  DCHECK(computed_size > LayoutUnit());
  int count_from_width =
      ((available_size + used_gap) / (computed_size + used_gap)).toInt();
  count_from_width = std::max(1, count_from_width);
  if (!computed_count)
    return count_from_width;
  return std::max(1, std::min(computed_count, count_from_width));
}

LayoutUnit ResolveUsedColumnInlineSize(int computed_count,
                                       LayoutUnit computed_size,
                                       LayoutUnit used_gap,
                                       LayoutUnit available_size) {
  int used_count = ResolveUsedColumnCount(computed_count, computed_size,
                                          used_gap, available_size);
  return ((available_size + used_gap) / used_count) - used_gap;
}

LayoutUnit ResolveUsedColumnInlineSize(LayoutUnit available_size,
                                       const ComputedStyle& style) {
  // Should only attempt to resolve this if columns != auto.
  DCHECK(!style.hasAutoColumnCount() || !style.hasAutoColumnWidth());

  LayoutUnit computed_size =
      style.hasAutoColumnWidth()
          ? NGSizeIndefinite
          : std::max(LayoutUnit(1), LayoutUnit(style.columnWidth()));
  int computed_count = style.hasAutoColumnCount() ? 0 : style.columnCount();
  LayoutUnit used_gap = ResolveUsedColumnGap(style);
  return ResolveUsedColumnInlineSize(computed_count, computed_size, used_gap,
                                     available_size);
}

LayoutUnit ResolveUsedColumnGap(const ComputedStyle& style) {
  if (style.hasNormalColumnGap())
    return LayoutUnit(style.getFontDescription().computedPixelSize());
  return LayoutUnit(style.columnGap());
}

NGBoxStrut ComputeMargins(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style,
                          const NGWritingMode writing_mode,
                          const TextDirection direction) {
  // We don't need these for margin computations
  MinAndMaxContentSizes empty_sizes;
  // Margins always get computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-margin-width
  NGPhysicalBoxStrut physical_dim;
  physical_dim.left = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.marginLeft(),
      LengthResolveType::kMarginBorderPaddingSize);
  physical_dim.right = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.marginRight(),
      LengthResolveType::kMarginBorderPaddingSize);
  physical_dim.top = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.marginTop(),
      LengthResolveType::kMarginBorderPaddingSize);
  physical_dim.bottom = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.marginBottom(),
      LengthResolveType::kMarginBorderPaddingSize);
  return physical_dim.ConvertToLogical(writing_mode, direction);
}

NGBoxStrut ComputeBorders(const ComputedStyle& style) {
  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.borderStartWidth());
  borders.inline_end = LayoutUnit(style.borderEndWidth());
  borders.block_start = LayoutUnit(style.borderBeforeWidth());
  borders.block_end = LayoutUnit(style.borderAfterWidth());
  return borders;
}

NGBoxStrut ComputePadding(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style) {
  // We don't need these for padding computations
  MinAndMaxContentSizes empty_sizes;
  // Padding always gets computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-padding-width
  NGBoxStrut padding;
  padding.inline_start = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.paddingStart(),
      LengthResolveType::kMarginBorderPaddingSize);
  padding.inline_end = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.paddingEnd(),
      LengthResolveType::kMarginBorderPaddingSize);
  padding.block_start = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.paddingBefore(),
      LengthResolveType::kMarginBorderPaddingSize);
  padding.block_end = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.paddingAfter(),
      LengthResolveType::kMarginBorderPaddingSize);
  return padding;
}

void ApplyAutoMargins(const NGConstraintSpace& constraint_space,
                      const ComputedStyle& style,
                      const NGFragmentBase& fragment,
                      NGBoxStrut* margins) {
  DCHECK(margins) << "Margins cannot be NULL here";
  const LayoutUnit used_space = fragment.InlineSize() + margins->InlineSum();
  const LayoutUnit available_space =
      constraint_space.AvailableSize().inline_size - used_space;
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

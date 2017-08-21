// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"
#include "platform/wtf/Optional.h"

namespace blink {
// TODO(layout-ng):
// - replaced calculations
// - Take scrollbars into account

bool NeedMinMaxSize(const NGConstraintSpace& constraint_space,
                    const ComputedStyle& style) {
  // This check is technically too broad (fill-available does not need intrinsic
  // size computation) but that's a rare case and only affects performance, not
  // correctness.
  return constraint_space.IsShrinkToFit() || NeedMinMaxSize(style);
}

bool NeedMinMaxSize(const ComputedStyle& style) {
  return style.LogicalWidth().IsIntrinsic() ||
         style.LogicalMinWidth().IsIntrinsic() ||
         style.LogicalMaxWidth().IsIntrinsic();
}

bool NeedMinMaxSizeForContentContribution(const ComputedStyle& style) {
  return style.LogicalWidth().IsIntrinsicOrAuto() ||
         style.LogicalMinWidth().IsIntrinsic() ||
         style.LogicalMaxWidth().IsIntrinsic();
}

LayoutUnit ResolveInlineLength(const NGConstraintSpace& constraint_space,
                               const ComputedStyle& style,
                               const WTF::Optional<MinMaxSize>& min_and_max,
                               const Length& length,
                               LengthResolveType type) {
  DCHECK(!length.IsMaxSizeNone());
  DCHECK_GE(constraint_space.AvailableSize().inline_size, LayoutUnit());
  DCHECK_GE(constraint_space.PercentageResolutionSize().inline_size,
            LayoutUnit());
  DCHECK_EQ(constraint_space.WritingMode(),
            FromPlatformWritingMode(style.GetWritingMode()));

  if (type == LengthResolveType::kMinSize && length.IsAuto())
    return LayoutUnit();

  if (type == LengthResolveType::kMarginBorderPaddingSize && length.IsAuto())
    return LayoutUnit();

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::kMarginBorderPaddingSize) {
    border_and_padding = ComputeBorders(constraint_space, style) +
                         ComputePadding(constraint_space, style);
  }
  switch (length.GetType()) {
    case kAuto:
    case kFillAvailable: {
      LayoutUnit content_size = constraint_space.AvailableSize().inline_size;
      NGBoxStrut margins = ComputeMargins(
          constraint_space, style,
          FromPlatformWritingMode(style.GetWritingMode()), style.Direction());
      return std::max(border_and_padding.InlineSum(),
                      content_size - margins.InlineSum());
    }
    case kPercent:
    case kFixed:
    case kCalculated: {
      LayoutUnit percentage_resolution_size =
          constraint_space.PercentageResolutionSize().inline_size;
      LayoutUnit value = ValueForLength(length, percentage_resolution_size);
      if (style.BoxSizing() == EBoxSizing::kContentBox) {
        value += border_and_padding.InlineSum();
      } else {
        value = std::max(border_and_padding.InlineSum(), value);
      }
      return value;
    }
    case kMinContent:
    case kMaxContent:
    case kFitContent: {
      DCHECK(min_and_max.has_value());
      LayoutUnit available_size = constraint_space.AvailableSize().inline_size;
      LayoutUnit value;
      if (length.IsMinContent()) {
        value = min_and_max->min_size;
      } else if (length.IsMaxContent() || available_size == LayoutUnit::Max()) {
        // If the available space is infinite, fit-content resolves to
        // max-content. See css-sizing section 2.1.
        value = min_and_max->max_size;
      } else {
        NGBoxStrut margins = ComputeMargins(
            constraint_space, style,
            FromPlatformWritingMode(style.GetWritingMode()), style.Direction());
        LayoutUnit fill_available =
            std::max(LayoutUnit(), available_size - margins.InlineSum() -
                                       border_and_padding.InlineSum());
        value = min_and_max->ShrinkToFit(fill_available);
      }
      return value + border_and_padding.InlineSum();
    }
    case kDeviceWidth:
    case kDeviceHeight:
    case kExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
    case kMaxSizeNone:
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
  DCHECK(!length.IsMaxSizeNone());
  DCHECK_NE(type, LengthResolveType::kMarginBorderPaddingSize);
  DCHECK_EQ(constraint_space.WritingMode(),
            FromPlatformWritingMode(style.GetWritingMode()));

  if (type == LengthResolveType::kMinSize && length.IsAuto())
    return LayoutUnit();

  // Make sure that indefinite percentages resolve to NGSizeIndefinite, not to
  // a random negative number.
  if (length.IsPercentOrCalc() &&
      constraint_space.PercentageResolutionSize().block_size ==
          NGSizeIndefinite)
    return content_size;

  // We don't need this when we're resolving margin/border/padding; skip
  // computing it as an optimization and to simplify the code below.
  NGBoxStrut border_and_padding;
  if (type != LengthResolveType::kMarginBorderPaddingSize) {
    border_and_padding = ComputeBorders(constraint_space, style) +
                         ComputePadding(constraint_space, style);
  }
  switch (length.GetType()) {
    case kFillAvailable: {
      LayoutUnit content_size = constraint_space.AvailableSize().block_size;
      NGBoxStrut margins = ComputeMargins(
          constraint_space, style,
          FromPlatformWritingMode(style.GetWritingMode()), style.Direction());
      return std::max(border_and_padding.BlockSum(),
                      content_size - margins.BlockSum());
    }
    case kPercent:
    case kFixed:
    case kCalculated: {
      LayoutUnit percentage_resolution_size =
          constraint_space.PercentageResolutionSize().block_size;
      LayoutUnit value = ValueForLength(length, percentage_resolution_size);
      if (style.BoxSizing() == EBoxSizing::kContentBox) {
        value += border_and_padding.BlockSum();
      } else {
        value = std::max(border_and_padding.BlockSum(), value);
      }
      return value;
    }
    case kAuto:
    case kMinContent:
    case kMaxContent:
    case kFitContent:
      // Due to how content_size is calculated, it should always include border
      // and padding.
      if (content_size != LayoutUnit(-1))
        DCHECK_GE(content_size, border_and_padding.BlockSum());
      return content_size;
    case kDeviceWidth:
    case kDeviceHeight:
    case kExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
    case kMaxSizeNone:
    default:
      NOTREACHED();
      return border_and_padding.BlockSum();
  }
}

MinMaxSize ComputeMinAndMaxContentContribution(
    const ComputedStyle& style,
    const WTF::Optional<MinMaxSize>& min_and_max) {
  // Synthesize a zero-sized constraint space for passing to
  // ResolveInlineLength.
  NGWritingMode writing_mode = FromPlatformWritingMode(style.GetWritingMode());
  NGConstraintSpaceBuilder builder(
      writing_mode,
      /* icb_size */ {NGSizeIndefinite, NGSizeIndefinite});
  RefPtr<NGConstraintSpace> space = builder.ToConstraintSpace(writing_mode);

  MinMaxSize computed_sizes;
  Length inline_size = style.LogicalWidth();
  if (inline_size.IsAuto()) {
    CHECK(min_and_max.has_value());
    NGBoxStrut border_and_padding =
        ComputeBorders(*space, style) + ComputePadding(*space, style);
    computed_sizes.min_size =
        min_and_max->min_size + border_and_padding.InlineSum();
    computed_sizes.max_size =
        min_and_max->max_size + border_and_padding.InlineSum();
  } else {
    computed_sizes.min_size = computed_sizes.max_size =
        ResolveInlineLength(*space, style, min_and_max, inline_size,
                            LengthResolveType::kContentSize);
  }

  Length max_length = style.LogicalMaxWidth();
  if (!max_length.IsMaxSizeNone()) {
    LayoutUnit max = ResolveInlineLength(*space, style, min_and_max, max_length,
                                         LengthResolveType::kMaxSize);
    computed_sizes.min_size = std::min(computed_sizes.min_size, max);
    computed_sizes.max_size = std::min(computed_sizes.max_size, max);
  }

  LayoutUnit min =
      ResolveInlineLength(*space, style, min_and_max, style.LogicalMinWidth(),
                          LengthResolveType::kMinSize);
  computed_sizes.min_size = std::max(computed_sizes.min_size, min);
  computed_sizes.max_size = std::max(computed_sizes.max_size, min);

  NGBoxStrut margins =
      ComputeMargins(*space, style, writing_mode, style.Direction());
  computed_sizes.min_size += margins.InlineSum();
  computed_sizes.max_size += margins.InlineSum();
  return computed_sizes;
}

LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const WTF::Optional<MinMaxSize>& min_and_max) {
  if (space.IsFixedSizeInline())
    return space.AvailableSize().inline_size;

  Length logical_width = style.LogicalWidth();
  if (logical_width.IsAuto() && space.IsShrinkToFit())
    logical_width = Length(kFitContent);

  LayoutUnit extent =
      ResolveInlineLength(space, style, min_and_max, logical_width,
                          LengthResolveType::kContentSize);

  Optional<LayoutUnit> max_length;
  if (!style.LogicalMaxWidth().IsMaxSizeNone()) {
    max_length =
        ResolveInlineLength(space, style, min_and_max, style.LogicalMaxWidth(),
                            LengthResolveType::kMaxSize);
  }
  Optional<LayoutUnit> min_length =
      ResolveInlineLength(space, style, min_and_max, style.LogicalMinWidth(),
                          LengthResolveType::kMinSize);
  return ConstrainByMinMax(extent, min_length, max_length);
}

LayoutUnit ComputeBlockSizeForFragment(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    LayoutUnit content_size) {
  if (constraint_space.IsFixedSizeBlock())
    return constraint_space.AvailableSize().block_size;

  LayoutUnit extent =
      ResolveBlockLength(constraint_space, style, style.LogicalHeight(),
                         content_size, LengthResolveType::kContentSize);
  if (extent == NGSizeIndefinite) {
    DCHECK_EQ(content_size, NGSizeIndefinite);
    return extent;
  }
  Optional<LayoutUnit> max_length;
  if (!style.LogicalMaxHeight().IsMaxSizeNone()) {
    max_length =
        ResolveBlockLength(constraint_space, style, style.LogicalMaxHeight(),
                           content_size, LengthResolveType::kMaxSize);
  }
  Optional<LayoutUnit> min_length =
      ResolveBlockLength(constraint_space, style, style.LogicalMinHeight(),
                         content_size, LengthResolveType::kMinSize);
  return ConstrainByMinMax(extent, min_length, max_length);
}

// Computes size for a replaced element.
NGLogicalSize ComputeReplacedSize(const NGLayoutInputNode& node,
                                  const NGConstraintSpace& space,
                                  const Optional<MinMaxSize>& child_minmax) {
  DCHECK(node.IsReplaced());

  NGLogicalSize replaced_size;

  NGLogicalSize default_intrinsic_size;
  Optional<LayoutUnit> computed_inline_size;
  Optional<LayoutUnit> computed_block_size;
  NGLogicalSize aspect_ratio;

  node.IntrinsicSize(&default_intrinsic_size, &computed_inline_size,
                     &computed_block_size, &aspect_ratio);

  const ComputedStyle& style = node.Style();
  Length inline_length = style.LogicalWidth();
  Length block_length = style.LogicalHeight();

  // Compute inline size
  if (inline_length.IsAuto()) {
    if (block_length.IsAuto() || aspect_ratio.IsEmpty()) {
      // Use intrinsic values if inline_size cannot be computed from block_size.
      if (computed_inline_size.has_value())
        replaced_size.inline_size = computed_inline_size.value();
      else
        replaced_size.inline_size = default_intrinsic_size.inline_size;
      replaced_size.inline_size +=
          (ComputeBorders(space, style) + ComputePadding(space, style))
              .InlineSum();
    } else {
      // inline_size is computed from block_size.
      replaced_size.inline_size =
          ResolveBlockLength(space, style, block_length,
                             default_intrinsic_size.block_size,
                             LengthResolveType::kContentSize) *
          aspect_ratio.inline_size / aspect_ratio.block_size;
    }
  } else {
    // inline_size is resolved directly.
    replaced_size.inline_size =
        ResolveInlineLength(space, style, child_minmax, inline_length,
                            LengthResolveType::kContentSize);
  }

  // Compute block size
  if (block_length.IsAuto()) {
    if (inline_length.IsAuto() || aspect_ratio.IsEmpty()) {
      // Use intrinsic values if block_size cannot be computed from inline_size.
      if (computed_block_size.has_value())
        replaced_size.block_size = LayoutUnit(computed_block_size.value());
      else
        replaced_size.block_size = default_intrinsic_size.block_size;
      replaced_size.block_size +=
          (ComputeBorders(space, style) + ComputePadding(space, style))
              .BlockSum();
    } else {
      // block_size is computed from inline_size.
      replaced_size.block_size =
          ResolveInlineLength(space, style, child_minmax, inline_length,
                              LengthResolveType::kContentSize) *
          aspect_ratio.block_size / aspect_ratio.inline_size;
    }
  } else {
    replaced_size.block_size = ResolveBlockLength(
        space, style, block_length, default_intrinsic_size.block_size,
        LengthResolveType::kContentSize);
  }
  return replaced_size;
}

int ResolveUsedColumnCount(int computed_count,
                           LayoutUnit computed_size,
                           LayoutUnit used_gap,
                           LayoutUnit available_size) {
  if (computed_size == NGSizeIndefinite) {
    DCHECK(computed_count);
    return computed_count;
  }
  DCHECK_GT(computed_size, LayoutUnit());
  int count_from_width =
      ((available_size + used_gap) / (computed_size + used_gap)).ToInt();
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
  DCHECK(!style.HasAutoColumnCount() || !style.HasAutoColumnWidth());

  LayoutUnit computed_size =
      style.HasAutoColumnWidth()
          ? NGSizeIndefinite
          : std::max(LayoutUnit(1), LayoutUnit(style.ColumnWidth()));
  int computed_count = style.HasAutoColumnCount() ? 0 : style.ColumnCount();
  LayoutUnit used_gap = ResolveUsedColumnGap(style);
  return ResolveUsedColumnInlineSize(computed_count, computed_size, used_gap,
                                     available_size);
}

LayoutUnit ResolveUsedColumnGap(const ComputedStyle& style) {
  if (style.HasNormalColumnGap())
    return LayoutUnit(style.GetFontDescription().ComputedPixelSize());
  return LayoutUnit(style.ColumnGap());
}

NGBoxStrut ComputeMargins(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style,
                          const NGWritingMode writing_mode,
                          const TextDirection direction) {
  // We don't need these for margin computations
  MinMaxSize empty_sizes;
  // Margins always get computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-margin-width
  NGPhysicalBoxStrut physical_dim;
  physical_dim.left = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.MarginLeft(),
      LengthResolveType::kMarginBorderPaddingSize);
  physical_dim.right = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.MarginRight(),
      LengthResolveType::kMarginBorderPaddingSize);
  physical_dim.top = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.MarginTop(),
      LengthResolveType::kMarginBorderPaddingSize);
  physical_dim.bottom = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.MarginBottom(),
      LengthResolveType::kMarginBorderPaddingSize);
  return physical_dim.ConvertToLogical(writing_mode, direction);
}

NGBoxStrut ComputeBorders(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style) {
  // If we are producing an anonymous fragment (e.g. a column) we shouldn't
  // have any borders.
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();

  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.BorderStartWidth());
  borders.inline_end = LayoutUnit(style.BorderEndWidth());
  borders.block_start = LayoutUnit(style.BorderBeforeWidth());
  borders.block_end = LayoutUnit(style.BorderAfterWidth());
  return borders;
}

NGBoxStrut ComputePadding(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style) {
  // If we are producing an anonymous fragment (e.g. a column) we shouldn't
  // have any padding.
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();

  // We don't need these for padding computations
  MinMaxSize empty_sizes;
  // Padding always gets computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-padding-width
  NGBoxStrut padding;
  padding.inline_start = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.PaddingStart(),
      LengthResolveType::kMarginBorderPaddingSize);
  padding.inline_end = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.PaddingEnd(),
      LengthResolveType::kMarginBorderPaddingSize);
  padding.block_start = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.PaddingBefore(),
      LengthResolveType::kMarginBorderPaddingSize);
  padding.block_end = ResolveInlineLength(
      constraint_space, style, empty_sizes, style.PaddingAfter(),
      LengthResolveType::kMarginBorderPaddingSize);
  return padding;
}

void ApplyAutoMargins(const NGConstraintSpace& constraint_space,
                      const ComputedStyle& style,
                      const LayoutUnit& inline_size,
                      NGBoxStrut* margins) {
  DCHECK(margins) << "Margins cannot be NULL here";
  const LayoutUnit used_space = inline_size + margins->InlineSum();
  const LayoutUnit available_space =
      constraint_space.AvailableSize().inline_size - used_space;
  if (available_space < LayoutUnit())
    return;
  if (style.MarginStart().IsAuto() && style.MarginEnd().IsAuto()) {
    margins->inline_start = available_space / 2;
    margins->inline_end = available_space - margins->inline_start;
  } else if (style.MarginStart().IsAuto()) {
    margins->inline_start = available_space;
  } else if (style.MarginEnd().IsAuto()) {
    margins->inline_end = available_space;
  }
}

LayoutUnit ConstrainByMinMax(LayoutUnit length,
                             Optional<LayoutUnit> min,
                             Optional<LayoutUnit> max) {
  if (max && length > max.value())
    length = max.value();
  if (min && length < min.value())
    length = min.value();
  return length;
}

NGBoxStrut GetScrollbarSizes(const LayoutObject* layout_object) {
  NGPhysicalBoxStrut sizes;
  const ComputedStyle* style = layout_object->Style();
  if (!style->IsOverflowVisible()) {
    const LayoutBox* box = ToLayoutBox(layout_object);
    LayoutUnit vertical = LayoutUnit(box->VerticalScrollbarWidth());
    LayoutUnit horizontal = LayoutUnit(box->HorizontalScrollbarHeight());
    sizes.bottom = horizontal;
    if (style->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
      sizes.left = vertical;
    else
      sizes.right = vertical;
  }
  return sizes.ConvertToLogical(
      FromPlatformWritingMode(style->GetWritingMode()), style->Direction());
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

#include <algorithm>
#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

enum class EBlockAlignment { kStart, kCenter, kEnd };

inline EBlockAlignment BlockAlignment(const ComputedStyle& style,
                                      const ComputedStyle& container_style) {
  bool start_auto = style.MarginStartUsing(container_style).IsAuto();
  bool end_auto = style.MarginEndUsing(container_style).IsAuto();
  if (start_auto || end_auto) {
    if (start_auto)
      return end_auto ? EBlockAlignment::kCenter : EBlockAlignment::kEnd;
    return EBlockAlignment::kStart;
  }

  // If none of the inline margins are auto, look for -webkit- text-align
  // values (which are really about block alignment). These are typically
  // mapped from the legacy "align" HTML attribute.
  switch (container_style.GetTextAlign()) {
    case ETextAlign::kWebkitLeft:
      if (container_style.IsLeftToRightDirection())
        return EBlockAlignment::kStart;
      return EBlockAlignment::kEnd;
    case ETextAlign::kWebkitRight:
      if (container_style.IsLeftToRightDirection())
        return EBlockAlignment::kEnd;
      return EBlockAlignment::kStart;
    case ETextAlign::kWebkitCenter:
      return EBlockAlignment::kCenter;
    default:
      return EBlockAlignment::kStart;
  }
}

}  // anonymous namespace

bool NeedMinMaxSizeForContentContribution(WritingMode mode,
                                          const ComputedStyle& style) {
  // During the intrinsic sizes pass percentages/calc() are defined to behave
  // like 'auto'. As a result we need to calculate the intrinsic sizes for any
  // children with percentages. E.g.
  // <div style="float:left;">
  //   <div style="width:30%;">text text</div>
  // </div>
  if (mode == WritingMode::kHorizontalTb) {
    return style.Width().IsIntrinsicOrAuto() ||
           style.Width().IsPercentOrCalc() || style.MinWidth().IsIntrinsic() ||
           style.MaxWidth().IsIntrinsic();
  }
  return style.Height().IsIntrinsicOrAuto() ||
         style.Height().IsPercentOrCalc() || style.MinHeight().IsIntrinsic() ||
         style.MaxHeight().IsIntrinsic();
}

LayoutUnit ResolveInlineLength(const NGConstraintSpace& constraint_space,
                               const ComputedStyle& style,
                               const NGBoxStrut& border_padding,
                               const base::Optional<MinMaxSize>& min_and_max,
                               const Length& length,
                               LengthResolveType type,
                               LengthResolvePhase phase) {
  DCHECK_GE(constraint_space.AvailableSize().inline_size, LayoutUnit());
  DCHECK_GE(constraint_space.PercentageResolutionInlineSize(), LayoutUnit());
  DCHECK_EQ(constraint_space.GetWritingMode(), style.GetWritingMode());

  if (constraint_space.IsAnonymous())
    return constraint_space.AvailableSize().inline_size;

  if (length.IsMaxSizeNone()) {
    DCHECK_EQ(type, LengthResolveType::kMaxSize);
    return LayoutUnit::Max();
  }

  if (type == LengthResolveType::kMinSize && length.IsAuto())
    return border_padding.InlineSum();

  // Check if we shouldn't resolve a percentage/calc()/-webkit-fill-available
  // if we are in the intrinsic sizes phase.
  if (phase == LengthResolvePhase::kIntrinsic &&
      (length.IsPercentOrCalc() || length.IsFillAvailable())) {
    // min-width/min-height should be "0", i.e. no min limit is applied.
    if (type == LengthResolveType::kMinSize)
      return border_padding.InlineSum();

    // max-width/max-height becomes "infinity", i.e. no max limit is applied.
    if (type == LengthResolveType::kMaxSize)
      return LayoutUnit::Max();
  }

  switch (length.GetType()) {
    case Length::kAuto:
    case Length::kFillAvailable: {
      LayoutUnit content_size = constraint_space.AvailableSize().inline_size;
      NGBoxStrut margins = ComputeMarginsForSelf(constraint_space, style);
      return std::max(border_padding.InlineSum(),
                      content_size - margins.InlineSum());
    }
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated: {
      LayoutUnit percentage_resolution_size =
          constraint_space.PercentageResolutionInlineSize();
      LayoutUnit value =
          MinimumValueForLength(length, percentage_resolution_size);
      if (style.BoxSizing() == EBoxSizing::kContentBox) {
        value += border_padding.InlineSum();
      } else {
        value = std::max(border_padding.InlineSum(), value);
      }
      return value;
    }
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kFitContent: {
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
        NGBoxStrut margins = ComputeMarginsForSelf(constraint_space, style);
        LayoutUnit fill_available =
            std::max(LayoutUnit(), available_size - margins.InlineSum());
        value = min_and_max->ShrinkToFit(fill_available);
      }
      return value;
    }
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
      FALLTHROUGH;
    case Length::kMaxSizeNone:
    default:
      NOTREACHED();
      return border_padding.InlineSum();
  }
}

LayoutUnit ResolveBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit content_size,
    LengthResolveType type,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max) {
  DCHECK_EQ(constraint_space.GetWritingMode(), style.GetWritingMode());

  if (constraint_space.IsAnonymous())
    return content_size;

  if (length.IsMaxSizeNone()) {
    DCHECK_EQ(type, LengthResolveType::kMaxSize);
    return LayoutUnit::Max();
  }

  if (type == LengthResolveType::kMinSize && length.IsAuto())
    return border_padding.BlockSum();

  // Scrollable percentage-sized children of table cells, in the table
  // "measure" phase contribute nothing to the row height measurement.
  // See: https://drafts.csswg.org/css-tables-3/#row-layout
  if (length.IsPercentOrCalc() &&
      constraint_space.TableCellChildLayoutPhase() ==
          NGTableCellChildLayoutPhase::kMeasure &&
      (style.OverflowY() == EOverflow::kAuto ||
       style.OverflowY() == EOverflow::kScroll))
    return border_padding.BlockSum();

  // When the containing block size to resolve against is indefinite, we
  // cannot resolve percentages / calc() / -webkit-fill-available.
  bool size_is_unresolvable = false;
  if (length.IsPercentOrCalc()) {
    LayoutUnit percentage_resolution_block_size =
        opt_percentage_resolution_block_size_for_min_max
            ? *opt_percentage_resolution_block_size_for_min_max
            : constraint_space.PercentageResolutionBlockSize();
    size_is_unresolvable = phase == LengthResolvePhase::kIntrinsic ||
                           percentage_resolution_block_size == NGSizeIndefinite;
  } else if (length.IsFillAvailable()) {
    size_is_unresolvable =
        phase == LengthResolvePhase::kIntrinsic ||
        constraint_space.AvailableSize().block_size == NGSizeIndefinite;
  }
  if (size_is_unresolvable) {
    // min-width/min-height should be "0", i.e. no min limit is applied.
    if (type == LengthResolveType::kMinSize)
      return border_padding.BlockSum();

    // max-width/max-height becomes "infinity", i.e. no max limit is applied.
    if (type == LengthResolveType::kMaxSize)
      return LayoutUnit::Max();

    // width/height becomes "auto", so we can just return the content size.
    DCHECK_EQ(type, LengthResolveType::kContentSize);
    return content_size;
  }

  switch (length.GetType()) {
    case Length::kFillAvailable: {
      LayoutUnit content_size = constraint_space.AvailableSize().block_size;
      NGBoxStrut margins = ComputeMarginsForSelf(constraint_space, style);
      return std::max(border_padding.BlockSum(),
                      content_size - margins.BlockSum());
    }
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated: {
      LayoutUnit percentage_resolution_block_size =
          opt_percentage_resolution_block_size_for_min_max
              ? *opt_percentage_resolution_block_size_for_min_max
              : constraint_space.PercentageResolutionBlockSize();
      LayoutUnit value =
          MinimumValueForLength(length, percentage_resolution_block_size);

      // Percentage-sized children of table cells, in the table "layout" phase,
      // pretend they have box-sizing: border-box.
      // TODO(crbug.com/285744): FF/Edge don't do this. Determine if there
      // would be compat issues for matching their behavior.
      if (style.BoxSizing() == EBoxSizing::kBorderBox ||
          (length.IsPercentOrCalc() &&
           constraint_space.TableCellChildLayoutPhase() ==
               NGTableCellChildLayoutPhase::kLayout)) {
        value = std::max(border_padding.BlockSum(), value);
      } else {
        value += border_padding.BlockSum();
      }
      return value;
    }
    case Length::kAuto:
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kFitContent:
#if DCHECK_IS_ON()
      // Due to how content_size is calculated, it should always include border
      // and padding. We cannot check for this if we are block-fragmented,
      // though, because then the block-start border/padding may be in a
      // different fragmentainer than the block-end border/padding.
      if (content_size != LayoutUnit(-1) &&
          !constraint_space.HasBlockFragmentation())
        DCHECK_GE(content_size, border_padding.BlockSum());
#endif  // DCHECK_IS_ON()
      return content_size;
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
      FALLTHROUGH;
    case Length::kMaxSizeNone:
    default:
      NOTREACHED();
      return border_padding.BlockSum();
  }
}

MinMaxSize ComputeMinAndMaxContentContribution(
    WritingMode parent_writing_mode,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSize>& min_and_max) {
  WritingMode child_writing_mode = style.GetWritingMode();

  // Synthesize a zero-sized constraint space for resolving sizes against.
  NGConstraintSpace space =
      NGConstraintSpaceBuilder(child_writing_mode, child_writing_mode,
                               /* is_new_fc */ false)
          .ToConstraintSpace();

  LayoutUnit content_size =
      min_and_max ? min_and_max->max_size : NGSizeIndefinite;

  MinMaxSize computed_sizes;
  const Length& inline_size = parent_writing_mode == WritingMode::kHorizontalTb
                                  ? style.Width()
                                  : style.Height();
  if (inline_size.IsAuto() || inline_size.IsPercentOrCalc() ||
      inline_size.IsFillAvailable() || inline_size.IsFitContent()) {
    CHECK(min_and_max.has_value());
    computed_sizes = *min_and_max;
  } else {
    if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
      computed_sizes = ResolveInlineLength(
          space, style, border_padding, min_and_max, inline_size,
          LengthResolveType::kContentSize, LengthResolvePhase::kIntrinsic);
    } else {
      computed_sizes = ResolveBlockLength(
          space, style, border_padding, inline_size, content_size,
          LengthResolveType::kContentSize, LengthResolvePhase::kIntrinsic);
    }
  }

  const Length& max_length = parent_writing_mode == WritingMode::kHorizontalTb
                                 ? style.MaxWidth()
                                 : style.MaxHeight();
  LayoutUnit max;
  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    max = ResolveInlineLength(space, style, border_padding, min_and_max,
                              max_length, LengthResolveType::kMaxSize,
                              LengthResolvePhase::kIntrinsic);
  } else {
    max = ResolveBlockLength(space, style, border_padding, max_length,
                             content_size, LengthResolveType::kMaxSize,
                             LengthResolvePhase::kIntrinsic);
  }
  computed_sizes.Constrain(max);

  const Length& min_length = parent_writing_mode == WritingMode::kHorizontalTb
                                 ? style.MinWidth()
                                 : style.MinHeight();
  LayoutUnit min;
  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    min = ResolveInlineLength(space, style, border_padding, min_and_max,
                              min_length, LengthResolveType::kMinSize,
                              LengthResolvePhase::kIntrinsic);
  } else {
    min = ResolveBlockLength(space, style, border_padding, min_length,
                             content_size, LengthResolveType::kMinSize,
                             LengthResolvePhase::kIntrinsic);
  }
  computed_sizes.Encompass(min);

  return computed_sizes;
}

MinMaxSize ComputeMinAndMaxContentContribution(
    const ComputedStyle& parent_style,
    NGLayoutInputNode child,
    const MinMaxSizeInput& input) {
  const ComputedStyle& child_style = child.Style();
  WritingMode parent_writing_mode = parent_style.GetWritingMode();
  WritingMode child_writing_mode = child_style.GetWritingMode();
  LayoutBox* box = child.GetLayoutBox();

  if (box->NeedsPreferredWidthsRecalculation()) {
    // Some objects (when there's an intrinsic ratio) have their min/max inline
    // size affected by the block size of their container. We don't really know
    // whether the containing block of this child did change or is going to
    // change size. However, this is our only opportunity to make sure that it
    // gets its min/max widths calculated.
    box->SetPreferredLogicalWidthsDirty();
  }

  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    if (!box->PreferredLogicalWidthsDirty()) {
      return {box->MinPreferredLogicalWidth(), box->MaxPreferredLogicalWidth()};
    }
    // Tables are special; even if a width is specified, they may end up being
    // sized different. So we just always let the table code handle this.
    // Replaced elements may size themselves using aspect ratios and block
    // sizes, so we pass that on as well.
    if (box->IsTable() || box->IsTablePart() || box->IsLayoutReplaced()) {
      return {box->MinPreferredLogicalWidth(), box->MaxPreferredLogicalWidth()};
    }
  }

  base::Optional<MinMaxSize> minmax;
  if (NeedMinMaxSizeForContentContribution(parent_writing_mode, child_style)) {
    // We need to set up a constraint space with correct fallback available
    // inline size in case of orthogonal children.
    NGConstraintSpace indefinite_constraint_space;
    const NGConstraintSpace* child_constraint_space = nullptr;
    if (!IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
      indefinite_constraint_space =
          CreateIndefiniteConstraintSpaceForChild(parent_style, child);
      child_constraint_space = &indefinite_constraint_space;
    }
    minmax = child.ComputeMinMaxSize(parent_writing_mode, input,
                                     child_constraint_space);
  }
  // Synthesize a zero-sized constraint space for determining the borders, and
  // padding.
  NGConstraintSpace space =
      NGConstraintSpaceBuilder(child_writing_mode, child_writing_mode,
                               /* is_new_fc */ false)
          .ToConstraintSpace();
  NGBoxStrut border_padding =
      ComputeBorders(space, child) + ComputePadding(space, child_style);

  MinMaxSize sizes = ComputeMinAndMaxContentContribution(
      parent_writing_mode, child_style, border_padding, minmax);
  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode))
    box->SetPreferredLogicalWidthsFromNG(sizes);
  return sizes;
}

LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace& space,
    NGLayoutInputNode node,
    const NGBoxStrut& border_padding,
    const MinMaxSize* override_minmax_for_test) {
  if (space.IsFixedSizeInline())
    return space.AvailableSize().inline_size;

  const ComputedStyle& style = node.Style();
  Length logical_width = style.LogicalWidth();
  if (logical_width.IsAuto() && space.IsShrinkToFit())
    logical_width = Length::FitContent();

  LayoutBox* box = node.GetLayoutBox();
  // If we have usable cached min/max intrinsic sizes, use those if we can. They
  // will normally also be constrained to {min,max}-inline-size, but not if
  // percentages are involved. In such cases we'll have to calculate and apply
  // the constraints on our own.
  if (!box->PreferredLogicalWidthsDirty() && !override_minmax_for_test &&
      !style.LogicalMinWidth().IsPercentOrCalc() &&
      !style.LogicalMaxWidth().IsPercentOrCalc()) {
    if (logical_width.IsFitContent()) {
      // This is not as easy as {min, max}.ShrinkToFit() because we also need
      // to subtract inline margins from the available size. The code in
      // ResolveInlineLength knows how to handle that, just call that.

      MinMaxSize min_and_max = {box->MinPreferredLogicalWidth(),
                                box->MaxPreferredLogicalWidth()};
      return ResolveInlineLength(space, style, border_padding, min_and_max,
                                 logical_width, LengthResolveType::kContentSize,
                                 LengthResolvePhase::kLayout);
    }
    if (logical_width.IsMinContent())
      return box->MinPreferredLogicalWidth();
    if (logical_width.IsMaxContent())
      return box->MaxPreferredLogicalWidth();
  }

  base::Optional<MinMaxSize> min_and_max;
  if (NeedMinMaxSize(space, style)) {
    if (override_minmax_for_test) {
      min_and_max = *override_minmax_for_test;
    } else {
      min_and_max = node.ComputeMinMaxSize(
          space.GetWritingMode(),
          MinMaxSizeInput(space.PercentageResolutionBlockSize()), &space);
      // Cache these computed values
      MinMaxSize contribution = ComputeMinAndMaxContentContribution(
          style.GetWritingMode(), style, border_padding, min_and_max);
      box->SetPreferredLogicalWidthsFromNG(contribution);
    }
  }

  LayoutUnit extent = ResolveInlineLength(
      space, style, border_padding, min_and_max, logical_width,
      LengthResolveType::kContentSize, LengthResolvePhase::kLayout);

  LayoutUnit max = ResolveInlineLength(
      space, style, border_padding, min_and_max, style.LogicalMaxWidth(),
      LengthResolveType::kMaxSize, LengthResolvePhase::kLayout);
  LayoutUnit min = ResolveInlineLength(
      space, style, border_padding, min_and_max, style.LogicalMinWidth(),
      LengthResolveType::kMinSize, LengthResolvePhase::kLayout);
  return ConstrainByMinMax(extent, min, max);
}

namespace {

// Computes the block-size for a fragment, ignoring the fixed block-size if set.
LayoutUnit ComputeBlockSizeForFragmentInternal(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    LayoutUnit content_size,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  LayoutUnit extent = ResolveBlockLength(
      constraint_space, style, border_padding, style.LogicalHeight(),
      content_size, LengthResolveType::kContentSize,
      LengthResolvePhase::kLayout,
      opt_percentage_resolution_block_size_for_min_max);
  if (extent == NGSizeIndefinite) {
    DCHECK_EQ(content_size, NGSizeIndefinite);
    return extent;
  }

  LayoutUnit max = ResolveBlockLength(
      constraint_space, style, border_padding, style.LogicalMaxHeight(),
      content_size, LengthResolveType::kMaxSize, LengthResolvePhase::kLayout,
      opt_percentage_resolution_block_size_for_min_max);
  LayoutUnit min = ResolveBlockLength(
      constraint_space, style, border_padding, style.LogicalMinHeight(),
      content_size, LengthResolveType::kMinSize, LengthResolvePhase::kLayout,
      opt_percentage_resolution_block_size_for_min_max);

  return ConstrainByMinMax(extent, min, max);
}

}  // namespace

LayoutUnit ComputeBlockSizeForFragment(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    LayoutUnit content_size) {
  if (constraint_space.IsFixedSizeBlock())
    return constraint_space.AvailableSize().block_size;

  return ComputeBlockSizeForFragmentInternal(constraint_space, style,
                                             border_padding, content_size);
}

// Computes size for a replaced element.
NGLogicalSize ComputeReplacedSize(
    const NGLayoutInputNode& node,
    const NGConstraintSpace& space,
    const base::Optional<MinMaxSize>& child_minmax) {
  DCHECK(node.IsReplaced());
  const ComputedStyle& style = node.Style();

  NGBoxStrut border_padding =
      ComputeBorders(space, node) + ComputePadding(space, style);
  LayoutUnit inline_min = ResolveInlineLength(
      space, style, border_padding, child_minmax, style.LogicalMinWidth(),
      LengthResolveType::kMinSize, LengthResolvePhase::kLayout);
  LayoutUnit inline_max = ResolveInlineLength(
      space, style, border_padding, child_minmax, style.LogicalMaxWidth(),
      LengthResolveType::kMaxSize, LengthResolvePhase::kLayout);
  LayoutUnit block_min =
      ResolveBlockLength(space, style, border_padding, style.LogicalMinHeight(),
                         border_padding.BlockSum(), LengthResolveType::kMinSize,
                         LengthResolvePhase::kLayout);
  LayoutUnit block_max = ResolveBlockLength(
      space, style, border_padding, style.LogicalMaxHeight(), LayoutUnit::Max(),
      LengthResolveType::kMaxSize, LengthResolvePhase::kLayout);

  const Length& inline_length = style.LogicalWidth();
  const Length& block_length = style.LogicalHeight();
  base::Optional<LayoutUnit> replaced_inline;
  if (!inline_length.IsAuto()) {
    replaced_inline = ResolveInlineLength(
        space, style, border_padding, child_minmax, inline_length,
        LengthResolveType::kContentSize, LengthResolvePhase::kLayout);
    replaced_inline =
        ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
  }
  base::Optional<LayoutUnit> replaced_block;
  if (!block_length.IsAuto()) {
    replaced_block = ResolveBlockLength(
        space, style, border_padding, block_length,
        space.AvailableSize().block_size, LengthResolveType::kContentSize,
        LengthResolvePhase::kLayout);
    replaced_block = ConstrainByMinMax(*replaced_block, block_min, block_max);
  }
  if (replaced_inline && replaced_block)
    return NGLogicalSize(*replaced_inline, *replaced_block);

  base::Optional<LayoutUnit> intrinsic_inline;
  base::Optional<LayoutUnit> intrinsic_block;
  NGLogicalSize aspect_ratio;

  node.IntrinsicSize(&intrinsic_inline, &intrinsic_block, &aspect_ratio);
  // Computing intrinsic size is complicated by the fact that
  // intrinsic_inline, intrinsic_block, and aspect_ratio can all
  // be empty independent of each other.
  // https://www.w3.org/TR/CSS22/visudet.html#inline-replaced-width
  if (intrinsic_inline)
    intrinsic_inline = *intrinsic_inline + border_padding.InlineSum();
  else if (aspect_ratio.IsEmpty())
    intrinsic_inline = LayoutUnit(300) + border_padding.InlineSum();
  if (intrinsic_block)
    intrinsic_block = *intrinsic_block + border_padding.BlockSum();
  else if (aspect_ratio.IsEmpty())
    intrinsic_block = LayoutUnit(150) + border_padding.BlockSum();
  if (!intrinsic_inline) {
    if (intrinsic_block) {
      intrinsic_inline = ((*intrinsic_block - border_padding.BlockSum()) *
                          aspect_ratio.inline_size / aspect_ratio.block_size) +
                         border_padding.InlineSum();
    } else {
      intrinsic_inline = space.AvailableSize().inline_size;
    }
  }
  if (!intrinsic_block) {
    intrinsic_block = ((*intrinsic_inline - border_padding.InlineSum()) *
                       aspect_ratio.block_size / aspect_ratio.inline_size) +
                      border_padding.BlockSum();
  }
  // At this point, both intrinsic_inline and intrinsic_block have value.
  // If we only know one length, the other length gets computed wrt one we know.
  auto ComputeBlockFromInline = [&replaced_inline, &aspect_ratio,
                                 &border_padding](LayoutUnit default_block) {
    DCHECK(default_block >= border_padding.BlockSum());
    if (aspect_ratio.IsEmpty())
      return default_block;
    return ((*replaced_inline - border_padding.InlineSum()) *
            aspect_ratio.block_size / aspect_ratio.inline_size) +
           border_padding.BlockSum();
  };

  auto ComputeInlineFromBlock = [&replaced_block, &aspect_ratio,
                                 &border_padding](LayoutUnit default_inline) {
    DCHECK(default_inline >= border_padding.InlineSum());
    if (aspect_ratio.IsEmpty())
      return default_inline;
    return ((*replaced_block - border_padding.BlockSum()) *
            aspect_ratio.inline_size / aspect_ratio.block_size) +
           border_padding.InlineSum();
  };
  if (replaced_inline) {
    DCHECK(!replaced_block);
    replaced_block = ComputeBlockFromInline(*intrinsic_block);
    replaced_block = ConstrainByMinMax(*replaced_block, block_min, block_max);
  } else if (replaced_block) {
    DCHECK(!replaced_inline);
    replaced_inline = ComputeInlineFromBlock(*intrinsic_inline);
    replaced_inline =
        ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
  } else {
    // If both lengths are unknown, they get defined by intrinsic values.
    DCHECK(!replaced_inline);
    DCHECK(!replaced_block);
    replaced_inline = *intrinsic_inline;
    replaced_block = *intrinsic_block;
    // If lengths are constrained, keep aspect ratio.
    // The side that shrank the most defines the other side.
    LayoutUnit constrained_inline =
        ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
    LayoutUnit constrained_block =
        ConstrainByMinMax(*replaced_block, block_min, block_max);
    if (constrained_inline != replaced_inline ||
        constrained_block != replaced_block) {
      LayoutUnit inline_ratio =
          (*replaced_inline - border_padding.InlineSum()) == LayoutUnit()
              ? LayoutUnit::Max()
              : (constrained_inline - border_padding.InlineSum()) /
                    (*replaced_inline - border_padding.InlineSum());
      LayoutUnit block_ratio =
          (*replaced_block - border_padding.BlockSum()) == LayoutUnit()
              ? LayoutUnit::Max()
              : (constrained_block - border_padding.BlockSum()) /
                    (*replaced_block - border_padding.BlockSum());

      // The following implements spec table from section 10.4 at
      // https://www.w3.org/TR/CSS22/visudet.html#min-max-widths
      // Translating specs to code:
      // inline_ratio < 1 => w > max_width
      // inline_ratio > 1 => w < min_width
      // block_ratio < 1 => h > max_height
      // block_ratio > 1 => h < min_height
      LayoutUnit one_unit(1);
      if (inline_ratio != one_unit || block_ratio != one_unit) {
        if ((inline_ratio < one_unit && block_ratio > one_unit) ||
            (inline_ratio > one_unit && block_ratio < one_unit)) {
          // Constraints caused us to grow in one dimension and shrink in the
          // other. Use both constrained sizes.
          replaced_inline = constrained_inline;
          replaced_block = constrained_block;
        } else if (block_ratio == one_unit ||
                   (inline_ratio < one_unit && inline_ratio <= block_ratio) ||
                   (inline_ratio > one_unit && inline_ratio >= block_ratio)) {
          // The inline size got constrained more extremely than the block size.
          // Use constrained inline size, re-calculate block size from aspect
          // ratio.
          replaced_inline = constrained_inline;
          replaced_block = ComputeBlockFromInline(constrained_block);
        } else {
          // The block size got constrained more extremely than the inline size.
          // Use constrained block size, re-calculate inline size from aspect
          // ratio.
          replaced_block = constrained_block;
          replaced_inline = ComputeInlineFromBlock(constrained_inline);
        }
      }
    }
  }
  return NGLogicalSize(*replaced_inline, *replaced_block);
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

int ResolveUsedColumnCount(LayoutUnit available_size,
                           const ComputedStyle& style) {
  LayoutUnit computed_column_inline_size =
      style.HasAutoColumnWidth()
          ? NGSizeIndefinite
          : std::max(LayoutUnit(1), LayoutUnit(style.ColumnWidth()));
  LayoutUnit gap = ResolveUsedColumnGap(available_size, style);
  int computed_count = style.ColumnCount();
  return ResolveUsedColumnCount(computed_count, computed_column_inline_size,
                                gap, available_size);
}

LayoutUnit ResolveUsedColumnInlineSize(int computed_count,
                                       LayoutUnit computed_size,
                                       LayoutUnit used_gap,
                                       LayoutUnit available_size) {
  int used_count = ResolveUsedColumnCount(computed_count, computed_size,
                                          used_gap, available_size);
  return std::max(((available_size + used_gap) / used_count) - used_gap,
                  LayoutUnit());
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
  LayoutUnit used_gap = ResolveUsedColumnGap(available_size, style);
  return ResolveUsedColumnInlineSize(computed_count, computed_size, used_gap,
                                     available_size);
}

LayoutUnit ResolveUsedColumnGap(LayoutUnit available_size,
                                const ComputedStyle& style) {
  if (style.ColumnGap().IsNormal())
    return LayoutUnit(style.GetFontDescription().ComputedPixelSize());
  return ValueForLength(style.ColumnGap().GetLength(), available_size);
}

NGPhysicalBoxStrut ComputePhysicalMargins(
    const ComputedStyle& style,
    LayoutUnit percentage_resolution_size) {
  if (!style.HasMargin())
    return NGPhysicalBoxStrut();

  return {
      MinimumValueForLength(style.MarginTop(), percentage_resolution_size),
      MinimumValueForLength(style.MarginRight(), percentage_resolution_size),
      MinimumValueForLength(style.MarginBottom(), percentage_resolution_size),
      MinimumValueForLength(style.MarginLeft(), percentage_resolution_size)};
}

NGBoxStrut ComputeMarginsFor(const NGConstraintSpace& constraint_space,
                             const ComputedStyle& style,
                             const NGConstraintSpace& compute_for) {
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLogical(compute_for.GetWritingMode(), compute_for.Direction());
}

NGBoxStrut ComputeMinMaxMargins(const ComputedStyle& parent_style,
                                NGLayoutInputNode child) {
  // An inline child just produces line-boxes which don't have any margins.
  if (child.IsInline())
    return NGBoxStrut();

  const Length& inline_start_margin_length =
      child.Style().MarginStartUsing(parent_style);
  const Length& inline_end_margin_length =
      child.Style().MarginEndUsing(parent_style);

  // TODO(ikilpatrick): We may want to re-visit calculated margins at some
  // point. Currently "margin-left: calc(10px + 50%)" will resolve to 0px, but
  // 10px would be more correct, (as percentages resolve to zero).
  NGBoxStrut margins;
  if (inline_start_margin_length.IsFixed())
    margins.inline_start = LayoutUnit(inline_start_margin_length.Value());
  if (inline_end_margin_length.IsFixed())
    margins.inline_end = LayoutUnit(inline_end_margin_length.Value());

  return margins;
}

namespace {

NGBoxStrut ComputeBordersInternal(const ComputedStyle& style) {
  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.BorderStartWidth());
  borders.inline_end = LayoutUnit(style.BorderEndWidth());
  borders.block_start = LayoutUnit(style.BorderBeforeWidth());
  borders.block_end = LayoutUnit(style.BorderAfterWidth());
  return borders;
}

}  // namespace

NGBoxStrut ComputeBorders(const NGConstraintSpace& constraint_space,
                          const NGLayoutInputNode node) {
  // If we are producing an anonymous fragment (e.g. a column), it has no
  // borders, padding or scrollbars. Using the ones from the container can only
  // cause trouble.
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();

  // If we are a table cell we just access the values set by the parent table
  // layout as border may be collapsed etc.
  if (node.IsTableCell()) {
    const LayoutBox* box = node.GetLayoutBox();
    return NGBoxStrut(box->BorderStart(), box->BorderEnd(), box->BorderBefore(),
                      box->BorderAfter());
  }
  return ComputeBordersInternal(node.Style());
}

NGBoxStrut ComputeBordersForInline(const ComputedStyle& style) {
  return ComputeBordersInternal(style);
}

NGBoxStrut ComputeBordersForTest(const ComputedStyle& style) {
  return ComputeBordersInternal(style);
}

NGBoxStrut ComputeIntrinsicPadding(const NGConstraintSpace& constraint_space,
                                   const NGLayoutInputNode node) {
  if (constraint_space.IsAnonymous() || !node.IsTableCell())
    return NGBoxStrut();

  // At the moment we just access the values set by the parent table layout.
  // Once we have a NGTableLayoutAlgorithm this should pass the intrinsic
  // padding via the constraint space object.

  // TODO(karlo): intrinsic padding can sometimes be negative; that seems
  // insane, but works in the old code; in NG it trips DCHECKs.
  return {LayoutUnit(), LayoutUnit(), node.IntrinsicPaddingBlockStart(),
          node.IntrinsicPaddingBlockEnd()};
}

NGBoxStrut ComputePadding(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style) {
  // If we are producing an anonymous fragment (e.g. a column) we shouldn't
  // have any padding.
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();

  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  NGBoxStrut padding = {
      MinimumValueForLength(style.PaddingStart(), percentage_resolution_size),
      MinimumValueForLength(style.PaddingEnd(), percentage_resolution_size),
      MinimumValueForLength(style.PaddingBefore(), percentage_resolution_size),
      MinimumValueForLength(style.PaddingAfter(), percentage_resolution_size)};

  if (style.Display() == EDisplay::kTableCell) {
    // Compatibility hack to mach legacy layout. Legacy layout floors padding on
    // the block sides, but not on the inline sides. o.O
    padding.block_start = LayoutUnit(padding.block_start.Floor());
    padding.block_end = LayoutUnit(padding.block_end.Floor());
  }

  return padding;
}


bool NeedsInlineSizeToResolveLineLeft(const ComputedStyle& style,
                                      const ComputedStyle& container_style) {
  // In RTL, there's no block alignment where we can guarantee that line-left
  // doesn't depend on the inline size of a fragment.
  if (IsRtl(container_style.Direction()))
    return true;

  return BlockAlignment(style, container_style) != EBlockAlignment::kStart;
}

void ResolveInlineMargins(const ComputedStyle& style,
                          const ComputedStyle& container_style,
                          LayoutUnit available_inline_size,
                          LayoutUnit inline_size,
                          NGBoxStrut* margins) {
  DCHECK(margins) << "Margins cannot be NULL here";
  const LayoutUnit used_space = inline_size + margins->InlineSum();
  const LayoutUnit available_space = available_inline_size - used_space;
  if (available_space > LayoutUnit()) {
    EBlockAlignment alignment = BlockAlignment(style, container_style);
    if (alignment == EBlockAlignment::kCenter)
      margins->inline_start += available_space / 2;
    else if (alignment == EBlockAlignment::kEnd)
      margins->inline_start += available_space;
  }
  margins->inline_end =
      available_inline_size - inline_size - margins->inline_start;
}

LayoutUnit LineOffsetForTextAlign(ETextAlign text_align,
                                  TextDirection direction,
                                  LayoutUnit space_left) {
  bool is_ltr = IsLtr(direction);
  if (text_align == ETextAlign::kStart || text_align == ETextAlign::kJustify)
    text_align = is_ltr ? ETextAlign::kLeft : ETextAlign::kRight;
  else if (text_align == ETextAlign::kEnd)
    text_align = is_ltr ? ETextAlign::kRight : ETextAlign::kLeft;

  switch (text_align) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft: {
      // The direction of the block should determine what happens with wide
      // lines. In particular with RTL blocks, wide lines should still spill
      // out to the left.
      if (is_ltr)
        return LayoutUnit();
      return space_left.ClampPositiveToZero();
    }
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight: {
      // In RTL, trailing spaces appear on the left of the line.
      if (UNLIKELY(!is_ltr))
        return space_left;
      // Wide lines spill out of the block based off direction.
      // So even if text-align is right, if direction is LTR, wide lines
      // should overflow out of the right side of the block.
      if (space_left > LayoutUnit())
        return space_left;
      return LayoutUnit();
    }
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter: {
      if (is_ltr)
        return (space_left / 2).ClampNegativeToZero();
      // In RTL, trailing spaces appear on the left of the line.
      if (space_left > LayoutUnit())
        return (space_left / 2).ClampNegativeToZero();
      // In RTL, wide lines should spill out to the left, same as kRight.
      return space_left;
    }
    default:
      NOTREACHED();
      return LayoutUnit();
  }
}

LayoutUnit InlineOffsetForTextAlign(const ComputedStyle& container_style,
                                    LayoutUnit space_left) {
  TextDirection direction = container_style.Direction();
  LayoutUnit line_offset = LineOffsetForTextAlign(
      container_style.GetTextAlign(), direction, space_left);
  return IsLtr(direction) ? line_offset : space_left - line_offset;
}

bool ClampScrollbarToContentBox(NGBoxStrut* scrollbars,
                                LayoutUnit content_box_inline_size) {
  DCHECK(scrollbars->InlineSum());
  if (scrollbars->InlineSum() <= content_box_inline_size)
    return false;
  if (scrollbars->inline_end) {
    DCHECK(!scrollbars->inline_start);
    scrollbars->inline_end = content_box_inline_size;
  } else {
    DCHECK(scrollbars->inline_start);
    scrollbars->inline_start = content_box_inline_size;
  }
  return true;
}

NGBoxStrut CalculateBorderScrollbarPadding(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode node) {
  // If we are producing an anonymous fragment (e.g. a column), it has no
  // borders, padding or scrollbars. Using the ones from the container can only
  // cause trouble.
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();
  return ComputeBorders(constraint_space, node) +
         ComputePadding(constraint_space, node.Style()) +
         ComputeIntrinsicPadding(constraint_space, node) +
         node.GetScrollbarSizes();
}

NGLogicalSize CalculateBorderBoxSize(const NGConstraintSpace& constraint_space,
                                     const NGBlockNode& node,
                                     const NGBoxStrut& border_padding,
                                     LayoutUnit block_content_size) {
  // If we have a percentage size, we need to set the
  // HasPercentHeightDescendants flag correctly so that flexboz knows it may
  // need to redo layout and can also do some performance optimizations.
  if (node.Style().LogicalHeight().IsPercentOrCalc() ||
      node.Style().LogicalMinHeight().IsPercentOrCalc() ||
      node.Style().LogicalMaxHeight().IsPercentOrCalc() ||
      (node.GetLayoutBox()->IsFlexItemIncludingNG() &&
       node.Style().FlexBasis().IsPercentOrCalc())) {
    // This call has the side-effect of setting HasPercentHeightDescendants
    // correctly.
    node.GetLayoutBox()->ComputePercentageLogicalHeight(Length::Percent(0));
  }

  return NGLogicalSize(
      ComputeInlineSizeForFragment(constraint_space, node, border_padding),
      ComputeBlockSizeForFragment(constraint_space, node.Style(),
                                  border_padding, block_content_size));
}

NGLogicalSize ShrinkAvailableSize(NGLogicalSize size, const NGBoxStrut& inset) {
  DCHECK_NE(size.inline_size, NGSizeIndefinite);
  size.inline_size -= inset.InlineSum();
  size.inline_size = std::max(size.inline_size, LayoutUnit());

  if (size.block_size != NGSizeIndefinite) {
    size.block_size -= inset.BlockSum();
    size.block_size = std::max(size.block_size, LayoutUnit());
  }

  return size;
}

LayoutUnit CalculateDefaultBlockSize(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const NGBoxStrut& border_scrollbar_padding) {
  // In quirks mode, html and body elements will completely fill the ICB, block
  // percentages should resolve against this size.
  if (node.IsQuirkyAndFillsViewport()) {
    LayoutUnit block_size = space.AvailableSize().block_size;
    block_size -= ComputeMarginsForSelf(space, node.Style()).BlockSum();
    return std::max(block_size.ClampNegativeToZero(),
                    border_scrollbar_padding.BlockSum());
  }
  return NGSizeIndefinite;
}

namespace {

// Implements the common part of the child percentage size calculation. Deals
// with how percentages are propagated from parent to child in quirks mode.
NGLogicalSize AdjustChildPercentageSizeForQuirksAndFlex(
    const NGConstraintSpace& space,
    const NGBlockNode node,
    NGLogicalSize child_percentage_size,
    LayoutUnit parent_percentage_block_size) {
  // Flex items may have a fixed block-size, but children shouldn't resolve
  // their percentages against this.
  if (space.IsFixedSizeBlock() && !space.FixedSizeBlockIsDefinite()) {
    DCHECK(node.IsFlexItem());
    child_percentage_size.block_size = NGSizeIndefinite;
    return child_percentage_size;
  }

  // In quirks mode the percentage resolution height is passed from parent to
  // child.
  // https://quirks.spec.whatwg.org/#the-percentage-height-calculation-quirk
  if (child_percentage_size.block_size == NGSizeIndefinite &&
      node.UseParentPercentageResolutionBlockSizeForChildren())
    child_percentage_size.block_size = parent_percentage_block_size;

  return child_percentage_size;
}

}  // namespace

NGLogicalSize CalculateChildPercentageSize(
    const NGConstraintSpace& space,
    const NGBlockNode node,
    const NGLogicalSize& child_available_size) {
  // Anonymous block or spaces should pass the percent size straight through.
  if (space.IsAnonymous() || node.IsAnonymousBlock())
    return space.PercentageResolutionSize();

  NGLogicalSize child_percentage_size = child_available_size;

  bool is_table_cell_in_measure_phase =
      node.IsTableCell() && !space.IsFixedSizeBlock();

  // Table cells which are measuring their content, force their children to
  // have an indefinite percentage resolution size.
  if (is_table_cell_in_measure_phase) {
    child_percentage_size.block_size = NGSizeIndefinite;
    return child_percentage_size;
  }

  // Table cell children don't apply the "percentage-quirk". I.e. if their
  // percentage resolution block-size is indefinite, they don't pass through
  // their parent's percentage resolution block-size.
  if (space.TableCellChildLayoutPhase() !=
      NGTableCellChildLayoutPhase::kNotTableCellChild)
    return child_percentage_size;

  return AdjustChildPercentageSizeForQuirksAndFlex(
      space, node, child_percentage_size,
      space.PercentageResolutionBlockSize());
}

NGLogicalSize CalculateReplacedChildPercentageSize(
    const NGConstraintSpace& space,
    const NGBlockNode node,
    NGLogicalSize border_box_size,
    const NGBoxStrut& border_scrollbar_padding,
    const NGBoxStrut& border_padding) {
  // Anonymous block or spaces should pass the percent size straight through.
  if (space.IsAnonymous() || node.IsAnonymousBlock())
    return space.ReplacedPercentageResolutionSize();

  bool has_resolvable_block_size = !node.Style().LogicalHeight().IsAuto() ||
                                   !node.Style().LogicalMinHeight().IsAuto();

  bool is_table_cell_in_layout_phase =
      node.IsTableCell() && space.IsFixedSizeBlock();

  // Table cells in the "layout" phase have a fixed block-size. However
  // replaced children should resolve their percentages against the size given
  // in the "measure" phase.
  //
  // To handle this we recalculate the border-box block-size, ignoring the
  // fixed size constraint.
  if (is_table_cell_in_layout_phase && has_resolvable_block_size) {
    border_box_size.block_size = ComputeBlockSizeForFragmentInternal(
        space, node.Style(), border_padding, NGSizeIndefinite);
  }

  NGLogicalSize child_percentage_size =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding);

  return AdjustChildPercentageSizeForQuirksAndFlex(
      space, node, child_percentage_size,
      space.ReplacedPercentageResolutionBlockSize());
}

LayoutUnit CalculateChildPercentageBlockSizeForMinMax(
    const NGConstraintSpace& space,
    const NGBlockNode node,
    const NGBoxStrut& border_padding,
    LayoutUnit parent_percentage_block_size) {
  // Anonymous block or spaces should pass the percent size straight through.
  if (space.IsAnonymous() || node.IsAnonymousBlock())
    return parent_percentage_block_size;

  LayoutUnit block_size = ComputeBlockSizeForFragmentInternal(
      space, node.Style(), border_padding,
      CalculateDefaultBlockSize(space, node, border_padding),
      &parent_percentage_block_size);

  LayoutUnit child_percentage_block_size =
      block_size == NGSizeIndefinite
          ? NGSizeIndefinite
          : (block_size - border_padding.BlockSum()).ClampNegativeToZero();

  if (child_percentage_block_size == NGSizeIndefinite &&
      node.UseParentPercentageResolutionBlockSizeForChildren())
    child_percentage_block_size = parent_percentage_block_size;

  return child_percentage_block_size;
}

}  // namespace blink

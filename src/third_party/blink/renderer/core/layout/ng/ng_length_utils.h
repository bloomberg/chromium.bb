// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LENGTH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LENGTH_UTILS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {
class ComputedStyle;
class Length;
struct MinMaxSizesInput;
class NGConstraintSpace;
class NGBlockNode;
class NGLayoutInputNode;

inline bool NeedMinMaxSize(const ComputedStyle& style) {
  return style.LogicalWidth().IsContentOrIntrinsic() ||
         style.LogicalMinWidth().IsContentOrIntrinsic() ||
         style.LogicalMaxWidth().IsContentOrIntrinsic();
}

LayoutUnit InlineSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                     double logical_aspect_ratio,
                                     EBoxSizing box_sizing,
                                     LayoutUnit block_size);
CORE_EXPORT LayoutUnit
InlineSizeFromAspectRatio(const NGBoxStrut& border_padding,
                          const LogicalSize& aspect_ratio,
                          EBoxSizing box_sizing,
                          LayoutUnit block_size);

LayoutUnit BlockSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                    double logical_aspect_ratio,
                                    EBoxSizing box_sizing,
                                    LayoutUnit inline_size);
LayoutUnit BlockSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                    const LogicalSize& aspect_ratio,
                                    EBoxSizing box_sizing,
                                    LayoutUnit inline_size);

// Returns if the given |Length| is unresolvable, e.g. the length is %-based
// and resolving against an indefinite size. For block lengths we also consider
// 'auto', 'min-content', 'max-content', 'fit-content' and 'none' (for
// max-block-size) as unresolvable.
CORE_EXPORT bool InlineLengthUnresolvable(const NGConstraintSpace&,
                                          const Length&);
CORE_EXPORT bool BlockLengthUnresolvable(
    const NGConstraintSpace&,
    const Length&,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Resolve means translate a Length to a LayoutUnit.
//  - |NGConstraintSpace| the information given by the parent, e.g. the
//    available-size.
//  - |ComputedStyle| the style of the node.
//  - |border_padding| the resolved border, and padding of the node.
//  - |MinMaxSizes| is only used when the length is intrinsic (fit-content).
//  - |Length| is the length to resolve.
CORE_EXPORT LayoutUnit
ResolveInlineLengthInternal(const NGConstraintSpace&,
                            const ComputedStyle&,
                            const NGBoxStrut& border_padding,
                            const base::Optional<MinMaxSizes>&,
                            const Length&);

// Same as ResolveInlineLengthInternal, except here |intrinsic_size| roughly
// plays the part of |MinMaxSizes|.
CORE_EXPORT LayoutUnit ResolveBlockLengthInternal(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    const Length&,
    LayoutUnit intrinsic_size,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Used for resolving min inline lengths, (|ComputedStyle::MinLogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMinInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length) {
  if (LIKELY(length.IsAuto() ||
             InlineLengthUnresolvable(constraint_space, length)))
    return border_padding.InlineSum();

  base::Optional<MinMaxSizes> min_max_sizes;
  if (length.IsContentOrIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

template <>
inline LayoutUnit ResolveMinInlineLength<base::Optional<MinMaxSizes>>(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length) {
  if (LIKELY(length.IsAuto() ||
             InlineLengthUnresolvable(constraint_space, length)))
    return border_padding.InlineSum();

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

// Used for resolving max inline lengths, (|ComputedStyle::MaxLogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMaxInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length) {
  if (LIKELY(length.IsNone() ||
             InlineLengthUnresolvable(constraint_space, length)))
    return LayoutUnit::Max();

  base::Optional<MinMaxSizes> min_max_sizes;
  if (length.IsContentOrIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

template <>
inline LayoutUnit ResolveMaxInlineLength<base::Optional<MinMaxSizes>>(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length) {
  if (LIKELY(length.IsNone() ||
             InlineLengthUnresolvable(constraint_space, length)))
    return LayoutUnit::Max();

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

// Used for resolving main inline lengths, (|ComputedStyle::LogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMainInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length) {
  DCHECK(!length.IsAuto());
  base::Optional<MinMaxSizes> min_max_sizes;
  if (length.IsContentOrIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

template <>
inline LayoutUnit ResolveMainInlineLength<base::Optional<MinMaxSizes>>(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length) {
  DCHECK(!length.IsAuto());
  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

// Used for resolving min block lengths, (|ComputedStyle::MinLogicalHeight|).
inline LayoutUnit ResolveMinBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (LIKELY(BlockLengthUnresolvable(
          constraint_space, length,
          opt_percentage_resolution_block_size_for_min_max)))
    return border_padding.BlockSum();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, kIndefiniteSize,
      available_block_size_adjustment,
      opt_percentage_resolution_block_size_for_min_max);
}

// Used for resolving max block lengths, (|ComputedStyle::MaxLogicalHeight|).
inline LayoutUnit ResolveMaxBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (LIKELY(BlockLengthUnresolvable(
          constraint_space, length,
          opt_percentage_resolution_block_size_for_min_max)))
    return LayoutUnit::Max();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, kIndefiniteSize,
      available_block_size_adjustment,
      opt_percentage_resolution_block_size_for_min_max);
}

// Used for resolving main block lengths, (|ComputedStyle::LogicalHeight|).
inline LayoutUnit ResolveMainBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit intrinsic_size,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  DCHECK(!length.IsAuto());
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(
                   constraint_space, length,
                   opt_percentage_resolution_block_size_for_min_max)))
    return intrinsic_size;

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, intrinsic_size,
      available_block_size_adjustment,
      opt_percentage_resolution_block_size_for_min_max);
}

template <typename IntrinsicBlockSizeFunc>
inline LayoutUnit ResolveMainBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    const IntrinsicBlockSizeFunc& intrinsic_block_size_func,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  DCHECK(!length.IsAuto());
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(
                   constraint_space, length,
                   opt_percentage_resolution_block_size_for_min_max)))
    return intrinsic_block_size_func();

  LayoutUnit intrinsic_block_size = kIndefiniteSize;
  if (length.IsContentOrIntrinsic())
    intrinsic_block_size = intrinsic_block_size_func();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, intrinsic_block_size,
      available_block_size_adjustment,
      opt_percentage_resolution_block_size_for_min_max);
}

// For the given |child|, computes the min and max content contribution
// (https://drafts.csswg.org/css-sizing/#contributions).
//
// This is similar to ComputeInlineSizeForFragment except that it does not
// require a constraint space (percentage sizes as well as auto margins compute
// to zero) and an auto inline-size resolves to the respective min/max content
// size.
//
// Additoinally, the min/max contribution includes the inline margins. Because
// content contributions are commonly needed by a block's parent, we also take
// a writing-mode here so we can compute this in the parent's coordinate system.
//
// Note that if the writing mode of the child is orthogonal to that of the
// parent, we'll still return the inline min/max contribution in the writing
// mode of the parent (i.e. typically something based on the preferred *block*
// size of the child).
MinMaxSizesResult ComputeMinAndMaxContentContribution(
    const ComputedStyle& parent_style,
    const NGBlockNode& child,
    const MinMaxSizesInput&);

// Similar to |ComputeMinAndMaxContentContribution| but ignores the parent
// writing-mode, and instead computes the contribution relative to |child|'s
// own writing-mode.
MinMaxSizesResult ComputeMinAndMaxContentContributionForSelf(
    const NGBlockNode& child,
    const MinMaxSizesInput&);

// Used for unit-tests.
CORE_EXPORT MinMaxSizes
ComputeMinAndMaxContentContributionForTest(WritingMode writing_mode,
                                           const NGBlockNode&,
                                           const MinMaxSizes&);

// Computes the min-block-size and max-block-size values for a node.
MinMaxSizes ComputeMinMaxBlockSizes(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

MinMaxSizes ComputeTransferredMinMaxInlineSizes(
    const LogicalSize& ratio,
    const MinMaxSizes& block_min_max,
    const NGBoxStrut& border_padding,
    const EBoxSizing sizing);

// Computes the transferred min/max inline sizes from the min/max block
// sizes and the aspect ratio.
// This will compute the min/max block sizes for you, but it only works with
// styles that have a LogicalAspectRatio. It doesn't work if the aspect ratio is
// coming from a replaced element.
MinMaxSizes ComputeMinMaxInlineSizesFromAspectRatio(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding);

// Tries to compute the inline size of a node from its block size and
// aspect ratio. If there is no aspect ratio or the block size is indefinite,
// returns kIndefiniteSize.
// This function will not do the right thing when used on a replaced element.
// block_size can be specified to base the calculation off of that size
// instead of calculating it.
LayoutUnit ComputeInlineSizeFromAspectRatio(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    bool should_be_considered_as_replaced,
    LayoutUnit block_size = kIndefiniteSize);

template <typename MinMaxSizesFunc>
MinMaxSizes ComputeMinMaxInlineSizes(const NGConstraintSpace& space,
                                     const ComputedStyle& style,
                                     const NGBoxStrut& border_padding,
                                     const MinMaxSizesFunc& min_max_sizes_func,
                                     const Length* opt_min_length = nullptr) {
  const Length& min_length =
      opt_min_length ? *opt_min_length : style.LogicalMinWidth();
  MinMaxSizes sizes = {
      ResolveMinInlineLength(space, style, border_padding, min_max_sizes_func,
                             min_length),
      ResolveMaxInlineLength(space, style, border_padding, min_max_sizes_func,
                             style.LogicalMaxWidth())};

  // This implements the transferred min/max sizes per:
  // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-size-transfers
  if (!style.AspectRatio().IsAuto() &&
      BlockLengthUnresolvable(space, style.LogicalHeight())) {
    MinMaxSizes transferred_sizes =
        ComputeMinMaxInlineSizesFromAspectRatio(space, style, border_padding);
    sizes.min_size = std::max(
        sizes.min_size, std::min(transferred_sizes.min_size, sizes.max_size));
    sizes.max_size = std::min(sizes.max_size, transferred_sizes.max_size);
  }

  sizes.max_size = std::max(sizes.max_size, sizes.min_size);
  return sizes;
}

// Returns inline size of the node's border box by resolving the computed value
// in style.logicalWidth (Length) to a layout unit, adding border and padding,
// then constraining the result by the resolved min logical width and max
// logical width from the ComputedStyle object. Calls Node::ComputeMinMaxSize
// if needed.
// |override_min_max_sizes_for_test| is provided *solely* for use by unit tests.
CORE_EXPORT LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace&,
    NGLayoutInputNode,
    const NGBoxStrut& border_padding,
    const MinMaxSizes* override_min_max_sizes_for_test = nullptr);

// Similar to |ComputeInlineSizeForFragment| but for determining the "used"
// inline-size for a table fragment. See:
// https://drafts.csswg.org/css-tables-3/#used-width-of-table
CORE_EXPORT LayoutUnit ComputeUsedInlineSizeForTableFragment(
    const NGConstraintSpace& space,
    NGLayoutInputNode node,
    const NGBoxStrut& border_padding,
    const MinMaxSizes& table_grid_min_max_sizes);

// Same as ComputeInlineSizeForFragment, but uses height instead of width.
// |inline_size| is necessary to compute the block size when an aspect ratio
// is in use.
// |available_block_size_adjustment| is needed for <table> layout. When a table
// is under an extrinsic constraint (being stretched by its parent, or forced
// to a fixed block-size), we need to subtract the block-size of all the
// <caption>s from the available block-size.
CORE_EXPORT LayoutUnit ComputeBlockSizeForFragment(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    base::Optional<LayoutUnit> inline_size,
    bool should_be_considered_as_replaced,
    LayoutUnit available_block_size_adjustment = LayoutUnit());

// Calculates default content size for html and body elements in quirks mode.
// Returns |kIndefiniteSize| in all other cases.
CORE_EXPORT LayoutUnit
CalculateDefaultBlockSize(const NGConstraintSpace& space,
                          const NGBlockNode& node,
                          const NGBoxStrut& border_scrollbar_padding);

// Intrinsic size for replaced elements is computed as:
// - |out_replaced_size| intrinsic size of the element. It might have no value.
// - |out_aspect_ratio| only set if out_replaced_size is empty.
//   If out_replaced_size is not empty, that is the aspect ratio.
// This routine will return precisely one of |out_replaced_size| and
// |out_aspect_ratio|. If |out_aspect_ratio| is filled in, both inline and block
// componenents will be non-zero.
CORE_EXPORT void ComputeReplacedSize(
    const NGBlockNode&,
    const NGConstraintSpace&,
    const base::Optional<MinMaxSizes>&,
    base::Optional<LogicalSize>* out_replaced_size,
    base::Optional<LogicalSize>* out_aspect_ratio);

// Based on available inline size, CSS computed column-width, CSS computed
// column-count and CSS used column-gap, return CSS used column-count.
// If computed column-count is auto, pass 0 as |computed_count|.
CORE_EXPORT int ResolveUsedColumnCount(int computed_count,
                                       LayoutUnit computed_size,
                                       LayoutUnit used_gap,
                                       LayoutUnit available_size);
CORE_EXPORT int ResolveUsedColumnCount(LayoutUnit available_size,
                                       const ComputedStyle&);

// Based on available inline size, CSS computed column-width, CSS computed
// column-count and CSS used column-gap, return CSS used column-width.
CORE_EXPORT LayoutUnit ResolveUsedColumnInlineSize(int computed_count,
                                                   LayoutUnit computed_size,
                                                   LayoutUnit used_gap,
                                                   LayoutUnit available_size);
CORE_EXPORT LayoutUnit ResolveUsedColumnInlineSize(LayoutUnit available_size,
                                                   const ComputedStyle&);

CORE_EXPORT LayoutUnit ResolveUsedColumnGap(LayoutUnit available_size,
                                            const ComputedStyle&);

CORE_EXPORT LayoutUnit ColumnInlineProgression(LayoutUnit available_size,
                                               const ComputedStyle&);
// Compute physical margins.
CORE_EXPORT NGPhysicalBoxStrut
ComputePhysicalMargins(const ComputedStyle&,
                       LayoutUnit percentage_resolution_size);

inline NGPhysicalBoxStrut ComputePhysicalMargins(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size);
}

// Compute margins for the specified NGConstraintSpace.
CORE_EXPORT NGBoxStrut ComputeMarginsFor(const NGConstraintSpace&,
                                         const ComputedStyle&,
                                         const NGConstraintSpace& compute_for);

inline NGBoxStrut ComputeMarginsFor(
    const ComputedStyle& child_style,
    LayoutUnit percentage_resolution_size,
    WritingDirectionMode container_writing_direction) {
  return ComputePhysicalMargins(child_style, percentage_resolution_size)
      .ConvertToLogical(container_writing_direction);
}

// Compute margins for the style owner.
inline NGBoxStrut ComputeMarginsForSelf(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLogical(style.GetWritingDirection());
}

// Compute line logical margins for the style owner.
//
// The "line" versions compute line-relative logical values. See NGLineBoxStrut
// for more details.
inline NGLineBoxStrut ComputeLineMarginsForSelf(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGLineBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLineLogical(style.GetWritingDirection());
}

// Compute line logical margins for the constraint space, in the visual order
// (always assumes LTR, ignoring the direction) for inline layout algorithm.
inline NGLineBoxStrut ComputeLineMarginsForVisualContainer(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGLineBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLineLogical(
          {constraint_space.GetWritingMode(), TextDirection::kLtr});
}

// Compute margins for a child during the min-max size calculation.
// TODO(ikilpatrick): Replace this function with ComputeMarginsFor.
CORE_EXPORT NGBoxStrut ComputeMinMaxMargins(const ComputedStyle& parent_style,
                                            NGLayoutInputNode child);

CORE_EXPORT NGBoxStrut ComputeBorders(const NGConstraintSpace&,
                                      const NGBlockNode&);

CORE_EXPORT NGBoxStrut ComputeBordersForInline(const ComputedStyle& style);

inline NGLineBoxStrut ComputeLineBorders(
    const ComputedStyle& style) {
  return NGLineBoxStrut(ComputeBordersForInline(style),
                        style.IsFlippedLinesWritingMode());
}

CORE_EXPORT NGBoxStrut ComputeBordersForTest(const ComputedStyle& style);

CORE_EXPORT NGBoxStrut ComputeIntrinsicPadding(const NGConstraintSpace&,
                                               const ComputedStyle&,
                                               const NGBoxStrut& scrollbar);

CORE_EXPORT NGBoxStrut ComputePadding(const NGConstraintSpace&,
                                      const ComputedStyle&);

inline NGLineBoxStrut ComputeLinePadding(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  return NGLineBoxStrut(ComputePadding(constraint_space, style),
                        style.IsFlippedLinesWritingMode());
}

// Compute the scrollbars and scrollbar gutters.
CORE_EXPORT NGBoxStrut ComputeScrollbarsForNonAnonymous(const NGBlockNode&);

inline NGBoxStrut ComputeScrollbars(const NGConstraintSpace& space,
                                    const NGBlockNode& node) {
  if (space.IsAnonymous())
    return NGBoxStrut();

  return ComputeScrollbarsForNonAnonymous(node);
}

// Return true if we need to know the inline size of the fragment in order to
// calculate its line-left offset. This is the case when we have auto margins,
// or when block alignment isn't line-left (e.g. with align!=left, and always in
// RTL mode).
bool NeedsInlineSizeToResolveLineLeft(
    const ComputedStyle& style,
    const ComputedStyle& containing_block_style);

// Convert inline margins from computed to used values. This will resolve 'auto'
// values and over-constrainedness. This uses the available size from the
// constraint space and inline size to compute the margins that are auto, if
// any, and adjusts the given NGBoxStrut accordingly.
// available_inline_size, inline_size, and margins are all in the
// containing_block's writing mode.
CORE_EXPORT void ResolveInlineMargins(
    const ComputedStyle& child_style,
    const ComputedStyle& containing_block_style,
    LayoutUnit available_inline_size,
    LayoutUnit inline_size,
    NGBoxStrut* margins);

// Calculate the adjustment needed for the line's left position, based on
// text-align, direction and amount of unused space.
CORE_EXPORT LayoutUnit LineOffsetForTextAlign(ETextAlign,
                                              TextDirection,
                                              LayoutUnit space_left);

inline LayoutUnit ConstrainByMinMax(LayoutUnit length,
                                    LayoutUnit min,
                                    LayoutUnit max) {
  return std::max(min, std::min(length, max));
}

// Calculates the initial (pre-layout) fragment geometry given a node, and a
// constraint space.
// The "pre-layout" block-size may be indefinite, as we'll only have enough
// information to determine this post-layout.
CORE_EXPORT NGFragmentGeometry
CalculateInitialFragmentGeometry(const NGConstraintSpace&, const NGBlockNode&);

// Similar to |CalculateInitialFragmentGeometry| however will only calculate
// the border, scrollbar, and padding (resolving percentages to zero).
CORE_EXPORT NGFragmentGeometry
CalculateInitialMinMaxFragmentGeometry(const NGConstraintSpace&,
                                       const NGBlockNode&);

// Shrinks the logical |size| by |insets|.
LogicalSize ShrinkLogicalSize(LogicalSize size, const NGBoxStrut& insets);

// Calculates the available size that children of the node should use.
LogicalSize CalculateChildAvailableSize(
    const NGConstraintSpace&,
    const NGBlockNode& node,
    const LogicalSize border_box_size,
    const NGBoxStrut& border_scrollbar_padding);

// Calculates the percentage resolution size that children of the node should
// use.
LogicalSize CalculateChildPercentageSize(
    const NGConstraintSpace&,
    const NGBlockNode node,
    const LogicalSize child_available_size);

// Calculates the percentage resolution size that replaced children of the node
// should use.
LogicalSize CalculateReplacedChildPercentageSize(
    const NGConstraintSpace&,
    const NGBlockNode node,
    const LogicalSize child_available_size,
    const NGBoxStrut& border_scrollbar_padding,
    const NGBoxStrut& border_padding);

LayoutUnit CalculateChildPercentageBlockSizeForMinMax(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode node,
    const NGBoxStrut& border_padding,
    const NGBoxStrut& scrollbar,
    LayoutUnit input_percentage_block_size,
    bool* uses_input_percentage_block_size);

// The following function clamps the calculated size based on the node
// requirements. Specifically, this adjusts the size based on size containment
// and display locking status.
LayoutUnit ClampIntrinsicBlockSize(
    const NGConstraintSpace&,
    const NGBlockNode&,
    const NGBoxStrut& border_scrollbar_padding,
    LayoutUnit current_intrinsic_block_size,
    base::Optional<LayoutUnit> body_margin_block_sum = base::nullopt);

// This function checks if the inline size of this node has to be calculated
// without considering children. If so, it returns the calculated size.
// Otherwise, it returns base::nullopt and the caller has to compute the size
// itself.
base::Optional<MinMaxSizesResult> CalculateMinMaxSizesIgnoringChildren(
    const NGBlockNode&,
    const NGBoxStrut& border_scrollbar_padding);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LENGTH_UTILS_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLengthUtils_h
#define NGLengthUtils_h

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {
class ComputedStyle;
class Length;
struct MinMaxSizeInput;
class NGConstraintSpace;
class NGBlockNode;
class NGLayoutInputNode;

// LengthResolvePhase indicates what type of layout pass we are currently in.
// This changes how lengths are resolved. kIntrinsic must be used during the
// intrinsic sizes pass, and kLayout must be used during the layout pass.
enum class LengthResolvePhase { kIntrinsic, kLayout };

// LengthResolveType indicates what type length the function is being passed
// based on its CSS property. E.g.
// kMinSize - min-width / min-height
// kMaxSize - max-width / max-height
// kMainSize - width / height
enum class LengthResolveType { kMinSize, kMaxSize, kMainSize };

inline bool NeedMinMaxSize(const ComputedStyle& style) {
  // This check is technically too broad (fill-available does not need intrinsic
  // size computation) but that's a rare case and only affects performance, not
  // correctness.
  return style.LogicalWidth().IsIntrinsic() ||
         style.LogicalMinWidth().IsIntrinsic() ||
         style.LogicalMaxWidth().IsIntrinsic();
}

// Whether the caller needs to compute min-content and max-content sizes to
// pass them to ResolveMainInlineLength / ComputeInlineSizeForFragment.
// If this function returns false, it is safe to pass an empty
// MinMaxSize struct to those functions.
inline bool NeedMinMaxSize(const NGConstraintSpace& constraint_space,
                           const ComputedStyle& style) {
  return constraint_space.IsShrinkToFit() || NeedMinMaxSize(style);
}

// Like NeedMinMaxSize, but for use when calling
// ComputeMinAndMaxContentContribution.
// Because content contributions are commonly needed by a block's parent,
// we also take a writing mode here so we can check this in the parent's
// coordinate system.
CORE_EXPORT bool NeedMinMaxSizeForContentContribution(WritingMode mode,
                                                      const ComputedStyle&);

// Returns if the given |Length| is unresolvable, e.g. the length is %-based
// during the intrinsic phase.
CORE_EXPORT bool InlineLengthUnresolvable(const Length&, LengthResolvePhase);
CORE_EXPORT bool BlockLengthUnresolvable(
    const NGConstraintSpace&,
    const Length&,
    LengthResolvePhase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Resolve means translate a Length to a LayoutUnit.
//  - |NGConstraintSpace| the information given by the parent, e.g. the
//    available-size.
//  - |ComputedStyle| the style of the node.
//  - |border_padding| the resolved border, and padding of the node.
//  - |MinMaxSize| is only used when the length is intrinsic (fit-content).
//  - |Length| is the length to resolve.
CORE_EXPORT LayoutUnit
ResolveInlineLengthInternal(const NGConstraintSpace&,
                            const ComputedStyle&,
                            const NGBoxStrut& border_padding,
                            const base::Optional<MinMaxSize>&,
                            const Length&);

// Same as ResolveInlineLengthInternal, except here |content_size| roughly plays
// the part of |MinMaxSize|.
CORE_EXPORT LayoutUnit ResolveBlockLengthInternal(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    const Length&,
    LayoutUnit content_size,
    LengthResolvePhase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Used for resolving min inline lengths, (|ComputedStyle::MinLogicalWidth|).
inline LayoutUnit ResolveMinInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSize>& min_and_max,
    const Length& length,
    LengthResolvePhase phase) {
  if (LIKELY(length.IsAuto() || InlineLengthUnresolvable(length, phase)))
    return border_padding.InlineSum();

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_and_max, length);
}

// Used for resolving max inline lengths, (|ComputedStyle::MaxLogicalWidth|).
inline LayoutUnit ResolveMaxInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSize>& min_and_max,
    const Length& length,
    LengthResolvePhase phase) {
  if (LIKELY(length.IsMaxSizeNone() || InlineLengthUnresolvable(length, phase)))
    return LayoutUnit::Max();

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_and_max, length);
}

// Used for resolving main inline lengths, (|ComputedStyle::LogicalWidth|).
inline LayoutUnit ResolveMainInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSize>& min_and_max,
    const Length& length) {
  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_and_max, length);
}

// Used for resolving min block lengths, (|ComputedStyle::MinLogicalHeight|).
inline LayoutUnit ResolveMinBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit content_size,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (LIKELY(length.IsAuto() ||
             BlockLengthUnresolvable(
                 constraint_space, length, phase,
                 opt_percentage_resolution_block_size_for_min_max)))
    return border_padding.BlockSum();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, content_size, phase,
      opt_percentage_resolution_block_size_for_min_max);
}

// Used for resolving max block lengths, (|ComputedStyle::MaxLogicalHeight|).
inline LayoutUnit ResolveMaxBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit content_size,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (LIKELY(length.IsMaxSizeNone() ||
             BlockLengthUnresolvable(
                 constraint_space, length, phase,
                 opt_percentage_resolution_block_size_for_min_max)))
    return LayoutUnit::Max();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, content_size, phase,
      opt_percentage_resolution_block_size_for_min_max);
}

// Used for resolving main block lengths, (|ComputedStyle::LogicalHeight|).
inline LayoutUnit ResolveMainBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit content_size,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(
                   constraint_space, length, phase,
                   opt_percentage_resolution_block_size_for_min_max)))
    return content_size;

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, content_size, phase,
      opt_percentage_resolution_block_size_for_min_max);
}

// For the given style and min/max content sizes, computes the min and max
// content contribution (https://drafts.csswg.org/css-sizing/#contributions).
// This is similar to ComputeInlineSizeForFragment except that it does not
// require a constraint space (percentage sizes as well as auto margins compute
// to zero) and that an auto inline size resolves to the respective min/max
// content size.
// Also, the min/max contribution does include the inline margins as well.
// Because content contributions are commonly needed by a block's parent,
// we also take a writing mode here so we can compute this in the parent's
// coordinate system.
CORE_EXPORT MinMaxSize
ComputeMinAndMaxContentContribution(WritingMode writing_mode,
                                    const ComputedStyle&,
                                    const NGBoxStrut& border_padding,
                                    const base::Optional<MinMaxSize>&);

// A version of ComputeMinAndMaxContentContribution that does not require you
// to compute the min/max content size of the child. Instead, this function
// will compute it if necessary.
// |child| is the node of which to compute the min/max content contribution.
// Note that if the writing mode of the child is orthogonal to that of the
// parent, we'll still return the inline min/max contribution in the writing
// mode of the parent (i.e. typically something based on the preferred *block*
// size of the child).
MinMaxSize ComputeMinAndMaxContentContribution(
    const ComputedStyle& parent_style,
    NGLayoutInputNode child,
    const MinMaxSizeInput&);

// Computes the min/max-content size for an out-of-flow positioned node and
// returns it, using the cache where possible. ALways computes it in the writing
// mode of the node itself.
MinMaxSize ComputeMinAndMaxContentSizeForOutOfFlow(
    const NGConstraintSpace&,
    NGLayoutInputNode,
    const NGBoxStrut& border_padding,
    const MinMaxSizeInput&);

// Returns inline size of the node's border box by resolving the computed value
// in style.logicalWidth (Length) to a layout unit, adding border and padding,
// then constraining the result by the resolved min logical width and max
// logical width from the ComputedStyle object. Calls Node::ComputeMinMaxSize
// if needed.
// |override_minmax_for_test| is provided *solely* for use by unit tests.
CORE_EXPORT LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace&,
    NGLayoutInputNode,
    const NGBoxStrut& border_padding,
    const MinMaxSize* override_minmax_for_test = nullptr);

// Same as ComputeInlineSizeForFragment, but uses height instead of width.
CORE_EXPORT LayoutUnit
ComputeBlockSizeForFragment(const NGConstraintSpace&,
                            const ComputedStyle&,
                            const NGBoxStrut& border_padding,
                            LayoutUnit content_size);

// Computes intrinsic size for replaced elements.
CORE_EXPORT NGLogicalSize
ComputeReplacedSize(const NGLayoutInputNode&,
                    const NGConstraintSpace&,
                    const base::Optional<MinMaxSize>&);

// Based on available inline size, CSS computed column-width, CSS computed
// column-count and CSS used column-gap, return CSS used column-count.
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

inline NGBoxStrut ComputeMarginsFor(const ComputedStyle& child_style,
                                    LayoutUnit percentage_resolution_size,
                                    WritingMode container_writing_mode,
                                    TextDirection container_direction) {
  return ComputePhysicalMargins(child_style, percentage_resolution_size)
      .ConvertToLogical(container_writing_mode, container_direction);
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
      .ConvertToLogical(style.GetWritingMode(), style.Direction());
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
      .ConvertToLineLogical(style.GetWritingMode(), style.Direction());
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
      .ConvertToLineLogical(constraint_space.GetWritingMode(),
                            TextDirection::kLtr);
}

// Compute margins for a child during the min-max size calculation.
CORE_EXPORT NGBoxStrut ComputeMinMaxMargins(const ComputedStyle& parent_style,
                                            NGLayoutInputNode child);

CORE_EXPORT NGBoxStrut ComputeBorders(const NGConstraintSpace&,
                                      const NGLayoutInputNode);

CORE_EXPORT NGBoxStrut ComputeBordersForInline(const ComputedStyle& style);

inline NGLineBoxStrut ComputeLineBorders(
    const ComputedStyle& style) {
  return NGLineBoxStrut(ComputeBordersForInline(style),
                        style.IsFlippedLinesWritingMode());
}

CORE_EXPORT NGBoxStrut ComputeBordersForTest(const ComputedStyle& style);

CORE_EXPORT NGBoxStrut ComputeIntrinsicPadding(const NGConstraintSpace&,
                                               const NGLayoutInputNode);

CORE_EXPORT NGBoxStrut ComputePadding(const NGConstraintSpace&,
                                      const ComputedStyle&);

inline NGLineBoxStrut ComputeLinePadding(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  return NGLineBoxStrut(ComputePadding(constraint_space, style),
                        style.IsFlippedLinesWritingMode());
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

// Same as |LineOffsetForTextAlign| but returns the logical inline offset
// instead of line-left offset.
CORE_EXPORT LayoutUnit InlineOffsetForTextAlign(const ComputedStyle&,
                                                LayoutUnit space_left);

inline LayoutUnit ConstrainByMinMax(LayoutUnit length,
                                    LayoutUnit min,
                                    LayoutUnit max) {
  return std::max(min, std::min(length, max));
}

// Clamp the inline size of the scrollbar, unless it's larger than the inline
// size of the content box, in which case we'll return that instead. Scrollbar
// handling is quite bad in such situations, and this method here is just to
// make sure that left-hand scrollbars don't mess up scrollWidth. For the full
// story, visit http://crbug.com/724255.
bool ClampScrollbarToContentBox(NGBoxStrut* scrollbars,
                                LayoutUnit content_box_inline_size);

NGBoxStrut CalculateBorderScrollbarPadding(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode node);

// border_padding can be passed in as an optimization; otherwise this function
// will compute it itself.
NGLogicalSize CalculateBorderBoxSize(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode& node,
    const NGBoxStrut& border_padding,
    LayoutUnit block_content_size = NGSizeIndefinite);

// Shrink and return the available size by an inset. This may e.g. be used to
// convert from border-box to content-box size. Indefinite block size is
// allowed, in which case the inset will be ignored for block size.
NGLogicalSize ShrinkAvailableSize(NGLogicalSize size, const NGBoxStrut& inset);

// Calculates default content size for html and body elements in quirks mode.
// Returns NGSizeIndefinite in all other cases.
LayoutUnit CalculateDefaultBlockSize(
    const NGConstraintSpace&,
    const NGBlockNode&,
    const NGBoxStrut& border_scrollbar_padding);

// Calculates the percentage resolution size that children of the node should
// use.
NGLogicalSize CalculateChildPercentageSize(
    const NGConstraintSpace&,
    const NGBlockNode node,
    const NGLogicalSize& child_available_size);

// Calculates the percentage resolution size that replaced children of the node
// should use.
NGLogicalSize CalculateReplacedChildPercentageSize(
    const NGConstraintSpace&,
    const NGBlockNode node,
    NGLogicalSize border_box_size,
    const NGBoxStrut& border_scrollbar_padding,
    const NGBoxStrut& border_padding);

LayoutUnit CalculateChildPercentageBlockSizeForMinMax(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode node,
    const NGBoxStrut& border_padding,
    LayoutUnit parent_percentage_block_size);

}  // namespace blink

#endif  // NGLengthUtils_h

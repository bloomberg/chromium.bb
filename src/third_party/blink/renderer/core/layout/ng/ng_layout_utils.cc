// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

namespace {

// LengthResolveType indicates what type length the function is being passed
// based on its CSS property. E.g.
// kMinSize - min-width / min-height
// kMaxSize - max-width / max-height
// kMainSize - width / height
enum class LengthResolveType { kMinSize, kMaxSize, kMainSize };

inline bool InlineLengthMayChange(const ComputedStyle& style,
                                  const Length& length,
                                  LengthResolveType type,
                                  const NGConstraintSpace& new_space,
                                  const NGConstraintSpace& old_space,
                                  const NGLayoutResult& layout_result) {
  DCHECK_EQ(new_space.IsShrinkToFit(), old_space.IsShrinkToFit());
#if DCHECK_IS_ON()
  if (type == LengthResolveType::kMainSize && new_space.IsShrinkToFit())
    DCHECK(length.IsAuto());
#endif

  bool is_unspecified =
      (length.IsAuto() && type != LengthResolveType::kMinSize) ||
      length.IsFitContent() || length.IsFillAvailable();

  // Percentage inline margins will affect the size if the size is unspecified
  // (auto and similar).
  if (is_unspecified && style.MayHaveMargin() &&
      (style.MarginStart().IsPercentOrCalc() ||
       style.MarginEnd().IsPercentOrCalc()) &&
      (new_space.PercentageResolutionInlineSize() !=
       old_space.PercentageResolutionInlineSize()))
    return true;

  if (is_unspecified) {
    if (new_space.AvailableSize().inline_size !=
        old_space.AvailableSize().inline_size)
      return true;
  }

  if (length.IsPercentOrCalc()) {
    if (new_space.PercentageResolutionInlineSize() !=
        old_space.PercentageResolutionInlineSize())
      return true;
  }
  return false;
}

inline bool BlockLengthMayChange(const Length& length,
                                 const NGConstraintSpace& new_space,
                                 const NGConstraintSpace& old_space) {
  if (length.IsFillAvailable()) {
    if (new_space.AvailableSize().block_size !=
        old_space.AvailableSize().block_size)
      return true;
  }

  return false;
}

// Return true if it's possible (but not necessarily guaranteed) that the new
// constraint space will give a different size compared to the old one, when
// computed style and child content remain unchanged.
bool SizeMayChange(const NGBlockNode& node,
                   const NGConstraintSpace& new_space,
                   const NGConstraintSpace& old_space,
                   const NGLayoutResult& layout_result) {
  if (node.IsQuirkyAndFillsViewport())
    return true;

  DCHECK_EQ(new_space.IsFixedSizeInline(), old_space.IsFixedSizeInline());
  DCHECK_EQ(new_space.IsFixedSizeBlock(), old_space.IsFixedSizeBlock());
  DCHECK_EQ(new_space.IsShrinkToFit(), old_space.IsShrinkToFit());
  DCHECK_EQ(new_space.TableCellChildLayoutPhase(),
            old_space.TableCellChildLayoutPhase());

  const ComputedStyle& style = node.Style();

  // Go through all length properties, and, depending on length type
  // (percentages, auto, etc.), check whether the constraint spaces differ in
  // such a way that the resulting size *may* change. There are currently many
  // possible false-positive situations here, as we don't rule out length
  // changes that won't have any effect on the final size (e.g. if inline-size
  // is 100px, max-inline-size is 50%, and percentage resolution inline size
  // changes from 1000px to 500px). If the constraint space has "fixed" size in
  // a dimension, we can skip checking properties in that dimension and just
  // look for available size changes, since that's how a "fixed" constraint
  // space works.
  if (new_space.IsFixedSizeInline()) {
    if (new_space.AvailableSize().inline_size !=
        old_space.AvailableSize().inline_size)
      return true;
  } else {
    if (InlineLengthMayChange(style, style.LogicalWidth(),
                              LengthResolveType::kMainSize, new_space,
                              old_space, layout_result) ||
        InlineLengthMayChange(style, style.LogicalMinWidth(),
                              LengthResolveType::kMinSize, new_space, old_space,
                              layout_result) ||
        InlineLengthMayChange(style, style.LogicalMaxWidth(),
                              LengthResolveType::kMaxSize, new_space, old_space,
                              layout_result))
      return true;
  }

  if (new_space.IsFixedSizeBlock()) {
    if (new_space.AvailableSize().block_size !=
        old_space.AvailableSize().block_size)
      return true;
  } else {
    if (BlockLengthMayChange(style.LogicalHeight(), new_space, old_space) ||
        BlockLengthMayChange(style.LogicalMinHeight(), new_space, old_space) ||
        BlockLengthMayChange(style.LogicalMaxHeight(), new_space, old_space))
      return true;
    // We only need to check if the PercentageResolutionBlockSizes match if the
    // layout result has explicitly marked itself as dependent.
    if (layout_result.PhysicalFragment().DependsOnPercentageBlockSize()) {
      if (new_space.PercentageResolutionBlockSize() !=
          old_space.PercentageResolutionBlockSize())
        return true;
      if (new_space.ReplacedPercentageResolutionBlockSize() !=
          old_space.ReplacedPercentageResolutionBlockSize())
        return true;
    }
  }

  if (style.MayHavePadding() &&
      new_space.PercentageResolutionInlineSize() !=
          old_space.PercentageResolutionInlineSize()) {
    // Percentage-based padding is resolved against the inline content box size
    // of the containing block.
    if (style.PaddingTop().IsPercentOrCalc() ||
        style.PaddingRight().IsPercentOrCalc() ||
        style.PaddingBottom().IsPercentOrCalc() ||
        style.PaddingLeft().IsPercentOrCalc())
      return true;
  }

  return false;
}

// Given the pre-computed |fragment_geometry| calcuates the
// |NGLayoutCacheStatus| based on this sizing information. Returns:
//  - |NGLayoutCacheStatus::kNeedsLayout| if the |new_space| will produce a
//    different sized fragment, or if any %-block-size children will change
//    size.
//  - |NGLayoutCacheStatus::kNeedsSimplifiedLayout| if the block-size of the
//    fragment will change, *without* affecting any descendants (no descendants
//    have %-block-sizes).
//  - |NGLayoutCacheStatus::kHit| otherwise.
NGLayoutCacheStatus CalculateSizeBasedLayoutCacheStatusWithGeometry(
    const NGBlockNode& node,
    const NGFragmentGeometry& fragment_geometry,
    const NGLayoutResult& layout_result,
    const NGConstraintSpace& new_space,
    const NGConstraintSpace& old_space) {
  const ComputedStyle& style = node.Style();
  const NGPhysicalBoxFragment& physical_fragment =
      To<NGPhysicalBoxFragment>(layout_result.PhysicalFragment());
  NGBoxFragment fragment(style.GetWritingMode(), style.Direction(),
                         physical_fragment);

  if (fragment_geometry.border_box_size.inline_size != fragment.InlineSize())
    return NGLayoutCacheStatus::kNeedsLayout;

  LayoutUnit block_size = fragment_geometry.border_box_size.block_size;
  bool is_initial_block_size_indefinite = block_size == kIndefiniteSize;
  if (is_initial_block_size_indefinite) {
    // The intrinsic size of column flex-boxes can depend on the
    // %-resolution-block-size. This occurs when a flex-box has "max-height:
    // 100%" or similar on itself.
    //
    // Due to this we can't use cached |NGLayoutResult::IntrinsicBlockSize|
    // value, as the following |block_size| calculation would be incorrect.
    if (node.IsFlexibleBox() && style.IsColumnFlexDirection() &&
        layout_result.PhysicalFragment().DependsOnPercentageBlockSize()) {
      if (new_space.PercentageResolutionBlockSize() !=
          old_space.PercentageResolutionBlockSize())
        return NGLayoutCacheStatus::kNeedsLayout;
    }

    block_size = ComputeBlockSizeForFragment(
        new_space, node, fragment_geometry.border + fragment_geometry.padding,
        layout_result.IntrinsicBlockSize());
  }

  bool is_block_size_equal = block_size == fragment.BlockSize();

  if (!is_block_size_equal) {
    // If we are the document or body element in quirks mode, changing our size
    // means that a scrollbar was added/removed. Require full layout.
    if (node.IsQuirkyAndFillsViewport())
      return NGLayoutCacheStatus::kNeedsLayout;

    // If a block (within a formatting-context) changes to/from an empty-block,
    // margins may collapse through this node, requiring full layout. We
    // approximate this check by checking if the block-size is/was zero.
    if (!physical_fragment.IsBlockFormattingContextRoot() &&
        !block_size != !fragment.BlockSize())
      return NGLayoutCacheStatus::kNeedsLayout;
  }

  if (layout_result.HasDescendantThatDependsOnPercentageBlockSize()) {
    // %-block-size children of flex-items sometimes don't resolve their
    // percentages against a fixed block-size.
    // We miss the cache if the %-resolution block-size changes from indefinite
    // to definite (or visa-versa).
    bool is_new_initial_block_size_indefinite =
        new_space.IsFixedSizeBlock() ? !new_space.FixedSizeBlockIsDefinite()
                                     : is_initial_block_size_indefinite;

    bool is_old_initial_block_size_indefinite =
        old_space.IsFixedSizeBlock()
            ? !old_space.FixedSizeBlockIsDefinite()
            : layout_result.IsInitialBlockSizeIndefinite();

    if (is_old_initial_block_size_indefinite !=
        is_new_initial_block_size_indefinite)
      return NGLayoutCacheStatus::kNeedsLayout;

    // %-block-size children of table-cells have different behaviour if they
    // are in the "measure" or "layout" phase.
    // Instead of trying to capture that logic here, we always miss the cache.
    if (node.IsTableCell() &&
        new_space.IsFixedSizeBlock() != old_space.IsFixedSizeBlock())
      return NGLayoutCacheStatus::kNeedsLayout;

    // If our initial block-size is definite, we know that if we change our
    // block-size we'll affect any descendant that depends on the resulting
    // percentage block-size.
    if (!is_block_size_equal && !is_new_initial_block_size_indefinite)
      return NGLayoutCacheStatus::kNeedsLayout;

    DCHECK(is_block_size_equal || is_new_initial_block_size_indefinite);

    // At this point we know that either we have the same block-size for our
    // fragment, or our initial block-size was indefinite.
    //
    // The |NGPhysicalContainerFragment::DependsOnPercentageBlockSize| flag
    // will returns true if we are in quirks mode, and have a descendant that
    // depends on a percentage block-size, however it will also return true if
    // the node itself depends on the %-block-size.
    //
    // As we only care about the quirks-mode %-block-size behaviour we remove
    // this false-positive by checking if we have an initial indefinite
    // block-size.
    if (is_new_initial_block_size_indefinite &&
        layout_result.PhysicalFragment().DependsOnPercentageBlockSize()) {
      DCHECK(is_old_initial_block_size_indefinite);
      if (new_space.PercentageResolutionBlockSize() !=
          old_space.PercentageResolutionBlockSize())
        return NGLayoutCacheStatus::kNeedsLayout;
      if (new_space.ReplacedPercentageResolutionBlockSize() !=
          old_space.ReplacedPercentageResolutionBlockSize())
        return NGLayoutCacheStatus::kNeedsLayout;
    }
  }

  if (style.MayHavePadding() && fragment_geometry.padding != fragment.Padding())
    return NGLayoutCacheStatus::kNeedsLayout;

  // If we've reached here we know that we can potentially "stretch"/"shrink"
  // ourselves without affecting any of our children.
  // In that case we may be able to perform "simplified" layout.
  return is_block_size_equal ? NGLayoutCacheStatus::kHit
                             : NGLayoutCacheStatus::kNeedsSimplifiedLayout;
}

bool IntrinsicSizeWillChange(
    const NGBlockNode& node,
    const NGLayoutResult& cached_layout_result,
    const NGConstraintSpace& new_space,
    base::Optional<NGFragmentGeometry>* fragment_geometry) {
  const ComputedStyle& style = node.Style();
  if (!new_space.IsShrinkToFit() && !NeedMinMaxSize(style))
    return false;

  if (!*fragment_geometry)
    *fragment_geometry = CalculateInitialFragmentGeometry(new_space, node);

  LayoutUnit inline_size = NGFragment(style.GetWritingMode(),
                                      cached_layout_result.PhysicalFragment())
                               .InlineSize();

  if ((*fragment_geometry)->border_box_size.inline_size != inline_size)
    return true;

  return false;
}

}  // namespace

NGLayoutCacheStatus CalculateSizeBasedLayoutCacheStatus(
    const NGBlockNode& node,
    const NGLayoutResult& cached_layout_result,
    const NGConstraintSpace& new_space,
    base::Optional<NGFragmentGeometry>* fragment_geometry) {
  DCHECK_EQ(cached_layout_result.Status(), NGLayoutResult::kSuccess);
  DCHECK(cached_layout_result.HasValidConstraintSpaceForCaching());

  const NGConstraintSpace& old_space =
      cached_layout_result.GetConstraintSpaceForCaching();

  if (!new_space.MaySkipLayout(old_space))
    return NGLayoutCacheStatus::kNeedsLayout;

  if (new_space.AreSizeConstraintsEqual(old_space)) {
    // It is possible that our intrinsic size has changed, check for that here.
    // TODO(cbiesinger): Investigate why this check doesn't apply to
    // |MaySkipLegacyLayout|.
    if (IntrinsicSizeWillChange(node, cached_layout_result, new_space,
                                fragment_geometry))
      return NGLayoutCacheStatus::kNeedsLayout;

    // We don't have to check our style if we know the constraint space sizes
    // will remain the same.
    if (new_space.AreSizesEqual(old_space))
      return NGLayoutCacheStatus::kHit;

    if (!SizeMayChange(node, new_space, old_space, cached_layout_result))
      return NGLayoutCacheStatus::kHit;
  }

  if (!*fragment_geometry)
    *fragment_geometry = CalculateInitialFragmentGeometry(new_space, node);

  return CalculateSizeBasedLayoutCacheStatusWithGeometry(
      node, **fragment_geometry, cached_layout_result, new_space, old_space);
}

bool MaySkipLegacyLayout(const NGBlockNode& node,
                         const NGLayoutResult& cached_layout_result,
                         const NGConstraintSpace& new_space) {
  DCHECK_EQ(cached_layout_result.Status(), NGLayoutResult::kSuccess);
  DCHECK(cached_layout_result.HasValidConstraintSpaceForCaching());

  const NGConstraintSpace& old_space =
      cached_layout_result.GetConstraintSpaceForCaching();
  if (!new_space.MaySkipLayout(old_space))
    return false;

  if (!new_space.AreSizeConstraintsEqual(old_space))
    return false;

  if (new_space.AreSizesEqual(old_space))
    return true;

  if (SizeMayChange(node, new_space, old_space, cached_layout_result))
    return false;

  return true;
}

bool MaySkipLayoutWithinBlockFormattingContext(
    const NGLayoutResult& cached_layout_result,
    const NGConstraintSpace& new_space,
    base::Optional<LayoutUnit>* bfc_block_offset) {
  DCHECK_EQ(cached_layout_result.Status(), NGLayoutResult::kSuccess);
  DCHECK(cached_layout_result.HasValidConstraintSpaceForCaching());
  DCHECK(bfc_block_offset);

  const NGConstraintSpace& old_space =
      cached_layout_result.GetConstraintSpaceForCaching();

  LayoutUnit old_clearance_offset = old_space.ClearanceOffset();
  LayoutUnit new_clearance_offset = new_space.ClearanceOffset();

  // Determine if we can reuse a result if it was affected by clearance.
  bool is_pushed_by_floats = cached_layout_result.IsPushedByFloats();
  if (is_pushed_by_floats) {
    DCHECK(old_space.HasFloats());

    // We don't attempt to reuse the cached result if the clearance offset
    // differs from the final BFC-block-offset.
    //
    // The |is_pushed_by_floats| flag is also used by nodes who have a *child*
    // which was pushed by floats. In this case the node may not have a
    // BFC-block-offset or one equal to the clearance offset.
    if (!cached_layout_result.BfcBlockOffset() ||
        *cached_layout_result.BfcBlockOffset() != old_space.ClearanceOffset())
      return false;

    // We only reuse the cached result if the delta between the
    // BFC-block-offset, and the clearance offset grows or remains the same. If
    // it shrinks it may not be affected by clearance anymore as a margin may
    // push the fragment below the clearance offset instead.
    //
    // TODO(layout-dev): If we track if any margins affected this calculation
    // (with an additional bit on the layout result) we could potentially skip
    // this check.
    if (old_clearance_offset - old_space.BfcOffset().block_offset >
        new_clearance_offset - new_space.BfcOffset().block_offset) {
      return false;
    }
  }

  // Check we have a descendant that *may* be positioned above the block-start
  // edge. We abort if either the old or new space has floats, as we don't keep
  // track of how far above the child could be. This case is relatively rare,
  // and only occurs with negative margins.
  if (cached_layout_result.PhysicalFragment()
          .MayHaveDescendantAboveBlockStart() &&
      (old_space.HasFloats() || new_space.HasFloats()))
    return false;

  // We can now try to adjust the BFC block-offset.
  if (*bfc_block_offset) {
    // Check if the previous position may intersect with any floats.
    if (**bfc_block_offset <
        old_space.ExclusionSpace().ClearanceOffset(EClear::kBoth))
      return false;

    if (is_pushed_by_floats) {
      DCHECK_EQ(**bfc_block_offset, old_clearance_offset);
      *bfc_block_offset = new_clearance_offset;
    } else {
      *bfc_block_offset = **bfc_block_offset -
                          old_space.BfcOffset().block_offset +
                          new_space.BfcOffset().block_offset;
    }

    // Check if the new position may intersect with any floats.
    if (**bfc_block_offset <
        new_space.ExclusionSpace().ClearanceOffset(EClear::kBoth))
      return false;
  }

  return true;
}

bool IsBlockLayoutComplete(const NGConstraintSpace& space,
                           const NGLayoutResult& result) {
  if (result.Status() != NGLayoutResult::kSuccess)
    return false;
  if (space.IsIntermediateLayout())
    return false;
  return true;
}

}  // namespace blink

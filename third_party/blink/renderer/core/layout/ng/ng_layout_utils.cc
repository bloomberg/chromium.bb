// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

namespace {

bool ContentShrinkToFitMayChange(const ComputedStyle& style,
                                 const NGConstraintSpace& new_space,
                                 const NGConstraintSpace& old_space,
                                 const NGLayoutResult& layout_result) {
  if (old_space.AvailableSize().inline_size ==
      new_space.AvailableSize().inline_size)
    return false;

  NGBoxStrut margins = ComputeMarginsForSelf(new_space, style);

#if DCHECK_IS_ON()
  // The margins must be the same, as this function won't be called if we have
  // percentage inline margins, and the percentage resolution size changes.
  NGBoxStrut old_margins = ComputeMarginsForSelf(old_space, style);
  DCHECK_EQ(margins.inline_start, old_margins.inline_start);
  DCHECK_EQ(margins.inline_end, old_margins.inline_end);
#endif

  LayoutUnit old_available_inline_size =
      std::max(LayoutUnit(),
               old_space.AvailableSize().inline_size - margins.InlineSum());
  LayoutUnit new_available_inline_size =
      std::max(LayoutUnit(),
               new_space.AvailableSize().inline_size - margins.InlineSum());

  DCHECK(layout_result.PhysicalFragment());
  LayoutUnit inline_size =
      NGFragment(style.GetWritingMode(), *layout_result.PhysicalFragment())
          .InlineSize();

  // If the previous fragment was at its min-content size (indicated by the old
  // available size being smaller than the fragment), we may be able to skip
  // layout if the new available size is also smaller.
  bool unaffected_as_min_content_size =
      old_available_inline_size < inline_size &&
      new_available_inline_size <= inline_size;

  // If the previous fragment was at its max-content size (indicated by the old
  // available size being larger than the fragment), we may be able to skip
  // layout if the new available size is also larger.
  bool unaffected_as_max_content_size =
      old_available_inline_size > inline_size &&
      new_available_inline_size >= inline_size;

  // TODO(crbug.com/935634): There is an additional optimization where if we
  // detect (by setting a flag in the layout result) that the
  // min-content == max-content we can simply just skip layout, as the
  // available size won't have any effect.

  if (unaffected_as_min_content_size || unaffected_as_max_content_size)
    return false;

  return true;
}

inline bool InlineLengthMayChange(const ComputedStyle& style,
                                  const Length& length,
                                  LengthResolveType type,
                                  const NGConstraintSpace& new_space,
                                  const NGConstraintSpace& old_space,
                                  const NGLayoutResult& layout_result) {
  DCHECK_EQ(new_space.IsShrinkToFit(), old_space.IsShrinkToFit());
#if DCHECK_IS_ON()
  if (type == LengthResolveType::kContentSize && new_space.IsShrinkToFit())
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

  // For elements which shrink to fit, we can perform a specific optimization
  // where we can skip relayout if the element was sized to its min-content or
  // max-content size.
  bool is_content_shrink_to_fit =
      type == LengthResolveType::kContentSize &&
      (new_space.IsShrinkToFit() || length.IsFitContent());

  if (is_content_shrink_to_fit) {
    return ContentShrinkToFitMayChange(style, new_space, old_space,
                                       layout_result);
  }

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
bool SizeMayChange(const ComputedStyle& style,
                   const NGConstraintSpace& new_space,
                   const NGConstraintSpace& old_space,
                   const NGLayoutResult& layout_result) {
  DCHECK_EQ(new_space.IsFixedSizeInline(), old_space.IsFixedSizeInline());
  DCHECK_EQ(new_space.IsFixedSizeBlock(), old_space.IsFixedSizeBlock());

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
                              LengthResolveType::kContentSize, new_space,
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
    if (layout_result.DependsOnPercentageBlockSize()) {
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

}  // namespace

bool MaySkipLayout(const NGBlockNode node,
                   const NGLayoutResult& cached_layout_result,
                   const NGConstraintSpace& new_space) {
  DCHECK_EQ(cached_layout_result.Status(), NGLayoutResult::kSuccess);
  DCHECK(cached_layout_result.HasValidConstraintSpaceForCaching());

  const NGConstraintSpace& old_space =
      cached_layout_result.GetConstraintSpaceForCaching();
  if (!new_space.MaySkipLayout(old_space))
    return false;

  if (!new_space.AreSizesEqual(old_space)) {
    // We need to descend all the way down into BODY if we're in quirks mode,
    // since it magically follows the viewport size.
    if (node.IsQuirkyAndFillsViewport())
      return false;

    // If the available / percentage sizes have changed in a way that may affect
    // layout, we cannot re-use the previous result.
    if (SizeMayChange(node.Style(), new_space, old_space, cached_layout_result))
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
  // Check that we're done positioning pending floats.
  return !result.AdjoiningFloatTypes() || result.BfcBlockOffset() ||
         space.FloatsBfcBlockOffset();
}

}  // namespace blink

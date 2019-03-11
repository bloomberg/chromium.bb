// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

namespace {

inline bool InlineLengthMayChange(const Length& length,
                                  LengthResolveType type,
                                  const NGConstraintSpace& new_space,
                                  const NGConstraintSpace& old_space) {
  // Percentage inline margins will affect the size if the size is unspecified
  // (auto and similar). So we need to check both available size and the
  // percentage resolution size in that case.
  bool is_unspecified =
      (length.IsAuto() && type != LengthResolveType::kMinSize) ||
      length.IsFitContent() || length.IsFillAvailable();
  if (is_unspecified) {
    if (new_space.AvailableSize().inline_size !=
        old_space.AvailableSize().inline_size)
      return true;
  }
  if (is_unspecified || length.IsPercentOrCalc()) {
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
    if (InlineLengthMayChange(style.LogicalWidth(),
                              LengthResolveType::kContentSize, new_space,
                              old_space) ||
        InlineLengthMayChange(style.LogicalMinWidth(),
                              LengthResolveType::kMinSize, new_space,
                              old_space) ||
        InlineLengthMayChange(style.LogicalMaxWidth(),
                              LengthResolveType::kMaxSize, new_space,
                              old_space))
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

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLengthUtils_h
#define NGLengthUtils_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/optional.h"

namespace blink {
class ComputedStyle;
class Length;
class NGConstraintSpace;
class NGBlockNode;
class NGLayoutInputNode;

enum class LengthResolveType { kMinSize, kMaxSize, kContentSize };

// Whether the caller needs to compute min-content and max-content sizes to
// pass them to ResolveInlineLength / ComputeInlineSizeForFragment.
// If this function returns false, it is safe to pass an empty
// MinMaxSize struct to those functions.
CORE_EXPORT bool NeedMinMaxSize(const NGConstraintSpace&, const ComputedStyle&);

CORE_EXPORT bool NeedMinMaxSize(const ComputedStyle&);

// Like NeedMinMaxSize, but for use when calling
// ComputeMinAndMaxContentContribution.
CORE_EXPORT bool NeedMinMaxSizeForContentContribution(const ComputedStyle&);

// Convert an inline-axis length to a layout unit using the given constraint
// space.
CORE_EXPORT LayoutUnit ResolveInlineLength(const NGConstraintSpace&,
                                           const ComputedStyle&,
                                           const WTF::Optional<MinMaxSize>&,
                                           const Length&,
                                           LengthResolveType);

// Convert a block-axis length to a layout unit using the given constraint
// space and content size.
CORE_EXPORT LayoutUnit ResolveBlockLength(const NGConstraintSpace&,
                                          const ComputedStyle&,
                                          const Length&,
                                          LayoutUnit content_size,
                                          LengthResolveType);

// Convert margin/border/padding length to a layout unit using the
// given constraint space.
CORE_EXPORT LayoutUnit ResolveMarginPaddingLength(const NGConstraintSpace&,
                                                  const Length&);

// For the given style and min/max content sizes, computes the min and max
// content contribution (https://drafts.csswg.org/css-sizing/#contributions).
// This is similar to ComputeInlineSizeForFragment except that it does not
// require a constraint space (percentage sizes as well as auto margins compute
// to zero) and that an auto inline size resolves to the respective min/max
// content size.
// Also, the min/max contribution does include the inline margins as well.
CORE_EXPORT MinMaxSize
ComputeMinAndMaxContentContribution(const ComputedStyle&,
                                    const WTF::Optional<MinMaxSize>&);

// Resolves the given length to a layout unit, constraining it by the min
// logical width and max logical width properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit
ComputeInlineSizeForFragment(const NGConstraintSpace&,
                             const ComputedStyle&,
                             const WTF::Optional<MinMaxSize>&);

// Resolves the given length to a layout unit, constraining it by the min
// logical height and max logical height properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit ComputeBlockSizeForFragment(const NGConstraintSpace&,
                                                   const ComputedStyle&,
                                                   LayoutUnit content_size);

// Computes intrinsic size for replaced elements.
CORE_EXPORT NGLogicalSize ComputeReplacedSize(const NGLayoutInputNode&,
                                              const NGConstraintSpace&,
                                              const Optional<MinMaxSize>&);

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
CORE_EXPORT NGPhysicalBoxStrut ComputePhysicalMargins(const NGConstraintSpace&,
                                                      const ComputedStyle&);
// Compute margins for the specified NGConstraintSpace.
CORE_EXPORT NGBoxStrut ComputeMarginsFor(const NGConstraintSpace&,
                                         const ComputedStyle&,
                                         const NGConstraintSpace& compute_for);
// Compute margins for the NGConstraintSpace.
CORE_EXPORT NGBoxStrut ComputeMarginsForContainer(const NGConstraintSpace&,
                                                  const ComputedStyle&);
// Compute margins for the NGConstraintSpace in the visual order.
CORE_EXPORT NGBoxStrut
ComputeMarginsForVisualContainer(const NGConstraintSpace&,
                                 const ComputedStyle&);
// Compute margins for the style owner.
CORE_EXPORT NGBoxStrut ComputeMarginsForSelf(const NGConstraintSpace&,
                                             const ComputedStyle&);

// Compute margins for a child during the min-max size calculation.
CORE_EXPORT NGBoxStrut ComputeMinMaxMargins(const ComputedStyle& parent_style,
                                            NGLayoutInputNode child);

CORE_EXPORT NGBoxStrut ComputeBorders(const NGConstraintSpace& constraint_space,
                                      const ComputedStyle&);

CORE_EXPORT NGBoxStrut ComputePadding(const NGConstraintSpace&,
                                      const ComputedStyle&);

// Resolves margin: auto in the inline direction.
// This uses the available size from the constraint space and inline size to
// compute the margins that are auto, if any, and adjusts
// the given NGBoxStrut accordingly.
CORE_EXPORT void ApplyAutoMargins(const ComputedStyle& child_style,
                                  const ComputedStyle& containing_block_style,
                                  LayoutUnit available_inline_size,
                                  LayoutUnit inline_size,
                                  NGBoxStrut* margins);

// Calculate the adjustment needed for the line's left position, based on
// text-align, direction and amount of unused space.
CORE_EXPORT LayoutUnit LineOffsetForTextAlign(ETextAlign,
                                              TextDirection,
                                              LayoutUnit space_left);

CORE_EXPORT LayoutUnit ConstrainByMinMax(LayoutUnit length,
                                         Optional<LayoutUnit> min,
                                         Optional<LayoutUnit> max);

NGBoxStrut CalculateBorderScrollbarPadding(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode node);

inline NGLogicalSize CalculateBorderBoxSize(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const Optional<MinMaxSize>& min_and_max,
    LayoutUnit block_content_size = NGSizeIndefinite) {
  return NGLogicalSize(
      ComputeInlineSizeForFragment(constraint_space, style, min_and_max),
      ComputeBlockSizeForFragment(constraint_space, style, block_content_size));
}

NGLogicalSize CalculateContentBoxSize(
    const NGLogicalSize border_box_size,
    const NGBoxStrut& border_scrollbar_padding);

}  // namespace blink

#endif  // NGLengthUtils_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"

namespace blink {
// TODO(layout-ng):
// - Handle border-box correctly
// - positioned and/or replaced calculations
// - Handle margins for fill-available and width: auto
// - Take scrollbars into account

LayoutUnit resolveInlineLength(const NGConstraintSpace& constraintSpace,
                               const Length& length,
                               LengthResolveType type) {
  // TODO(layout-ng): Handle min/max/fit-content
  DCHECK(!length.isMaxSizeNone());
  DCHECK_GE(constraintSpace.ContainerSize().inline_size, LayoutUnit());

  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();

  if (type == LengthResolveType::MarginBorderPaddingSize && length.isAuto())
    return LayoutUnit();

  return valueForLength(length, constraintSpace.ContainerSize().inline_size);
}

LayoutUnit resolveBlockLength(const NGConstraintSpace& constraintSpace,
                              const Length& length,
                              LayoutUnit contentSize,
                              LengthResolveType type) {
  DCHECK(!length.isMaxSizeNone());
  DCHECK(type != LengthResolveType::MarginBorderPaddingSize);

  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();

  if (length.isAuto())
    return contentSize;

  if (length.isMinContent() || length.isMaxContent() || length.isFitContent())
    return contentSize;

  // Make sure that indefinite percentages resolve to NGSizeIndefinite, not to
  // a random negative number.
  if (length.isPercentOrCalc() &&
      constraintSpace.ContainerSize().block_size == NGSizeIndefinite)
    return contentSize;

  return valueForLength(length, constraintSpace.ContainerSize().block_size);
}

LayoutUnit computeInlineSizeForFragment(
    const NGConstraintSpace& constraintSpace,
    const ComputedStyle& style) {
  if (constraintSpace.FixedInlineSize())
    return constraintSpace.ContainerSize().inline_size;

  LayoutUnit extent = resolveInlineLength(constraintSpace, style.logicalWidth(),
                                          LengthResolveType::ContentSize);

  Length maxLength = style.logicalMaxWidth();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max = resolveInlineLength(constraintSpace, maxLength,
                                         LengthResolveType::MaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min = resolveInlineLength(constraintSpace, style.logicalMinWidth(),
                                       LengthResolveType::MinSize);
  extent = std::max(extent, min);

  if (style.boxSizing() == BoxSizingContentBox) {
    // TODO(layout-ng): Compute border/padding size and add it
  }

  return extent;
}

LayoutUnit computeBlockSizeForFragment(const NGConstraintSpace& constraintSpace,
                                       const ComputedStyle& style,
                                       LayoutUnit contentSize) {
  if (constraintSpace.FixedBlockSize())
    return constraintSpace.ContainerSize().block_size;

  LayoutUnit extent =
      resolveBlockLength(constraintSpace, style.logicalHeight(), contentSize,
                         LengthResolveType::ContentSize);
  if (extent == NGSizeIndefinite) {
    DCHECK_EQ(contentSize, NGSizeIndefinite);
    return extent;
  }

  Length maxLength = style.logicalMaxHeight();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max = resolveBlockLength(constraintSpace, maxLength, contentSize,
                                        LengthResolveType::MaxSize);
    extent = std::min(extent, max);
  }

  LayoutUnit min = resolveBlockLength(constraintSpace, style.logicalMinHeight(),
                                      contentSize, LengthResolveType::MinSize);
  extent = std::max(extent, min);

  if (style.boxSizing() == BoxSizingContentBox) {
    // TODO(layout-ng): Compute border/padding size and add it
  }

  return extent;
}

NGBoxStrut computeMargins(const NGConstraintSpace& constraintSpace,
                          const ComputedStyle& style) {
  // Margins always get computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-margin-width
  NGBoxStrut margins;
  margins.inline_start =
      resolveInlineLength(constraintSpace, style.marginStart(),
                          LengthResolveType::MarginBorderPaddingSize);
  margins.inline_end =
      resolveInlineLength(constraintSpace, style.marginEnd(),
                          LengthResolveType::MarginBorderPaddingSize);
  margins.block_start =
      resolveInlineLength(constraintSpace, style.marginBefore(),
                          LengthResolveType::MarginBorderPaddingSize);
  margins.block_end =
      resolveInlineLength(constraintSpace, style.marginAfter(),
                          LengthResolveType::MarginBorderPaddingSize);
  return margins;
}

NGBoxStrut computeBorders(const ComputedStyle& style) {
  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.borderStartWidth());
  borders.inline_end = LayoutUnit(style.borderEndWidth());
  borders.block_start = LayoutUnit(style.borderBeforeWidth());
  borders.block_end = LayoutUnit(style.borderAfterWidth());
  return borders;
}

NGBoxStrut computePadding(const NGConstraintSpace& constraintSpace,
                          const ComputedStyle& style) {
  // Padding always gets computed relative to the inline size:
  // https://www.w3.org/TR/CSS2/box.html#value-def-padding-width
  NGBoxStrut padding;
  padding.inline_start =
      resolveInlineLength(constraintSpace, style.paddingStart(),
                          LengthResolveType::MarginBorderPaddingSize);
  padding.inline_end =
      resolveInlineLength(constraintSpace, style.paddingEnd(),
                          LengthResolveType::MarginBorderPaddingSize);
  padding.block_start =
      resolveInlineLength(constraintSpace, style.paddingBefore(),
                          LengthResolveType::MarginBorderPaddingSize);
  padding.block_end =
      resolveInlineLength(constraintSpace, style.paddingAfter(),
                          LengthResolveType::MarginBorderPaddingSize);
  return padding;
}

}  // namespace blink

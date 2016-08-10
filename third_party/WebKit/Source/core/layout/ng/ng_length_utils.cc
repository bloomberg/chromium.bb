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

LayoutUnit resolveInlineLength(LengthResolveType type,
                               const Length& length,
                               const NGConstraintSpace& constraintSpace) {
  DCHECK(!length.isMaxSizeNone());
  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();
  // TODO(layout-ng): Handle min/max/fit-content
  return valueForLength(length, constraintSpace.inlineContainerSize());
}

LayoutUnit resolveBlockLength(LengthResolveType type,
                              const Length& length,
                              const NGConstraintSpace& constraintSpace,
                              LayoutUnit contentContribution) {
  DCHECK(!length.isMaxSizeNone());
  if (type == LengthResolveType::MinSize && length.isAuto())
    return LayoutUnit();
  if (length.isAuto())
    return contentContribution;
  if (length.isMinContent() || length.isMaxContent() || length.isFitContent())
    return contentContribution;
  return valueForLength(length, constraintSpace.blockContainerSize());
}

LayoutUnit computeInlineSizeForFragment(
    const NGConstraintSpace& constraintSpace,
    const ComputedStyle& style) {
  LayoutUnit extent = resolveInlineLength(
      LengthResolveType::ContentSize, style.logicalWidth(), constraintSpace);
  Length maxLength = style.logicalMaxWidth();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max = resolveInlineLength(LengthResolveType::MaxSize, maxLength,
                                         constraintSpace);
    extent = std::min(extent, max);
  }
  LayoutUnit min = resolveInlineLength(
      LengthResolveType::MinSize, style.logicalMinWidth(), constraintSpace);
  extent = std::max(extent, min);
  if (style.boxSizing() == BoxSizingContentBox) {
    // TODO(layout-ng): Compute border/padding size and add it
  }
  return extent;
}

LayoutUnit computeBlockSizeForFragment(const NGConstraintSpace& constraintSpace,
                                       const ComputedStyle& style,
                                       LayoutUnit contentContribution) {
  LayoutUnit extent =
      resolveBlockLength(LengthResolveType::ContentSize, style.logicalHeight(),
                         constraintSpace, contentContribution);
  Length maxLength = style.logicalMaxHeight();
  if (!maxLength.isMaxSizeNone()) {
    LayoutUnit max = resolveBlockLength(LengthResolveType::MaxSize, maxLength,
                                        constraintSpace, contentContribution);
    extent = std::min(extent, max);
  }
  LayoutUnit min =
      resolveBlockLength(LengthResolveType::MinSize, style.logicalMinHeight(),
                         constraintSpace, contentContribution);
  extent = std::max(extent, min);
  if (style.boxSizing() == BoxSizingContentBox) {
    // TODO(layout-ng): Compute border/padding size and add it
  }
  return extent;
}

}  // namespace blink

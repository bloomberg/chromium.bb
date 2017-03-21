// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_space_utils.h"

namespace blink {
namespace {

// Returns max of 2 {@code WTF::Optional} values.
template <typename T>
WTF::Optional<T> OptionalMax(const WTF::Optional<T>& value1,
                             const WTF::Optional<T>& value2) {
  if (value1 && value2) {
    return std::max(value1.value(), value2.value());
  } else if (value1) {
    return value1;
  }
  return value2;
}

}  // namespace

bool IsNewFormattingContextForInFlowBlockLevelChild(
    const NGConstraintSpace& space,
    const ComputedStyle& style) {
  // TODO(layout-dev): This doesn't capture a few cases which can't be computed
  // directly from style yet:
  //  - The child is a <fieldset>.
  //  - "column-span: all" is set on the child (requires knowledge that we are
  //    in a multi-col formatting context).
  //    (https://drafts.csswg.org/css-multicol-1/#valdef-column-span-all)

  if (style.specifiesColumns() || style.containsPaint() ||
      style.containsLayout())
    return true;

  if (!style.isOverflowVisible())
    return true;

  EDisplay display = style.display();
  if (display == EDisplay::kGrid || display == EDisplay::kFlex ||
      display == EDisplay::kWebkitBox)
    return true;

  if (space.WritingMode() != FromPlatformWritingMode(style.getWritingMode()))
    return true;

  return false;
}

WTF::Optional<LayoutUnit> GetClearanceOffset(
    const std::shared_ptr<NGExclusions>& exclusions,
    const ComputedStyle& style) {
  const NGExclusion* right_exclusion = exclusions->last_right_float;
  const NGExclusion* left_exclusion = exclusions->last_left_float;

  WTF::Optional<LayoutUnit> left_offset;
  if (left_exclusion) {
    left_offset = left_exclusion->rect.BlockEndOffset();
  }
  WTF::Optional<LayoutUnit> right_offset;
  if (right_exclusion) {
    right_offset = right_exclusion->rect.BlockEndOffset();
  }

  switch (style.clear()) {
    case EClear::kNone:
      return WTF::nullopt;  // nothing to do here.
    case EClear::kLeft:
      return left_offset;
    case EClear::kRight:
      return right_offset;
    case EClear::kBoth:
      return OptionalMax<LayoutUnit>(left_offset, right_offset);
    default:
      ASSERT_NOT_REACHED();
  }
  return WTF::nullopt;
}

}  // namespace blink

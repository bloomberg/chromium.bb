// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style_ license that can be
// found in the LICENSE file.

#include "core/layout/TextDecorationOffset.h"

#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "platform/fonts/FontMetrics.h"

namespace blink {

int TextDecorationOffset::ComputeUnderlineOffsetForUnder(
    float text_decoration_thickness,
    LineVerticalPositionType position_type) const {
  const RootInlineBox& root = inline_text_box_->Root();
  FontBaseline baseline_type = root.BaselineType();
  LayoutUnit offset = inline_text_box_->OffsetTo(position_type, baseline_type);

  // Compute offset to the farthest position of the decorating box.
  LayoutUnit logical_top = inline_text_box_->LogicalTop();
  LayoutUnit position = logical_top + offset;
  LayoutUnit farthest = root.FarthestPositionForUnderline(
      decorating_box_, position_type, baseline_type, position);
  // Round() looks more logical but Floor() produces better results in
  // positive/negative offsets, in horizontal/vertical flows, on Win/Mac/Linux.
  int offset_int = (farthest - logical_top).Floor();

  // Gaps are not needed for TextTop because it generally has internal
  // leadings.
  if (position_type == LineVerticalPositionType::TextTop)
    return offset_int;
  return !IsLineOverSide(position_type) ? offset_int + 1 : offset_int - 1;
}

}  // namespace blink

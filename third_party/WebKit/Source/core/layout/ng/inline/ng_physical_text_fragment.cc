// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_physical_text_fragment.h"

#include "core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGPhysicalOffsetRect NGPhysicalTextFragment::LocalVisualRect() const {
  if (!shape_result_)
    return {};

  // TODO(kojii): Copying InlineTextBox logic from
  // InlineFlowBox::ComputeOverflow().
  //
  // InlineFlowBox::ComputeOverflow() computes GlpyhOverflow first
  // (ComputeGlyphOverflow) then AddTextBoxVisualOverflow(). We probably don't
  // have to keep these two steps separated.

  // Glyph bounds is in logical coordinate, origin at the baseline.
  FloatRect visual_float_rect = shape_result_->Bounds();

  // Make the origin at the logical top of this fragment.
  // ShapeResult::Bounds() is in logical coordinate with alphabetic baseline.
  const ComputedStyle& style = Style();
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  visual_float_rect.SetY(
      visual_float_rect.Y() +
      font_data->GetFontMetrics().FixedAscent(kAlphabeticBaseline));

  // TODO(kojii): Copying from AddTextBoxVisualOverflow()
  if (float stroke_width = style.TextStrokeWidth()) {
    visual_float_rect.Inflate(stroke_width / 2.0f);
  }

  // TODO(kojii): Implement emphasis marks.

  if (ShadowList* text_shadow = style.TextShadow()) {
    // TODO(kojii): Implement text shadow.
  }

  LayoutRect visual_rect = EnclosingLayoutRect(visual_float_rect);
  switch (LineOrientation()) {
    case NGLineOrientation::kHorizontal:
      return NGPhysicalOffsetRect(visual_rect);
    case NGLineOrientation::kClockWiseVertical:
      return {{size_.width - visual_rect.MaxY(), visual_rect.X()},
              {visual_rect.Height(), visual_rect.Width()}};
    case NGLineOrientation::kCounterClockWiseVertical:
      return {{visual_rect.Y(), size_.height - visual_rect.MaxX()},
              {visual_rect.Height(), visual_rect.Width()}};
  }
  NOTREACHED();
  return {};
}

}  // namespace blink

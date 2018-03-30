// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_physical_text_fragment.h"

#include "core/dom/Node.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/geometry/ng_logical_rect.h"
#include "core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/inline/ng_offset_mapping.h"
#include "core/style/ComputedStyle.h"

namespace blink {

LayoutUnit NGPhysicalTextFragment::InlinePositionForOffset(
    unsigned offset) const {
  DCHECK_GE(offset, start_offset_);
  DCHECK_LE(offset, end_offset_);

  offset -= start_offset_;
  if (shape_result_) {
    return LayoutUnit::FromFloatRound(shape_result_->PositionForOffset(offset));
  }

  // All non-flow controls have ShapeResult.
  DCHECK(IsFlowControl());
  DCHECK_EQ(1u, Length());
  if (!offset || UNLIKELY(IsRtl(Style().Direction())))
    return LayoutUnit();
  return IsHorizontal() ? Size().width : Size().height;
}

NGPhysicalOffsetRect NGPhysicalTextFragment::LocalRect(
    unsigned start_offset,
    unsigned end_offset) const {
  DCHECK_LE(start_offset, end_offset);
  DCHECK_GE(start_offset, start_offset_);
  DCHECK_LE(end_offset, end_offset_);

  if (UNLIKELY(!shape_result_)) {
    DCHECK(IsFlowControl());
    DCHECK_EQ(Length(), 1u);
    return {{}, Size()};
  }

  if (UNLIKELY(IsRtl(shape_result_->Direction())))
    std::swap(start_offset, end_offset);
  LayoutUnit start_position = LayoutUnit::FromFloatFloor(
      shape_result_->PositionForOffset(start_offset - start_offset_));
  LayoutUnit end_position = LayoutUnit::FromFloatCeil(
      shape_result_->PositionForOffset(end_offset - start_offset_));
  DCHECK_GE(end_position, start_position);
  LayoutUnit inline_size = end_position - start_position;
  switch (LineOrientation()) {
    case NGLineOrientation::kHorizontal:
      return {{start_position, LayoutUnit()}, {inline_size, Size().height}};
    case NGLineOrientation::kClockWiseVertical:
      return {{LayoutUnit(), start_position}, {Size().width, inline_size}};
    case NGLineOrientation::kCounterClockWiseVertical:
      return {{LayoutUnit(), Size().height - end_position},
              {Size().width, inline_size}};
  }
  NOTREACHED();
  return {};
}

NGPhysicalOffsetRect NGPhysicalTextFragment::SelfVisualRect() const {
  if (!shape_result_)
    return {};

  // Glyph bounds is in logical coordinate, origin at the alphabetic baseline.
  LayoutRect visual_rect = EnclosingLayoutRect(shape_result_->Bounds());

  // Make the origin at the logical top of this fragment.
  const ComputedStyle& style = Style();
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  if (font_data) {
    visual_rect.SetY(visual_rect.Y() + font_data->GetFontMetrics().FixedAscent(
                                           kAlphabeticBaseline));
  }

  if (float stroke_width = style.TextStrokeWidth()) {
    visual_rect.Inflate(LayoutUnit::FromFloatCeil(stroke_width / 2.0f));
  }

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    LayoutUnit emphasis_mark_height =
        LayoutUnit(font.EmphasisMarkHeight(style.TextEmphasisMarkString()));
    DCHECK_GT(emphasis_mark_height, LayoutUnit());
    if (style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver) {
      visual_rect.ShiftYEdgeTo(
          std::min(visual_rect.Y(), -emphasis_mark_height));
    } else {
      LayoutUnit logical_height =
          style.IsHorizontalWritingMode() ? Size().height : Size().width;
      visual_rect.ShiftMaxYEdgeTo(
          std::max(visual_rect.MaxY(), logical_height + emphasis_mark_height));
    }
  }

  if (ShadowList* text_shadow = style.TextShadow()) {
    LayoutRectOutsets text_shadow_logical_outsets =
        LayoutRectOutsets(text_shadow->RectOutsetsIncludingOriginal())
            .LineOrientationOutsets(style.GetWritingMode());
    text_shadow_logical_outsets.ClampNegativeToZero();
    visual_rect.Expand(text_shadow_logical_outsets);
  }

  visual_rect = LayoutRect(EnclosingIntRect(visual_rect));

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

scoped_refptr<NGPhysicalFragment> NGPhysicalTextFragment::CloneWithoutOffset()
    const {
  return base::AdoptRef(new NGPhysicalTextFragment(
      layout_object_, Style(), static_cast<NGStyleVariant>(style_variant_),
      TextType(), text_, start_offset_, end_offset_, size_, LineOrientation(),
      EndEffect(), shape_result_));
}

bool NGPhysicalTextFragment::IsAnonymousText() const {
  // TODO(xiaochengh): Introduce and set a flag for anonymous text.
  const LayoutObject* layout_object = GetLayoutObject();
  if (layout_object && layout_object->IsText() &&
      ToLayoutText(layout_object)->IsTextFragment())
    return !ToLayoutTextFragment(layout_object)->AssociatedTextNode();
  const Node* node = GetNode();
  return !node || node->IsPseudoElement();
}

unsigned NGPhysicalTextFragment::TextOffsetForPoint(
    const NGPhysicalOffset& point) const {
  if (IsLineBreak())
    return StartOffset();
  DCHECK(TextShapeResult());
  const LayoutUnit& point_in_line_direction =
      Style().IsHorizontalWritingMode() ? point.left : point.top;
  const bool include_partial_glyphs = true;
  return TextShapeResult()->OffsetForPosition(point_in_line_direction.ToFloat(),
                                              include_partial_glyphs) +
         StartOffset();
}

PositionWithAffinity NGPhysicalTextFragment::PositionForPoint(
    const NGPhysicalOffset& point) const {
  if (IsAnonymousText())
    return PositionWithAffinity();
  const unsigned text_offset = TextOffsetForPoint(point);
  const Position position =
      NGOffsetMapping::GetFor(GetLayoutObject())->GetFirstPosition(text_offset);
  // TODO(xiaochengh): Adjust TextAffinity.
  return PositionWithAffinity(position, TextAffinity::kDownstream);
}

}  // namespace blink

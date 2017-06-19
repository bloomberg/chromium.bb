/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutTextCombine.h"

#include "platform/graphics/GraphicsContext.h"

namespace blink {

const float kTextCombineMargin = 1.1f;  // Allow em + 10% margin

LayoutTextCombine::LayoutTextCombine(Node* node, PassRefPtr<StringImpl> string)
    : LayoutText(node, std::move(string)),
      combined_text_width_(0),
      scale_x_(1.0f),
      is_combined_(false),
      needs_font_update_(false) {}

void LayoutTextCombine::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  SetStyleInternal(ComputedStyle::Clone(StyleRef()));
  LayoutText::StyleDidChange(diff, old_style);

  UpdateIsCombined();
}

void LayoutTextCombine::SetTextInternal(RefPtr<StringImpl> text) {
  LayoutText::SetTextInternal(std::move(text));

  UpdateIsCombined();
}

float LayoutTextCombine::Width(unsigned from,
                               unsigned length,
                               const Font& font,
                               LayoutUnit x_position,
                               TextDirection direction,
                               HashSet<const SimpleFontData*>* fallback_fonts,
                               FloatRect* glyph_bounds) const {
  if (!length)
    return 0;

  if (HasEmptyText())
    return 0;

  if (is_combined_)
    return font.GetFontDescription().ComputedSize();

  return LayoutText::Width(from, length, font, x_position, direction,
                           fallback_fonts, glyph_bounds);
}

void ScaleHorizontallyAndTranslate(GraphicsContext& context,
                                   float scale_x,
                                   float center_x,
                                   float offset_x,
                                   float offset_y) {
  context.ConcatCTM(AffineTransform(
      scale_x, 0, 0, 1, center_x * (1.0f - scale_x) + offset_x * scale_x,
      offset_y));
}

void LayoutTextCombine::TransformToInlineCoordinates(GraphicsContext& context,
                                                     const LayoutRect& box_rect,
                                                     bool clip) const {
  DCHECK_EQ(needs_font_update_, false);
  DCHECK(is_combined_);

  // On input, the |boxRect| is:
  // 1. Horizontal flow, rotated from the main vertical flow coordinate using
  //    TextPainter::rotation().
  // 2. height() is cell-height, which includes internal leading. This equals
  //    to A+D, and to em+internal leading.
  // 3. width() is the same as m_combinedTextWidth.
  // 4. Left is (right-edge - height()).
  // 5. Top is where char-top (not include internal leading) should be.
  // See https://support.microsoft.com/en-us/kb/32667.
  // We move it so that it comes to the center of em excluding internal
  // leading.

  float cell_height = box_rect.Height();
  float internal_leading =
      StyleRef().GetFont().PrimaryFont()->InternalLeading();
  float offset_y = -internal_leading / 2;
  float width;
  if (scale_x_ >= 1.0f) {
    // Fast path, more than 90% of cases
    DCHECK_EQ(scale_x_, 1.0f);
    float offset_x = (cell_height - combined_text_width_) / 2;
    context.ConcatCTM(AffineTransform::Translation(offset_x, offset_y));
    width = box_rect.Width();
  } else {
    DCHECK_GE(scale_x_, 0.0f);
    float center_x = box_rect.X() + cell_height / 2;
    width = combined_text_width_ / scale_x_;
    float offset_x = (cell_height - width) / 2;
    ScaleHorizontallyAndTranslate(context, scale_x_, center_x, offset_x,
                                  offset_y);
  }

  if (clip)
    context.Clip(FloatRect(box_rect.X(), box_rect.Y(), width, cell_height));
}

void LayoutTextCombine::UpdateIsCombined() {
  // CSS3 spec says text-combine works only in vertical writing mode.
  is_combined_ = !Style()->IsHorizontalWritingMode()
                 // Nothing to combine.
                 && !HasEmptyText();

  if (is_combined_)
    needs_font_update_ = true;
}

void LayoutTextCombine::UpdateFont() {
  if (!needs_font_update_)
    return;

  needs_font_update_ = false;

  if (!is_combined_)
    return;

  unsigned offset = 0;
  TextRun run = ConstructTextRun(OriginalFont(), this, offset, TextLength(),
                                 StyleRef(), Style()->Direction());
  FontDescription description = OriginalFont().GetFontDescription();
  float em_width = description.ComputedSize();
  if (!EnumHasFlags(Style()->TextDecorationsInEffect(),
                    TextDecoration::kUnderline | TextDecoration::kOverline))
    em_width *= kTextCombineMargin;

  // We are going to draw combined text horizontally.
  description.SetOrientation(FontOrientation::kHorizontal);
  combined_text_width_ = OriginalFont().Width(run);

  FontSelector* font_selector = Style()->GetFont().GetFontSelector();

  bool should_update_font = MutableStyleRef().SetFontDescription(
      description);  // Need to change font orientation to horizontal.

  if (combined_text_width_ <= em_width) {
    scale_x_ = 1.0f;
  } else {
    // Need to try compressed glyphs.
    static const FontWidthVariant kWidthVariants[] = {kHalfWidth, kThirdWidth,
                                                      kQuarterWidth};
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(kWidthVariants); ++i) {
      description.SetWidthVariant(kWidthVariants[i]);
      Font compressed_font = Font(description);
      compressed_font.Update(font_selector);
      float run_width = compressed_font.Width(run);
      if (run_width <= em_width) {
        combined_text_width_ = run_width;

        // Replace my font with the new one.
        should_update_font = MutableStyleRef().SetFontDescription(description);
        break;
      }
    }

    // If width > ~1em, shrink to fit within ~1em, otherwise render without
    // scaling (no expansion).
    // http://dev.w3.org/csswg/css-writing-modes-3/#text-combine-compression
    if (combined_text_width_ > em_width) {
      scale_x_ = em_width / combined_text_width_;
      combined_text_width_ = em_width;
    } else {
      scale_x_ = 1.0f;
    }
  }

  if (should_update_font)
    Style()->GetFont().Update(font_selector);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_text_fragment_painter.h"

#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_text_decoration_offset.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainterBase.h"
#include "core/paint/ng/ng_paint_fragment.h"
#include "core/paint/ng/ng_text_painter.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/ComputedStyle.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

NGTextFragmentPainter::NGTextFragmentPainter(
    const NGPaintFragment& text_fragment)
    : fragment_(text_fragment) {
  DCHECK(text_fragment.PhysicalFragment().IsText());
}

void NGTextFragmentPainter::Paint(const Document& document,
                                  const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) {
  const ComputedStyle& style = fragment_.Style();

  NGPhysicalSize size_;
  NGPhysicalOffset offset_;

  // We round the y-axis to ensure consistent line heights.
  LayoutPoint adjusted_paint_offset =
      LayoutPoint(paint_offset.X(), LayoutUnit(paint_offset.Y().Round()));

  NGPhysicalOffset offset = fragment_.Offset();
  LayoutPoint box_origin(offset.left, offset.top);
  box_origin.Move(adjusted_paint_offset.X(), adjusted_paint_offset.Y());

  LayoutRect box_rect(
      box_origin, LayoutSize(fragment_.Size().width, fragment_.Size().height));

  GraphicsContext& context = paint_info.context;

  bool is_printing = paint_info.IsPrinting();

  // Determine whether or not we're selected.
  bool have_selection = false;  // TODO(layout-dev): Implement.
  if (!have_selection && paint_info.phase == kPaintPhaseSelection) {
    // When only painting the selection, don't bother to paint if there is none.
    return;
  }

  // Determine text colors.
  TextPaintStyle text_style =
      TextPainterBase::TextPaintingStyle(document, style, paint_info);
  TextPaintStyle selection_style = TextPainterBase::SelectionPaintingStyle(
      document, style, fragment_.GetNode(), have_selection, paint_info,
      text_style);
  bool paint_selected_text_only = (paint_info.phase == kPaintPhaseSelection);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  LayoutPoint text_origin(box_origin.X(), box_origin.Y() + ascent);

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  if (paint_info.phase != kPaintPhaseSelection &&
      paint_info.phase != kPaintPhaseTextClip && !is_printing) {
    // TODO(layout-dev): Implement.
  }

  // 2. Now paint the foreground, including text and decorations.
  int selection_start = 0;
  int selection_end = 0;

  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(fragment_.PhysicalFragment());

  NGTextPainter text_painter(context, font, text_fragment, text_origin,
                             box_rect, text_fragment.IsHorizontal());

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  unsigned length = text_fragment.Text().length();
  if (!paint_selected_text_only) {
    // Paint text decorations except line-through.
    DecorationInfo decoration_info;
    bool has_line_through_decoration = false;
    if (style.TextDecorationsInEffect() != TextDecoration::kNone) {
      LayoutPoint local_origin = LayoutPoint(box_origin);
      LayoutUnit width = fragment_.Size().width;
      const NGPhysicalBoxFragment* decorating_box = nullptr;
      const ComputedStyle* decorating_box_style =
          decorating_box ? &decorating_box->Style() : nullptr;

      // TODO(eae): Use correct baseline when available.
      FontBaseline baseline_type = kAlphabeticBaseline;

      text_painter.ComputeDecorationInfo(decoration_info, box_origin,
                                         local_origin, width, baseline_type,
                                         style, decorating_box_style);

      NGTextDecorationOffset decoration_offset(*decoration_info.style,
                                               text_fragment, decorating_box);
      text_painter.PaintDecorationsExceptLineThrough(
          decoration_offset, decoration_info, paint_info,
          style.AppliedTextDecorations(), text_style,
          &has_line_through_decoration);
    }

    int start_offset = 0;
    int end_offset = length;

    if (paint_selected_text_separately && selection_start < selection_end) {
      start_offset = selection_end;
      end_offset = selection_start;
    }

    text_painter.Paint(start_offset, end_offset, length, text_style);

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info, paint_info, style.AppliedTextDecorations(),
          text_style);
    }
  }

  if ((paint_selected_text_only || paint_selected_text_separately) &&
      selection_start < selection_end) {
    // Paint only the text that is selected
    text_painter.Paint(selection_start, selection_end, length, selection_style);
  }
}

}  // namespace blink

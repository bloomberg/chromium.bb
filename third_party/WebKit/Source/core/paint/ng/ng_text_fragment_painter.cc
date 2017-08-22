// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_text_fragment_painter.h"

#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainterBase.h"
#include "core/paint/ng/ng_text_painter.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/ComputedStyle.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

namespace {

static void PaintDecorationsExceptLineThrough(
    NGTextPainter& text_painter,
    bool& has_line_through_decoration,
    const NGPhysicalTextFragment& fragment,
    const DecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  context.SetStrokeThickness(decoration_info.thickness);

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underline_position =
      decoration_info.underline_position;
  bool flip_underline_and_overline = false;
  if (underline_position == ResolvedUnderlinePosition::kOver) {
    flip_underline_and_overline = true;
    underline_position = ResolvedUnderlinePosition::kUnder;
  }

  // TODO(layout-dev): Add support for text decorations.
}

}  // anonymous namespace

void NGTextFragmentPainter::Paint(const Document& document,
                                  const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) {
  // TODO(eae): These constants are currently defined in core/layout/line/
  // InlineTextBox.h, move them to a separate header and have InlineTextBox.h
  // and this file include it.
  static unsigned short kCNoTruncation = USHRT_MAX;
  static unsigned short kCFullTruncation = USHRT_MAX - 1;

  const ComputedStyle& style_to_use = fragment_.Style();

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
  TextPainterBase::Style text_style =
      TextPainterBase::TextPaintingStyle(document, style_to_use, paint_info);
  // TODO(layout-dev): Implement.
  TextPainterBase::Style selection_style = text_style;
  bool paint_selected_text_only = (paint_info.phase == kPaintPhaseSelection);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style_to_use.GetFont();
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

  // TODO(layout-dev): Add hyphen support.

  unsigned length = fragment_.Text().length();
  unsigned truncation = kCNoTruncation;

  bool ltr = true;
  bool flow_is_ltr = true;
  if (truncation != kCNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // rather than at the end of it.
    selection_start = ltr == flow_is_ltr
                          ? std::min<int>(selection_start, truncation)
                          : std::max<int>(selection_start, truncation);
    selection_end = ltr == flow_is_ltr
                        ? std::min<int>(selection_end, truncation)
                        : std::max<int>(selection_end, truncation);
    length = ltr == flow_is_ltr ? truncation : length;
  }

  NGTextPainter text_painter(context, font, fragment_, text_origin, box_rect,
                             fragment_.IsHorizontal());
  TextEmphasisPosition emphasis_mark_position;
  bool has_text_emphasis = false;  // TODO(layout-dev): Implement.
  emphasis_mark_position = TextEmphasisPosition::kOver;
  if (has_text_emphasis) {
    text_painter.SetEmphasisMark(style_to_use.TextEmphasisMarkString(),
                                 emphasis_mark_position);
  }
  if (truncation != kCNoTruncation && ltr != flow_is_ltr)
    text_painter.SetEllipsisOffset(truncation);

  if (!paint_selected_text_only) {
    // Paint text decorations except line-through.
    DecorationInfo decoration_info;
    bool has_line_through_decoration = false;
    if (style_to_use.TextDecorationsInEffect() != TextDecoration::kNone &&
        truncation != kCFullTruncation) {
      LayoutPoint local_origin = LayoutPoint(box_origin);
      LayoutUnit width = fragment_.Size().width;
      const ComputedStyle* decorating_box_style = nullptr;
      // TODO(layout-dev): Implement.
      FontBaseline baseline_type = kAlphabeticBaseline;

      text_painter.ComputeDecorationInfo(decoration_info, box_origin,
                                         local_origin, width, baseline_type,
                                         style_to_use, decorating_box_style);
      GraphicsContextStateSaver state_saver(context, false);
      TextPainterBase::UpdateGraphicsContext(
          context, text_style, fragment_.IsHorizontal(), state_saver);

      PaintDecorationsExceptLineThrough(
          text_painter, has_line_through_decoration, fragment_, decoration_info,
          paint_info, style_to_use.AppliedTextDecorations());
    }

    int start_offset = 0;
    int end_offset = length;
    // Where the text and its flow have opposite directions then our offset into
    // the text given by |truncation| is at the start of the part that will be
    // visible.
    if (truncation != kCNoTruncation && ltr != flow_is_ltr) {
      start_offset = truncation;
      end_offset = length;
    }

    if (paint_selected_text_separately && selection_start < selection_end) {
      start_offset = selection_end;
      end_offset = selection_start;
    }

    text_painter.Paint(start_offset, end_offset, length, text_style);

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      GraphicsContextStateSaver state_saver(context, false);
      TextPainterBase::UpdateGraphicsContext(
          context, text_style, fragment_.IsHorizontal(), state_saver);
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info, paint_info, style_to_use.AppliedTextDecorations());
    }
  }

  if ((paint_selected_text_only || paint_selected_text_separately) &&
      selection_start < selection_end) {
    // Paint only the text that is selected
    text_painter.Paint(selection_start, selection_end, length, selection_style);
  }
}

}  // namespace blink

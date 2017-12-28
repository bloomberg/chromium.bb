// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_text_fragment_painter.h"

#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_text_decoration_offset.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SelectionPaintingUtils.h"
#include "core/paint/TextPainterBase.h"
#include "core/paint/ng/ng_paint_fragment.h"
#include "core/paint/ng/ng_text_painter.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

namespace {

inline bool ShouldPaintTextFragment(const NGPhysicalTextFragment& text_fragment,
                                    const ComputedStyle& style) {
  if (style.Visibility() != EVisibility::kVisible)
    return false;

  // When painting selection, we want to include a highlight when the
  // selection spans line breaks. In other cases such as invisible elements
  // or those with no text that are not line breaks, we can skip painting
  // wholesale.
  // TODO(wkorman): Constrain line break painting to appropriate paint phase.
  // This code path is only called in PaintPhaseForeground whereas we would
  // expect PaintPhaseSelection. The existing haveSelection logic in paint()
  // tests for != PaintPhaseTextClip.
  if (!text_fragment.Length() || !text_fragment.TextShapeResult())
    return false;

  return true;
}

// Copied from Font.cpp
inline FloatRect PixelSnappedSelectionRect(FloatRect rect) {
  // Using roundf() rather than ceilf() for the right edge as a compromise to
  // ensure correct caret positioning.
  float rounded_x = roundf(rect.X());
  return FloatRect(rounded_x, rect.Y(), roundf(rect.MaxX() - rounded_x),
                   rect.Height());
}

Color SelectionBackgroundColor(const Document& document,
                               const ComputedStyle& style,
                               const NGPhysicalTextFragment& text_fragment,
                               Color text_color) {
  const Color color = SelectionPaintingUtils::SelectionBackgroundColor(
      document, style, text_fragment.GetNode());
  if (!color.Alpha())
    return Color();

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == color)
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  return color;
}

}  // namespace

NGTextFragmentPainter::NGTextFragmentPainter(
    const NGPaintFragment& text_fragment)
    : fragment_(text_fragment) {
  DCHECK(text_fragment.PhysicalFragment().IsText());
}

// Logic is copied from InlineTextBoxPainter::PaintSelection.
// |selection_start| and |selection_end| should be between
// [text_fragment.StartOffset(), text_fragment.EndOffset()].
static void PaintSelection(GraphicsContext& context,
                           const NGPhysicalTextFragment& text_fragment,
                           const Document& document,
                           const ComputedStyle& style,
                           Color text_color,
                           const LayoutRect& box_rect,
                           unsigned int selection_start,
                           unsigned int selection_end) {
  const Color color =
      SelectionBackgroundColor(document, style, text_fragment, text_color);
  if (!color.Alpha())
    return;

  GraphicsContextStateSaver state_saver(context);

  DCHECK_LE(text_fragment.StartOffset(), selection_start);
  DCHECK_LE(text_fragment.StartOffset(), selection_end);
  DCHECK_GE(text_fragment.EndOffset(), selection_start);
  DCHECK_GE(text_fragment.EndOffset(), selection_end);
  const ShapeResult* shape_result = text_fragment.TextShapeResult();
  const CharacterRange& range = shape_result->GetCharacterRange(
      selection_start - text_fragment.StartOffset(),
      selection_end - text_fragment.StartOffset());
  const FloatRect& selection_rect = PixelSnappedSelectionRect(
      FloatRect(box_rect.Location().X() + range.start, box_rect.Location().Y(),
                range.Width(), box_rect.Height().ToFloat()));
  context.FillRect(selection_rect, color);
}

// This is copied from InlineTextBoxPainter::PaintSelection() but lacks of
// ltr, expanding new line wrap or so which uses InlineTextBox functions.
void NGTextFragmentPainter::Paint(const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) {
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(fragment_.PhysicalFragment());
  const ComputedStyle& style = fragment_.Style();
  const Document& document = fragment_.GetLayoutObject()->GetDocument();

  if (!ShouldPaintTextFragment(text_fragment, style))
    return;

  NGPhysicalSize size_;
  NGPhysicalOffset offset_;

  // We round the y-axis to ensure consistent line heights.
  LayoutPoint adjusted_paint_offset =
      LayoutPoint(paint_offset.X(), LayoutUnit(paint_offset.Y().Round()));

  NGPhysicalOffset offset = fragment_.Offset();
  LayoutPoint box_origin(offset.left, offset.top);
  box_origin.Move(adjusted_paint_offset.X(), adjusted_paint_offset.Y());

  GraphicsContext& context = paint_info.context;

  bool is_printing = paint_info.IsPrinting();

  // Determine whether or not we're selected.
  int selection_start;
  int selection_end;
  std::tie(selection_start, selection_end) =
      document.GetFrame()->Selection().LayoutSelectionStartEndForNG(
          text_fragment);
  DCHECK_LE(selection_start, selection_end);
  const bool have_selection = selection_start < selection_end;
  if (!have_selection && paint_info.phase == PaintPhase::kSelection) {
    // When only painting the selection, don't bother to paint if there is none.
    return;
  }

  // Determine text colors.
  TextPaintStyle text_style =
      TextPainterBase::TextPaintingStyle(document, style, paint_info);
  TextPaintStyle selection_style = TextPainterBase::SelectionPaintingStyle(
      document, style, fragment_.GetNode(), have_selection, paint_info,
      text_style);
  bool paint_selected_text_only = (paint_info.phase == PaintPhase::kSelection);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  LayoutRect box_rect(box_origin, fragment_.Size().ToLayoutSize());
  Optional<GraphicsContextStateSaver> state_saver;
  NGLineOrientation orientation = text_fragment.LineOrientation();
  if (orientation != NGLineOrientation::kHorizontal) {
    state_saver.emplace(context);
    // Because we rotate the GraphicsContext to be logical, flip the
    // |box_rect| to match to it.
    box_rect.SetSize(
        LayoutSize(fragment_.Size().height, fragment_.Size().width));
    context.ConcatCTM(TextPainterBase::Rotation(
        box_rect, orientation == NGLineOrientation::kClockWiseVertical
                      ? TextPainterBase::kClockwise
                      : TextPainterBase::kCounterclockwise));
  }

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  if (have_selection && paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing) {
    // TODO(yoichio): Implement composition highlights.
    PaintSelection(context, text_fragment, document, style,
                   selection_style.fill_color, box_rect, selection_start,
                   selection_end);
  }

  // 2. Now paint the foreground, including text and decorations.
  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  LayoutPoint text_origin(box_origin.X(), box_origin.Y() + ascent);
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
      LayoutUnit width = box_rect.Width();
      const NGPhysicalBoxFragment* decorating_box = nullptr;
      const ComputedStyle* decorating_box_style =
          decorating_box ? &decorating_box->Style() : nullptr;

      const FontDescription& font_description = font.GetFontDescription();
      FontBaseline baseline_type = font_description.IsVerticalAnyUpright()
                                       ? kIdeographicBaseline
                                       : kAlphabeticBaseline;

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

    int start_offset = text_fragment.StartOffset();
    int end_offset = start_offset + length;

    if (have_selection && paint_selected_text_separately) {
      // Paint only the text that is not selected.
      if (start_offset < selection_start)
        text_painter.Paint(start_offset, selection_start, length, text_style);
      if (selection_end < end_offset)
        text_painter.Paint(selection_end, end_offset, length, text_style);
    } else {
      text_painter.Paint(start_offset, end_offset, length, text_style);
    }

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info, paint_info, style.AppliedTextDecorations(),
          text_style);
    }
  }

  if (have_selection &&
      (paint_selected_text_only || paint_selected_text_separately)) {
    // Paint only the text that is selected.
    text_painter.Paint(selection_start, selection_end, length, selection_style);
  }
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TextPainter.h"

#include "core/CSSPropertyNames.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTextCombine.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/paint/AppliedDecorationPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ShadowList.h"
#include "platform/fonts/Font.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/text/TextRun.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

void TextPainter::Paint(unsigned start_offset,
                        unsigned end_offset,
                        unsigned length,
                        const Style& text_style) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(text_style, state_saver);
  if (combined_text_) {
    graphics_context_.Save();
    combined_text_->TransformToInlineCoordinates(graphics_context_,
                                                 text_bounds_);
    PaintInternal<kPaintText>(start_offset, end_offset, length);
    graphics_context_.Restore();
  } else {
    PaintInternal<kPaintText>(start_offset, end_offset, length);
  }

  if (!emphasis_mark_.IsEmpty()) {
    if (text_style.emphasis_mark_color != text_style.fill_color)
      graphics_context_.SetFillColor(text_style.emphasis_mark_color);

    if (combined_text_)
      PaintEmphasisMarkForCombinedText();
    else
      PaintInternal<kPaintEmphasisMark>(start_offset, end_offset, length);
  }
}

template <TextPainter::PaintInternalStep step>
void TextPainter::PaintInternalRun(TextRunPaintInfo& text_run_paint_info,
                                   unsigned from,
                                   unsigned to) {
  DCHECK(from <= text_run_paint_info.run.length());
  DCHECK(to <= text_run_paint_info.run.length());

  text_run_paint_info.from = from;
  text_run_paint_info.to = to;

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, text_run_paint_info, emphasis_mark_,
        FloatPoint(text_origin_) + IntSize(0, emphasis_mark_offset_));
  } else {
    DCHECK(step == kPaintText);
    graphics_context_.DrawText(font_, text_run_paint_info,
                               FloatPoint(text_origin_));
  }
}

template <TextPainter::PaintInternalStep Step>
void TextPainter::PaintInternal(unsigned start_offset,
                                unsigned end_offset,
                                unsigned truncation_point) {
  TextRunPaintInfo text_run_paint_info(run_);
  text_run_paint_info.bounds = FloatRect(text_bounds_);
  if (start_offset <= end_offset) {
    PaintInternalRun<Step>(text_run_paint_info, start_offset, end_offset);
  } else {
    if (end_offset > 0)
      PaintInternalRun<Step>(text_run_paint_info, ellipsis_offset_, end_offset);
    if (start_offset < truncation_point)
      PaintInternalRun<Step>(text_run_paint_info, start_offset,
                             truncation_point);
  }
}

void TextPainter::ClipDecorationsStripe(float upper,
                                        float stripe_width,
                                        float dilation) {
  TextRunPaintInfo text_run_paint_info(run_);
  if (!run_.length())
    return;

  Vector<Font::TextIntercept> text_intercepts;
  font_.GetTextIntercepts(
      text_run_paint_info, graphics_context_.DeviceScaleFactor(),
      graphics_context_.FillFlags(),
      std::make_tuple(upper, upper + stripe_width), text_intercepts);

  DecorationsStripeIntercepts(upper, stripe_width, dilation, text_intercepts);
}

void TextPainter::PaintEmphasisMarkForCombinedText() {
  const SimpleFontData* font_data = font_.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  DCHECK(combined_text_);
  TextRun placeholder_text_run(&kIdeographicFullStopCharacter, 1);
  FloatPoint emphasis_mark_text_origin(
      text_bounds_.X().ToFloat(), text_bounds_.Y().ToFloat() +
                                      font_data->GetFontMetrics().Ascent() +
                                      emphasis_mark_offset_);
  TextRunPaintInfo text_run_paint_info(placeholder_text_run);
  text_run_paint_info.bounds = FloatRect(text_bounds_);
  graphics_context_.ConcatCTM(Rotation(text_bounds_, kClockwise));
  graphics_context_.DrawEmphasisMarks(combined_text_->OriginalFont(),
                                      text_run_paint_info, emphasis_mark_,
                                      emphasis_mark_text_origin);
  graphics_context_.ConcatCTM(Rotation(text_bounds_, kCounterclockwise));
}

TextPainter::Style TextPainter::SelectionPaintingStyle(
    LineLayoutItem line_layout_item,
    bool have_selection,
    const PaintInfo& paint_info,
    const TextPainterBase::Style& text_style) {
  const LayoutObject& layout_object =
      *LineLayoutAPIShim::ConstLayoutObjectFrom(line_layout_item);
  TextPainterBase::Style selection_style = text_style;
  bool uses_text_as_clip = paint_info.phase == kPaintPhaseTextClip;
  bool is_printing = paint_info.IsPrinting();

  if (have_selection) {
    if (!uses_text_as_clip) {
      selection_style.fill_color = layout_object.SelectionForegroundColor(
          paint_info.GetGlobalPaintFlags());
      selection_style.emphasis_mark_color =
          layout_object.SelectionEmphasisMarkColor(
              paint_info.GetGlobalPaintFlags());
    }

    if (const ComputedStyle* pseudo_style =
            layout_object.GetCachedPseudoStyle(kPseudoIdSelection)) {
      selection_style.stroke_color =
          uses_text_as_clip
              ? Color::kBlack
              : layout_object.ResolveColor(*pseudo_style,
                                           CSSPropertyWebkitTextStrokeColor);
      selection_style.stroke_width = pseudo_style->TextStrokeWidth();
      selection_style.shadow =
          uses_text_as_clip ? 0 : pseudo_style->TextShadow();
    }

    // Text shadows are disabled when printing. http://crbug.com/258321
    if (is_printing)
      selection_style.shadow = 0;
  }

  return selection_style;
}

}  // namespace blink

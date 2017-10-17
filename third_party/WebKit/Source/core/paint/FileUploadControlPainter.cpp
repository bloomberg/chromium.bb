// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FileUploadControlPainter.h"

#include "core/layout/LayoutButton.h"
#include "core/layout/LayoutFileUploadControl.h"
#include "core/layout/TextRunConstructor.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

const int kButtonShadowHeight = 2;

void FileUploadControlPainter::PaintObject(const PaintInfo& paint_info,
                                           const LayoutPoint& paint_offset) {
  if (layout_file_upload_control_.Style()->Visibility() !=
      EVisibility::kVisible)
    return;

  // Push a clip.
  Optional<ClipRecorder> clip_recorder;
  if (paint_info.phase == PaintPhase::kForeground ||
      paint_info.phase == PaintPhase::kDescendantBlockBackgroundsOnly) {
    IntRect clip_rect = EnclosingIntRect(LayoutRect(
        LayoutPoint(paint_offset.X() + layout_file_upload_control_.BorderLeft(),
                    paint_offset.Y() + layout_file_upload_control_.BorderTop()),
        layout_file_upload_control_.Size() +
            LayoutSize(LayoutUnit(),
                       -layout_file_upload_control_.BorderWidth() +
                           kButtonShadowHeight)));
    if (clip_rect.IsEmpty())
      return;
    clip_recorder.emplace(paint_info.context, layout_file_upload_control_,
                          DisplayItem::kClipFileUploadControlRect, clip_rect);
  }

  if (paint_info.phase == PaintPhase::kForeground &&
      !LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_file_upload_control_, paint_info.phase)) {
    const String& displayed_filename =
        layout_file_upload_control_.FileTextValue();
    const Font& font = layout_file_upload_control_.Style()->GetFont();
    TextRun text_run = ConstructTextRun(
        font, displayed_filename, layout_file_upload_control_.StyleRef(),
        kRespectDirection | kRespectDirectionOverride);
    text_run.SetExpansionBehavior(TextRun::kAllowTrailingExpansion);

    // Determine where the filename should be placed
    LayoutUnit content_left = paint_offset.X() +
                              layout_file_upload_control_.BorderLeft() +
                              layout_file_upload_control_.PaddingLeft();
    Node* button = layout_file_upload_control_.UploadButton();
    if (!button)
      return;

    int button_width = (button && button->GetLayoutBox())
                           ? button->GetLayoutBox()->PixelSnappedWidth()
                           : 0;
    LayoutUnit button_and_spacing_width(
        button_width + LayoutFileUploadControl::kAfterButtonSpacing);
    float text_width = font.Width(text_run);
    LayoutUnit text_x;
    if (layout_file_upload_control_.Style()->IsLeftToRightDirection())
      text_x = content_left + button_and_spacing_width;
    else
      text_x =
          LayoutUnit(content_left + layout_file_upload_control_.ContentWidth() -
                     button_and_spacing_width - text_width);

    LayoutUnit text_y;
    // We want to match the button's baseline
    // FIXME: Make this work with transforms.
    if (LayoutButton* button_layout_object =
            ToLayoutButton(button->GetLayoutObject()))
      text_y = paint_offset.Y() + layout_file_upload_control_.BorderTop() +
               layout_file_upload_control_.PaddingTop() +
               button_layout_object->BaselinePosition(
                   kAlphabeticBaseline, true, kHorizontalLine,
                   kPositionOnContainingLine);
    else
      text_y = LayoutUnit(layout_file_upload_control_.BaselinePosition(
          kAlphabeticBaseline, true, kHorizontalLine,
          kPositionOnContainingLine));
    TextRunPaintInfo text_run_paint_info(text_run);

    const SimpleFontData* font_data =
        layout_file_upload_control_.Style()->GetFont().PrimaryFont();
    if (!font_data)
      return;
    // FIXME: Shouldn't these offsets be rounded? crbug.com/350474
    text_run_paint_info.bounds =
        FloatRect(text_x.ToFloat(),
                  text_y.ToFloat() - font_data->GetFontMetrics().Ascent(),
                  text_width, font_data->GetFontMetrics().Height());

    // Draw the filename.
    LayoutObjectDrawingRecorder recorder(
        paint_info.context, layout_file_upload_control_, paint_info.phase,
        text_run_paint_info.bounds);
    paint_info.context.SetFillColor(
        layout_file_upload_control_.ResolveColor(CSSPropertyColor));
    paint_info.context.DrawBidiText(
        font, text_run_paint_info,
        FloatPoint(RoundToInt(text_x), RoundToInt(text_y)));
  }

  // Paint the children.
  layout_file_upload_control_.LayoutBlockFlow::PaintObject(paint_info,
                                                           paint_offset);
}

}  // namespace blink

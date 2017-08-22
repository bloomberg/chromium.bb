// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_text_painter.h"

#include "core/CSSPropertyNames.h"
#include "core/frame/Settings.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ShadowList.h"
#include "platform/fonts/Font.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

void NGTextPainter::Paint(unsigned start_offset,
                          unsigned end_offset,
                          unsigned length,
                          const Style& text_style) {
  GraphicsContextStateSaver state_saver(graphics_context_, false);
  UpdateGraphicsContext(text_style, state_saver);
  // TODO(layout-dev): Handle combine text here or elsewhere.
  PaintInternal<kPaintText>(start_offset, end_offset, length);

  // TODO(layout-dev): Handle emphasis marks.
}

template <NGTextPainter::PaintInternalStep step>
void NGTextPainter::PaintInternalFragment(
    TextFragmentPaintInfo& fragment_paint_info,
    unsigned from,
    unsigned to) {
  DCHECK(from <= fragment_paint_info.text.length());
  DCHECK(to <= fragment_paint_info.text.length());

  fragment_paint_info.from = from;
  fragment_paint_info.to = to;

  if (step == kPaintEmphasisMark) {
    graphics_context_.DrawEmphasisMarks(
        font_, fragment_paint_info, emphasis_mark_,
        FloatPoint(text_origin_) + IntSize(0, emphasis_mark_offset_));
  } else {
    DCHECK(step == kPaintText);
    graphics_context_.DrawText(font_, fragment_paint_info,
                               FloatPoint(text_origin_));
  }
}

template <NGTextPainter::PaintInternalStep Step>
void NGTextPainter::PaintInternal(unsigned start_offset,
                                  unsigned end_offset,
                                  unsigned truncation_point) {
  // TODO(layout-dev): We shouldn't be creating text fragments without text.
  if (!fragment_.TextShapeResult())
    return;

  unsigned offset = 0;
  TextFragmentPaintInfo fragment_paint_info = {
      fragment_.Text(), TextDirection::kLtr, offset, fragment_.Text().length(),
      fragment_.TextShapeResult()};

  if (start_offset <= end_offset) {
    PaintInternalFragment<Step>(fragment_paint_info, start_offset, end_offset);
  } else {
    if (end_offset > 0) {
      PaintInternalFragment<Step>(fragment_paint_info, ellipsis_offset_,
                                  end_offset);
    }
    if (start_offset < truncation_point) {
      PaintInternalFragment<Step>(fragment_paint_info, start_offset,
                                  truncation_point);
    }
  }
}

void NGTextPainter::ClipDecorationsStripe(float upper,
                                          float stripe_width,
                                          float dilation) {}

void NGTextPainter::PaintEmphasisMarkForCombinedText() {}

}  // namespace blink

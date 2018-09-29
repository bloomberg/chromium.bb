// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_text_painter.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"

namespace blink {

void SVGTextPainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection)
    return;

  PaintInfo block_info(paint_info);
  block_info.UpdateCullRect(layout_svg_text_.LocalToSVGParentTransform());
  ScopedSVGTransformState transform_state(
      block_info, layout_svg_text_,
      layout_svg_text_.LocalToSVGParentTransform());

  if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled())
    RecordHitTestData(paint_info);
  BlockPainter(layout_svg_text_).Paint(block_info);

  // Paint the outlines, if any
  if (paint_info.phase == PaintPhase::kForeground) {
    block_info.phase = PaintPhase::kOutline;
    BlockPainter(layout_svg_text_).Paint(block_info);
  }
}

void SVGTextPainter::RecordHitTestData(const PaintInfo& paint_info) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  if (paint_info.phase != PaintPhase::kForeground)
    return;

  auto touch_action = layout_svg_text_.EffectiveWhitelistedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  auto rect = LayoutRect(layout_svg_text_.VisualRectInLocalSVGCoordinates());
  HitTestData::RecordHitTestRect(paint_info.context, layout_svg_text_,
                                 HitTestRect(rect, touch_action));
}

}  // namespace blink

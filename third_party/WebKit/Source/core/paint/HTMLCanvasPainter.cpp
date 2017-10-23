// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPainter.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/ScopedInterpolationQuality.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"

namespace blink {

namespace {

InterpolationQuality InterpolationQualityForCanvas(const ComputedStyle& style) {
  if (style.ImageRendering() == EImageRendering::kWebkitOptimizeContrast)
    return kInterpolationLow;

  if (style.ImageRendering() == EImageRendering::kPixelated)
    return kInterpolationNone;

  return CanvasDefaultInterpolationQuality;
}

}  // namespace

void HTMLCanvasPainter::PaintReplaced(const PaintInfo& paint_info,
                                      const LayoutPoint& paint_offset) {
  GraphicsContext& context = paint_info.context;

  LayoutRect content_rect = layout_html_canvas_.ContentBoxRect();
  content_rect.MoveBy(paint_offset);
  LayoutRect paint_rect = layout_html_canvas_.ReplacedContentRect();
  paint_rect.MoveBy(paint_offset);

  HTMLCanvasElement* canvas =
      ToHTMLCanvasElement(layout_html_canvas_.GetNode());

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      canvas->RenderingContext() &&
      canvas->RenderingContext()->IsComposited()) {
    if (WebLayer* layer = canvas->RenderingContext()->PlatformLayer()) {
      IntRect pixel_snapped_rect = PixelSnappedIntRect(content_rect);
      RecordForeignLayer(
          context, layout_html_canvas_, DisplayItem::kForeignLayerCanvas, layer,
          pixel_snapped_rect.Location(), pixel_snapped_rect.Size());
      return;
    }
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, layout_html_canvas_,
                                                  paint_info.phase))
    return;

  DrawingRecorder recorder(context, layout_html_canvas_, paint_info.phase,
                           content_rect);

  bool clip = !content_rect.Contains(paint_rect);
  if (clip) {
    context.Save();
    // TODO(chrishtr): this should be pixel-snapped.
    context.Clip(FloatRect(content_rect));
  }

  {
    ScopedInterpolationQuality interpolation_quality_scope(
        context, InterpolationQualityForCanvas(layout_html_canvas_.StyleRef()));
    canvas->Paint(context, paint_rect);
  }

  if (clip)
    context.Restore();
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPainter.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"

namespace blink {

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

  // FIXME: InterpolationNone should be used if ImageRenderingOptimizeContrast
  // is set.  See bug for more details: crbug.com/353716.
  InterpolationQuality interpolation_quality =
      layout_html_canvas_.Style()->ImageRendering() ==
              EImageRendering::kWebkitOptimizeContrast
          ? kInterpolationLow
          : CanvasDefaultInterpolationQuality;
  if (layout_html_canvas_.Style()->ImageRendering() ==
      EImageRendering::kPixelated)
    interpolation_quality = kInterpolationNone;

  InterpolationQuality previous_interpolation_quality =
      context.ImageInterpolationQuality();
  context.SetImageInterpolationQuality(interpolation_quality);
  canvas->Paint(context, paint_rect);
  context.SetImageInterpolationQuality(previous_interpolation_quality);

  if (clip)
    context.Restore();
}

}  // namespace blink

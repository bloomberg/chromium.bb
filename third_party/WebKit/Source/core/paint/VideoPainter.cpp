// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/VideoPainter.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/layout/LayoutVideo.h"
#include "core/paint/ImagePainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"

namespace blink {

void VideoPainter::PaintReplaced(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  WebMediaPlayer* media_player =
      layout_video_.MediaElement()->GetWebMediaPlayer();
  bool displaying_poster =
      layout_video_.VideoElement()->ShouldDisplayPosterImage();
  if (!displaying_poster && !media_player)
    return;

  LayoutRect replaced_rect(layout_video_.ReplacedContentRect());
  replaced_rect.MoveBy(paint_offset);
  IntRect snapped_replaced_rect = PixelSnappedIntRect(replaced_rect);

  if (snapped_replaced_rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_video_, paint_info.phase))
    return;

  GraphicsContext& context = paint_info.context;
  LayoutRect content_rect = layout_video_.ContentBoxRect();
  content_rect.MoveBy(paint_offset);

  // Video frames are only painted in software for printing or capturing node
  // images via web APIs.
  bool force_software_video_paint =
      paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers;

  bool paint_with_foreign_layer =
      !displaying_poster && !force_software_video_paint &&
      RuntimeEnabledFeatures::SlimmingPaintV2Enabled();
  if (paint_with_foreign_layer) {
    if (WebLayer* layer = layout_video_.MediaElement()->PlatformLayer()) {
      IntRect pixel_snapped_rect = PixelSnappedIntRect(content_rect);
      RecordForeignLayer(
          context, layout_video_, DisplayItem::kForeignLayerVideo, layer,
          pixel_snapped_rect.Location(), pixel_snapped_rect.Size());
      return;
    }
  }

  // TODO(trchen): Video rect could overflow the content rect due to object-fit.
  // Should apply a clip here like EmbeddedObjectPainter does.
  DrawingRecorder recorder(context, layout_video_, paint_info.phase,
                           content_rect);

  if (displaying_poster || !force_software_video_paint) {
    // This will display the poster image, if one is present, and otherwise
    // paint nothing.
    ImagePainter(layout_video_)
        .PaintIntoRect(context, replaced_rect, content_rect);
  } else {
    PaintFlags video_flags = context.FillFlags();
    video_flags.setColor(SK_ColorBLACK);
    layout_video_.VideoElement()->PaintCurrentFrame(
        context.Canvas(), snapped_replaced_rect, &video_flags);
  }
}

}  // namespace blink

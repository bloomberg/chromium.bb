// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/NinePieceImagePainter.h"

#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/NinePieceImageGrid.h"
#include "core/style/ComputedStyle.h"
#include "core/style/NinePieceImage.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

NinePieceImagePainter::NinePieceImagePainter(
    const LayoutBoxModelObject& layout_object)
    : layout_object_(layout_object) {}

bool NinePieceImagePainter::Paint(GraphicsContext& graphics_context,
                                  const LayoutRect& rect,
                                  const ComputedStyle& style,
                                  const NinePieceImage& nine_piece_image,
                                  SkBlendMode op) const {
  StyleImage* style_image = nine_piece_image.GetImage();
  if (!style_image)
    return false;

  if (!style_image->IsLoaded())
    return true;  // Never paint a nine-piece image incrementally, but don't
                  // paint the fallback borders either.

  if (!style_image->CanRender())
    return false;

  // FIXME: border-image is broken with full page zooming when tiling has to
  // happen, since the tiling function doesn't have any understanding of the
  // zoom that is in effect on the tile.
  LayoutRect rect_with_outsets = rect;
  rect_with_outsets.Expand(style.ImageOutsets(nine_piece_image));
  LayoutRect border_image_rect = rect_with_outsets;

  // NinePieceImage returns the image slices without effective zoom applied and
  // thus we compute the nine piece grid on top of the image in unzoomed
  // coordinates.
  //
  // FIXME: The default object size passed to imageSize() should be scaled by
  // the zoom factor passed in. In this case it means that borderImageRect
  // should be passed in compensated by effective zoom, since the scale factor
  // is one. For generated images, the actual image data (gradient stops, etc.)
  // are scaled to effective zoom instead so we must take care not to cause
  // scale of them again.
  IntSize image_size = RoundedIntSize(
      style_image->ImageSize(layout_object_, 1, border_image_rect.Size()));

  IntRectOutsets border_widths(style.BorderTopWidth(), style.BorderRightWidth(),
                               style.BorderBottomWidth(),
                               style.BorderLeftWidth());
  NinePieceImageGrid grid(nine_piece_image, image_size,
                          PixelSnappedIntRect(border_image_rect),
                          border_widths);

  RefPtr<Image> image =
      style_image->GetImage(layout_object_, image_size, style.EffectiveZoom());

  InterpolationQuality interpolation_quality =
      BoxPainter::ChooseInterpolationQuality(layout_object_, image.Get(), 0,
                                             rect_with_outsets.Size());
  InterpolationQuality previous_interpolation_quality =
      graphics_context.ImageInterpolationQuality();
  graphics_context.SetImageInterpolationQuality(interpolation_quality);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data",
               InspectorPaintImageEvent::Data(layout_object_, *style_image));

  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece, style_image->ImageScaleFactor());

    if (draw_info.is_drawable) {
      if (draw_info.is_corner_piece) {
        graphics_context.DrawImage(image.Get(), draw_info.destination,
                                   &draw_info.source, op);
      } else {
        graphics_context.DrawTiledImage(image.Get(), draw_info.destination,
                                        draw_info.source, draw_info.tile_scale,
                                        draw_info.tile_rule.horizontal,
                                        draw_info.tile_rule.vertical, op);
      }
    }
  }

  graphics_context.SetImageInterpolationQuality(previous_interpolation_quality);
  return true;
}

}  // namespace blink

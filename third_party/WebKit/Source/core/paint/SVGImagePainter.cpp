// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGImagePainter.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/svg/LayoutSVGImage.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/svg/SVGImageElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintRecord.h"

namespace blink {

void SVGImagePainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_svg_image_.Style()->Visibility() != EVisibility::kVisible ||
      !layout_svg_image_.ImageResource()->HasImage())
    return;

  FloatRect bounding_box = layout_svg_image_.VisualRectInLocalSVGCoordinates();
  if (!paint_info.GetCullRect().IntersectsCullRect(
          layout_svg_image_.LocalToSVGParentTransform(), bounding_box))
    return;

  PaintInfo paint_info_before_filtering(paint_info);
  // Images cannot have children so do not call updateCullRect.
  SVGTransformContext transform_context(
      paint_info_before_filtering.context, layout_svg_image_,
      layout_svg_image_.LocalToSVGParentTransform());
  {
    SVGPaintContext paint_context(layout_svg_image_,
                                  paint_info_before_filtering);
    if (paint_context.ApplyClipMaskAndFilterIfNecessary() &&
        !DrawingRecorder::UseCachedDrawingIfPossible(
            paint_context.GetPaintInfo().context, layout_svg_image_,
            paint_context.GetPaintInfo().phase)) {
      DrawingRecorder recorder(
          paint_context.GetPaintInfo().context, layout_svg_image_,
          paint_context.GetPaintInfo().phase, bounding_box);
      PaintForeground(paint_context.GetPaintInfo());
    }
  }

  if (layout_svg_image_.Style()->OutlineWidth()) {
    PaintInfo outline_paint_info(paint_info_before_filtering);
    outline_paint_info.phase = PaintPhase::kSelfOutlineOnly;
    ObjectPainter(layout_svg_image_)
        .PaintOutline(outline_paint_info, LayoutPoint(bounding_box.Location()));
  }
}

void SVGImagePainter::PaintForeground(const PaintInfo& paint_info) {
  const LayoutImageResource* image_resource = layout_svg_image_.ImageResource();
  IntSize image_viewport_size = ExpandedIntSize(ComputeImageViewportSize());
  if (image_viewport_size.IsEmpty())
    return;

  RefPtr<Image> image = image_resource->GetImage(image_viewport_size);
  FloatRect dest_rect = layout_svg_image_.ObjectBoundingBox();
  FloatRect src_rect(0, 0, image->width(), image->height());

  SVGImageElement* image_element =
      ToSVGImageElement(layout_svg_image_.GetElement());
  image_element->preserveAspectRatio()->CurrentValue()->TransformRect(dest_rect,
                                                                      src_rect);
  InterpolationQuality interpolation_quality =
      layout_svg_image_.StyleRef().GetInterpolationQuality();
  InterpolationQuality previous_interpolation_quality =
      paint_info.context.ImageInterpolationQuality();
  paint_info.context.SetImageInterpolationQuality(interpolation_quality);
  Image::ImageDecodingMode decode_mode =
      image_element->GetDecodingModeForPainting(image->paint_image_id());
  paint_info.context.DrawImage(image.get(), decode_mode, dest_rect, &src_rect);
  paint_info.context.SetImageInterpolationQuality(
      previous_interpolation_quality);
}

FloatSize SVGImagePainter::ComputeImageViewportSize() const {
  DCHECK(layout_svg_image_.ImageResource()->HasImage());

  if (ToSVGImageElement(layout_svg_image_.GetElement())
          ->preserveAspectRatio()
          ->CurrentValue()
          ->Align() != SVGPreserveAspectRatio::kSvgPreserveaspectratioNone)
    return layout_svg_image_.ObjectBoundingBox().Size();

  ImageResourceContent* cached_image =
      layout_svg_image_.ImageResource()->CachedImage();

  // Images with preserveAspectRatio=none should force non-uniform scaling. This
  // can be achieved by setting the image's container size to its viewport size
  // (i.e. concrete object size returned by the default sizing algorithm.)  See
  // https://www.w3.org/TR/SVG/single-page.html#coords-PreserveAspectRatioAttribute
  // and https://drafts.csswg.org/css-images-3/#default-sizing.

  // Avoid returning the size of the broken image.
  if (cached_image->ErrorOccurred())
    return FloatSize();

  if (cached_image->GetImage()->IsSVGImage()) {
    return ToSVGImage(cached_image->GetImage())
        ->ConcreteObjectSize(layout_svg_image_.ObjectBoundingBox().Size());
  }

  return FloatSize(cached_image->GetImage()->Size());
}

}  // namespace blink

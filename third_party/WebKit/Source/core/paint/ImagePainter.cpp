// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ImagePainter.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutReplaced.h"
#include "core/layout/TextRunConstructor.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/Path.h"

namespace blink {

void ImagePainter::Paint(const PaintInfo& paint_info,
                         const LayoutPoint& paint_offset) {
  layout_image_.LayoutReplaced::Paint(paint_info, paint_offset);

  if (paint_info.phase == kPaintPhaseOutline)
    PaintAreaElementFocusRing(paint_info, paint_offset);
}

void ImagePainter::PaintAreaElementFocusRing(const PaintInfo& paint_info,
                                             const LayoutPoint& paint_offset) {
  Document& document = layout_image_.GetDocument();

  if (paint_info.IsPrinting() ||
      !document.GetFrame()->Selection().FrameIsFocusedAndActive())
    return;

  Element* focused_element = document.FocusedElement();
  if (!IsHTMLAreaElement(focused_element))
    return;

  HTMLAreaElement& area_element = ToHTMLAreaElement(*focused_element);
  if (area_element.ImageElement() != layout_image_.GetNode())
    return;

  // Even if the theme handles focus ring drawing for entire elements, it won't
  // do it for an area within an image, so we don't call
  // LayoutTheme::themeDrawsFocusRing here.

  const ComputedStyle& area_element_style = *area_element.EnsureComputedStyle();
  // If the outline width is 0 we want to avoid drawing anything even if we
  // don't use the value directly.
  if (!area_element_style.OutlineWidth())
    return;

  Path path = area_element.GetPath(&layout_image_);
  if (path.IsEmpty())
    return;

  LayoutPoint adjusted_paint_offset = paint_offset;
  adjusted_paint_offset.MoveBy(layout_image_.Location());
  path.Translate(
      FloatSize(adjusted_paint_offset.X(), adjusted_paint_offset.Y()));

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_image_, DisplayItem::kImageAreaFocusRing))
    return;

  LayoutRect focus_rect = layout_image_.ContentBoxRect();
  focus_rect.MoveBy(adjusted_paint_offset);
  LayoutObjectDrawingRecorder drawing_recorder(
      paint_info.context, layout_image_, DisplayItem::kImageAreaFocusRing,
      focus_rect);

  // FIXME: Clip path instead of context when Skia pathops is ready.
  // https://crbug.com/251206

  paint_info.context.Save();
  paint_info.context.Clip(PixelSnappedIntRect(focus_rect));
  paint_info.context.DrawFocusRing(
      path, area_element_style.GetOutlineStrokeWidthForFocusRing(),
      area_element_style.OutlineOffset(),
      layout_image_.ResolveColor(area_element_style, CSSPropertyOutlineColor));
  paint_info.context.Restore();
}

void ImagePainter::PaintReplaced(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  LayoutUnit c_width = layout_image_.ContentWidth();
  LayoutUnit c_height = layout_image_.ContentHeight();

  GraphicsContext& context = paint_info.context;

  if (!layout_image_.ImageResource()->HasImage()) {
    if (paint_info.phase == kPaintPhaseSelection)
      return;
    if (c_width > 2 && c_height > 2) {
      if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
              context, layout_image_, paint_info.phase))
        return;
      // Draw an outline rect where the image should be.
      IntRect paint_rect = PixelSnappedIntRect(
          LayoutRect(paint_offset.X() + layout_image_.BorderLeft() +
                         layout_image_.PaddingLeft(),
                     paint_offset.Y() + layout_image_.BorderTop() +
                         layout_image_.PaddingTop(),
                     c_width, c_height));
      LayoutObjectDrawingRecorder drawing_recorder(
          context, layout_image_, paint_info.phase, paint_rect);
      context.SetStrokeStyle(kSolidStroke);
      context.SetStrokeColor(Color::kLightGray);
      context.SetFillColor(Color::kTransparent);
      context.DrawRect(paint_rect);
    }
  } else if (c_width > 0 && c_height > 0) {
    if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
            context, layout_image_, paint_info.phase))
      return;

    LayoutRect content_rect = layout_image_.ContentBoxRect();
    content_rect.MoveBy(paint_offset);
    LayoutRect paint_rect = layout_image_.ReplacedContentRect();
    paint_rect.MoveBy(paint_offset);

    LayoutObjectDrawingRecorder drawing_recorder(
        context, layout_image_, paint_info.phase, content_rect);
    PaintIntoRect(context, paint_rect, content_rect);
  }
}

void ImagePainter::PaintIntoRect(GraphicsContext& context,
                                 const LayoutRect& dest_rect,
                                 const LayoutRect& content_rect) {
  if (!layout_image_.ImageResource()->HasImage() ||
      layout_image_.ImageResource()->ErrorOccurred())
    return;  // FIXME: should we just ASSERT these conditions? (audit all
             // callers).

  IntRect pixel_snapped_dest_rect = PixelSnappedIntRect(dest_rect);
  if (pixel_snapped_dest_rect.IsEmpty())
    return;

  RefPtr<Image> image =
      layout_image_.ImageResource()->GetImage(pixel_snapped_dest_rect.Size());
  if (!image || image->IsNull())
    return;

  InterpolationQuality interpolation_quality =
      layout_image_.StyleRef().GetInterpolationQuality();

  FloatRect src_rect = image->Rect();
  // If the content rect requires clipping, adjust |srcRect| and
  // |pixelSnappedDestRect| over using a clip.
  if (!content_rect.Contains(dest_rect)) {
    IntRect pixel_snapped_content_rect = PixelSnappedIntRect(content_rect);
    pixel_snapped_content_rect.Intersect(pixel_snapped_dest_rect);
    if (pixel_snapped_content_rect.IsEmpty())
      return;
    src_rect =
        MapRect(pixel_snapped_content_rect, pixel_snapped_dest_rect, src_rect);
    pixel_snapped_dest_rect = pixel_snapped_content_rect;
  }

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data",
               InspectorPaintImageEvent::Data(layout_image_, src_rect,
                                              FloatRect(dest_rect)));

  InterpolationQuality previous_interpolation_quality =
      context.ImageInterpolationQuality();
  context.SetImageInterpolationQuality(interpolation_quality);

  Node* node = layout_image_.GetNode();
  Image::ImageDecodingMode decode_mode =
      IsHTMLImageElement(node) ? ToHTMLImageElement(node)->GetDecodingMode()
                               : Image::kUnspecifiedDecode;
  context.DrawImage(
      image.get(), decode_mode, pixel_snapped_dest_rect, &src_rect,
      SkBlendMode::kSrcOver,
      LayoutObject::ShouldRespectImageOrientation(&layout_image_));
  context.SetImageInterpolationQuality(previous_interpolation_quality);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_painter.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"

namespace blink {

void ImagePainter::Paint(const PaintInfo& paint_info,
                         const LayoutPoint& paint_offset) {
  layout_image_.LayoutReplaced::Paint(paint_info, paint_offset);

  if (paint_info.phase == PaintPhase::kOutline)
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

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_image_, DisplayItem::kImageAreaFocusRing))
    return;

  DrawingRecorder recorder(paint_info.context, layout_image_,
                           DisplayItem::kImageAreaFocusRing);

  // FIXME: Clip path instead of context when Skia pathops is ready.
  // https://crbug.com/251206

  paint_info.context.Save();
  LayoutRect focus_rect = layout_image_.ContentBoxRect();
  focus_rect.MoveBy(adjusted_paint_offset);
  paint_info.context.Clip(PixelSnappedIntRect(focus_rect));
  paint_info.context.DrawFocusRing(
      path, area_element_style.GetOutlineStrokeWidthForFocusRing(),
      area_element_style.OutlineOffset(),
      layout_image_.ResolveColor(area_element_style,
                                 GetCSSPropertyOutlineColor()));
  paint_info.context.Restore();
}

void ImagePainter::PaintReplaced(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  LayoutSize content_size = layout_image_.ContentSize();
  bool has_image = layout_image_.ImageResource()->HasImage();

  if (has_image) {
    if (content_size.IsEmpty())
      return;
  } else {
    if (paint_info.phase == PaintPhase::kSelection)
      return;
    if (content_size.Width() <= 2 || content_size.Height() <= 2)
      return;
  }

  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, layout_image_,
                                                  paint_info.phase))
    return;

  // Disable cache in under-invalidation checking mode for animated image
  // because it may change before it's actually invalidated.
  Optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      layout_image_.ImageResource() &&
      layout_image_.ImageResource()->MaybeAnimated())
    cache_skipper.emplace(context);

  LayoutRect content_rect(paint_offset + layout_image_.ContentBoxOffset(),
                          content_size);

  if (!has_image) {
    // Draw an outline rect where the image should be.
    IntRect paint_rect = PixelSnappedIntRect(content_rect);
    DrawingRecorder recorder(context, layout_image_, paint_info.phase);
    context.SetStrokeStyle(kSolidStroke);
    context.SetStrokeColor(Color::kLightGray);
    context.SetFillColor(Color::kTransparent);
    context.DrawRect(paint_rect);
    return;
  }

  LayoutRect paint_rect = layout_image_.ReplacedContentRect();
  paint_rect.MoveBy(paint_offset);

  DrawingRecorder recorder(context, layout_image_, paint_info.phase);
  PaintIntoRect(context, paint_rect, content_rect);
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

  scoped_refptr<Image> image = layout_image_.ImageResource()->GetImage(
      LayoutSize(pixel_snapped_dest_rect.Size()));
  if (!image || image->IsNull())
    return;

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

  ScopedInterpolationQuality interpolation_quality_scope(
      context, layout_image_.StyleRef().GetInterpolationQuality());

  Node* node = layout_image_.GetNode();
  Image::ImageDecodingMode decode_mode =
      IsHTMLImageElement(node)
          ? ToHTMLImageElement(node)->GetDecodingModeForPainting(
                image->paint_image_id())
          : Image::kUnspecifiedDecode;
  context.DrawImage(
      image.get(), decode_mode, pixel_snapped_dest_rect, &src_rect,
      SkBlendMode::kSrcOver,
      LayoutObject::ShouldRespectImageOrientation(&layout_image_));
}

}  // namespace blink

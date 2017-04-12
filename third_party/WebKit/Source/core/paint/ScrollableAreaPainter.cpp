// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ScrollableAreaPainter.h"

#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/HostWindow.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"

namespace blink {

void ScrollableAreaPainter::PaintResizer(GraphicsContext& context,
                                         const IntPoint& paint_offset,
                                         const CullRect& cull_rect) {
  if (GetScrollableArea().Box().Style()->Resize() == RESIZE_NONE)
    return;

  IntRect abs_rect = GetScrollableArea().ResizerCornerRect(
      GetScrollableArea().Box().PixelSnappedBorderBoxRect(),
      kResizerForPointer);
  if (abs_rect.IsEmpty())
    return;
  abs_rect.MoveBy(paint_offset);

  if (GetScrollableArea().Resizer()) {
    if (!cull_rect.IntersectsCullRect(abs_rect))
      return;
    ScrollbarPainter::PaintIntoRect(*GetScrollableArea().Resizer(), context,
                                    paint_offset, LayoutRect(abs_rect));
    return;
  }

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          context, GetScrollableArea().Box(), DisplayItem::kResizer))
    return;

  LayoutObjectDrawingRecorder recorder(context, GetScrollableArea().Box(),
                                       DisplayItem::kResizer, abs_rect);

  DrawPlatformResizerImage(context, abs_rect);

  // Draw a frame around the resizer (1px grey line) if there are any scrollbars
  // present.  Clipping will exclude the right and bottom edges of this frame.
  if (!GetScrollableArea().HasOverlayScrollbars() &&
      GetScrollableArea().HasScrollbar()) {
    GraphicsContextStateSaver state_saver(context);
    context.Clip(abs_rect);
    IntRect larger_corner = abs_rect;
    larger_corner.SetSize(
        IntSize(larger_corner.Width() + 1, larger_corner.Height() + 1));
    context.SetStrokeColor(Color(217, 217, 217));
    context.SetStrokeThickness(1.0f);
    context.SetFillColor(Color::kTransparent);
    context.DrawRect(larger_corner);
  }
}

void ScrollableAreaPainter::DrawPlatformResizerImage(
    GraphicsContext& context,
    IntRect resizer_corner_rect) {
  float old_device_scale_factor =
      blink::DeviceScaleFactorDeprecated(GetScrollableArea().Box().GetFrame());
  // |blink::deviceScaleFactor| returns different values between MAC (2 or 1)
  // and other platforms (always 1). For this reason we cannot hardcode the
  // value of 1 in the call for |windowToViewportScalar|. Since zoom-for-dsf is
  // disabled on MAC, |windowToViewportScalar| will be a no-op on it.
  float device_scale_factor =
      GetScrollableArea().GetHostWindow()->WindowToViewportScalar(
          old_device_scale_factor);

  RefPtr<Image> resize_corner_image;
  IntSize corner_resizer_size;
  if (device_scale_factor >= 2) {
    DEFINE_STATIC_REF(Image, resize_corner_image_hi_res,
                      (Image::LoadPlatformResource("textAreaResizeCorner@2x")));
    resize_corner_image = resize_corner_image_hi_res;
    corner_resizer_size = resize_corner_image->size();
    if (old_device_scale_factor >= 2)
      corner_resizer_size.Scale(0.5f);
  } else {
    DEFINE_STATIC_REF(Image, resize_corner_image_lo_res,
                      (Image::LoadPlatformResource("textAreaResizeCorner")));
    resize_corner_image = resize_corner_image_lo_res;
    corner_resizer_size = resize_corner_image->size();
  }

  if (GetScrollableArea()
          .Box()
          .ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    context.Save();
    context.Translate(resizer_corner_rect.X() + corner_resizer_size.Width(),
                      resizer_corner_rect.Y() + resizer_corner_rect.Height() -
                          corner_resizer_size.Height());
    context.Scale(-1.0, 1.0);
    context.DrawImage(resize_corner_image.Get(),
                      IntRect(IntPoint(), corner_resizer_size));
    context.Restore();
    return;
  }
  IntRect image_rect(resizer_corner_rect.MaxXMaxYCorner() - corner_resizer_size,
                     corner_resizer_size);
  context.DrawImage(resize_corner_image.Get(), image_rect);
}

void ScrollableAreaPainter::PaintOverflowControls(
    GraphicsContext& context,
    const IntPoint& paint_offset,
    const CullRect& cull_rect,
    bool painting_overlay_controls) {
  // Don't do anything if we have no overflow.
  if (!GetScrollableArea().Box().HasOverflowClip())
    return;

  IntPoint adjusted_paint_offset = paint_offset;
  if (painting_overlay_controls)
    adjusted_paint_offset = GetScrollableArea().CachedOverlayScrollbarOffset();

  CullRect adjusted_cull_rect(cull_rect, -adjusted_paint_offset);
  // Overlay scrollbars paint in a second pass through the layer tree so that
  // they will paint on top of everything else. If this is the normal painting
  // pass, paintingOverlayControls will be false, and we should just tell the
  // root layer that there are overlay scrollbars that need to be painted. That
  // will cause the second pass through the layer tree to run, and we'll paint
  // the scrollbars then. In the meantime, cache tx and ty so that the second
  // pass doesn't need to re-enter the LayoutTree to get it right.
  if (GetScrollableArea().HasOverlayScrollbars() &&
      !painting_overlay_controls) {
    GetScrollableArea().SetCachedOverlayScrollbarOffset(paint_offset);
    // It's not necessary to do the second pass if the scrollbars paint into
    // layers.
    if ((GetScrollableArea().HorizontalScrollbar() &&
         GetScrollableArea().LayerForHorizontalScrollbar()) ||
        (GetScrollableArea().VerticalScrollbar() &&
         GetScrollableArea().LayerForVerticalScrollbar()))
      return;
    if (!OverflowControlsIntersectRect(adjusted_cull_rect))
      return;

    LayoutView* layout_view = GetScrollableArea().Box().View();

    PaintLayer* painting_root =
        GetScrollableArea().Layer()->EnclosingLayerWithCompositedLayerMapping(
            kIncludeSelf);
    if (!painting_root)
      painting_root = layout_view->Layer();

    painting_root->SetContainsDirtyOverlayScrollbars(true);
    return;
  }

  // This check is required to avoid painting custom CSS scrollbars twice.
  if (painting_overlay_controls && !GetScrollableArea().HasOverlayScrollbars())
    return;

  IntRect clip_rect(adjusted_paint_offset,
                    GetScrollableArea().VisibleContentRect().Size());
  ClipRecorder clip_recorder(context, GetScrollableArea().Box(),
                             DisplayItem::kClipLayerOverflowControls,
                             clip_rect);

  {
    Optional<ScopedPaintChunkProperties> scoped_transform_property;
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      const auto* object_properties =
          GetScrollableArea().Box().PaintProperties();
      if (object_properties && object_properties->ScrollbarPaintOffset()) {
        PaintChunkProperties properties(
            context.GetPaintController().CurrentPaintChunkProperties());
        properties.property_tree_state.SetTransform(
            object_properties->ScrollbarPaintOffset());
        scoped_transform_property.emplace(
            context.GetPaintController(), GetScrollableArea().Box(),
            DisplayItem::kScrollOverflowControls, properties);
      }
    }
    if (GetScrollableArea().HorizontalScrollbar() &&
        !GetScrollableArea().LayerForHorizontalScrollbar()) {
      TransformRecorder translate_recorder(
          context, *GetScrollableArea().HorizontalScrollbar(),
          AffineTransform::Translation(adjusted_paint_offset.X(),
                                       adjusted_paint_offset.Y()));
      GetScrollableArea().HorizontalScrollbar()->Paint(context,
                                                       adjusted_cull_rect);
    }
    if (GetScrollableArea().VerticalScrollbar() &&
        !GetScrollableArea().LayerForVerticalScrollbar()) {
      TransformRecorder translate_recorder(
          context, *GetScrollableArea().VerticalScrollbar(),
          AffineTransform::Translation(adjusted_paint_offset.X(),
                                       adjusted_paint_offset.Y()));
      GetScrollableArea().VerticalScrollbar()->Paint(context,
                                                     adjusted_cull_rect);
    }
  }

  if (GetScrollableArea().LayerForScrollCorner())
    return;

  // We fill our scroll corner with white if we have a scrollbar that doesn't
  // run all the way up to the edge of the box.
  PaintScrollCorner(context, adjusted_paint_offset, cull_rect);

  // Paint our resizer last, since it sits on top of the scroll corner.
  PaintResizer(context, adjusted_paint_offset, cull_rect);
}

bool ScrollableAreaPainter::OverflowControlsIntersectRect(
    const CullRect& cull_rect) const {
  const IntRect border_box =
      GetScrollableArea().Box().PixelSnappedBorderBoxRect();

  if (cull_rect.IntersectsCullRect(
          GetScrollableArea().RectForHorizontalScrollbar(border_box)))
    return true;

  if (cull_rect.IntersectsCullRect(
          GetScrollableArea().RectForVerticalScrollbar(border_box)))
    return true;

  if (cull_rect.IntersectsCullRect(GetScrollableArea().ScrollCornerRect()))
    return true;

  if (cull_rect.IntersectsCullRect(GetScrollableArea().ResizerCornerRect(
          border_box, kResizerForPointer)))
    return true;

  return false;
}

void ScrollableAreaPainter::PaintScrollCorner(
    GraphicsContext& context,
    const IntPoint& paint_offset,
    const CullRect& adjusted_cull_rect) {
  IntRect abs_rect = GetScrollableArea().ScrollCornerRect();
  if (abs_rect.IsEmpty())
    return;
  abs_rect.MoveBy(paint_offset);

  if (GetScrollableArea().ScrollCorner()) {
    if (!adjusted_cull_rect.IntersectsCullRect(abs_rect))
      return;
    ScrollbarPainter::PaintIntoRect(*GetScrollableArea().ScrollCorner(),
                                    context, paint_offset,
                                    LayoutRect(abs_rect));
    return;
  }

  // We don't want to paint white if we have overlay scrollbars, since we need
  // to see what is behind it.
  if (GetScrollableArea().HasOverlayScrollbars())
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          context, GetScrollableArea().Box(), DisplayItem::kScrollbarCorner))
    return;

  LayoutObjectDrawingRecorder recorder(context, GetScrollableArea().Box(),
                                       DisplayItem::kScrollbarCorner, abs_rect);
  context.FillRect(abs_rect, Color::kWhite);
}

PaintLayerScrollableArea& ScrollableAreaPainter::GetScrollableArea() const {
  return *scrollable_area_;
}

}  // namespace blink

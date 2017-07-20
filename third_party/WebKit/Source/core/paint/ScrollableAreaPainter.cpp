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
#include "platform/PlatformChromeClient.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"

namespace blink {

void ScrollableAreaPainter::PaintResizer(GraphicsContext& context,
                                         const IntPoint& paint_offset,
                                         const CullRect& cull_rect) {
  if (GetScrollableArea().Box().Style()->Resize() == EResize::kNone)
    return;

  IntRect abs_rect = GetScrollableArea().ResizerCornerRect(
      GetScrollableArea().Box().PixelSnappedBorderBoxRect(
          GetScrollableArea().Layer()->SubpixelAccumulation()),
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
  IntPoint points[4];
  bool on_left = false;
  if (GetScrollableArea()
          .Box()
          .ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    on_left = true;
    points[0].SetX(resizer_corner_rect.X() + 1);
    points[1].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() -
                   resizer_corner_rect.Width() / 2);
    points[2].SetX(points[0].X());
    points[3].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() -
                   resizer_corner_rect.Width() * 3 / 4);
  } else {
    points[0].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() - 1);
    points[1].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() / 2);
    points[2].SetX(points[0].X());
    points[3].SetX(resizer_corner_rect.X() +
                   resizer_corner_rect.Width() * 3 / 4);
  }
  points[0].SetY(resizer_corner_rect.Y() + resizer_corner_rect.Height() / 2);
  points[1].SetY(resizer_corner_rect.Y() + resizer_corner_rect.Height() - 1);
  points[2].SetY(resizer_corner_rect.Y() +
                 resizer_corner_rect.Height() * 3 / 4);
  points[3].SetY(points[1].Y());

  PaintFlags paint_flags;
  paint_flags.setStyle(PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(1);

  SkPath line_path;

  // Draw a dark line, to ensure contrast against a light background
  line_path.moveTo(points[0].X(), points[0].Y());
  line_path.lineTo(points[1].X(), points[1].Y());
  line_path.moveTo(points[2].X(), points[2].Y());
  line_path.lineTo(points[3].X(), points[3].Y());
  paint_flags.setARGB(153, 0, 0, 0);
  context.DrawPath(line_path, paint_flags);

  // Draw a light line one pixel below the light line,
  // to ensure contrast against a dark background
  line_path.rewind();
  line_path.moveTo(points[0].X(), points[0].Y() + 1);
  line_path.lineTo(points[1].X() + (on_left ? -1 : 1), points[1].Y());
  line_path.moveTo(points[2].X(), points[2].Y() + 1);
  line_path.lineTo(points[3].X() + (on_left ? -1 : 1), points[3].Y());
  paint_flags.setARGB(153, 255, 255, 255);
  context.DrawPath(line_path, paint_flags);
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

  IntRect clip_rect(
      adjusted_paint_offset,
      GetScrollableArea().VisibleContentRect(kIncludeScrollbars).Size());
  ClipRecorder clip_recorder(context, GetScrollableArea().Box(),
                             DisplayItem::kClipLayerOverflowControls,
                             clip_rect);

  {
    Optional<ScopedPaintChunkProperties> scoped_transform_property;
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
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
      GetScrollableArea().Box().PixelSnappedBorderBoxRect(
          GetScrollableArea().Layer()->SubpixelAccumulation());

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

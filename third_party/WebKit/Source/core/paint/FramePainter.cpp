// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FramePainter.h"

#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/LocalFrameView.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/FramePaintTiming.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/paint/TransformRecorder.h"
#include "core/probe/CoreProbes.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

bool FramePainter::in_paint_contents_ = false;

void FramePainter::Paint(GraphicsContext& context,
                         const GlobalPaintFlags global_paint_flags,
                         const CullRect& rect) {
  GetFrameView().NotifyPageThatContentAreaWillPaint();

  IntRect document_dirty_rect;
  IntRect visible_area_without_scrollbars(
      GetFrameView().Location(), GetFrameView().VisibleContentRect().Size());
  IntPoint content_offset =
      -GetFrameView().Location() + GetFrameView().ScrollOffsetInt();
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled() &&
      !RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    auto content_cull_rect = rect;
    content_cull_rect.UpdateForScrollingContents(
        visible_area_without_scrollbars,
        AffineTransform().Translate(-content_offset.X(), -content_offset.Y()));
    document_dirty_rect = content_cull_rect.rect_;
  } else {
    document_dirty_rect = rect.rect_;
    document_dirty_rect.Intersect(visible_area_without_scrollbars);
    document_dirty_rect.MoveBy(content_offset);
  }

  bool should_paint_contents = !document_dirty_rect.IsEmpty();
  bool should_paint_scrollbars = !GetFrameView().ScrollbarsSuppressed() &&
                                 (GetFrameView().HorizontalScrollbar() ||
                                  GetFrameView().VerticalScrollbar());
  if (!should_paint_contents && !should_paint_scrollbars)
    return;

  if (should_paint_contents) {
    // TODO(pdr): Creating frame paint properties here will not be needed once
    // settings()->rootLayerScrolls() is enabled.
    // TODO(pdr): Make this conditional on the rootLayerScrolls setting.
    Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled() &&
        !RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      if (const PropertyTreeState* contents_state =
              frame_view_->TotalPropertyTreeStateForContents()) {
        PaintChunkProperties properties(
            context.GetPaintController().CurrentPaintChunkProperties());
        properties.property_tree_state = *contents_state;
        scoped_paint_chunk_properties.emplace(context.GetPaintController(),
                                              *GetFrameView().GetLayoutView(),
                                              properties);
      }
    }

    TransformRecorder transform_recorder(
        context, *GetFrameView().GetLayoutView(),
        AffineTransform::Translation(
            GetFrameView().X() - GetFrameView().ScrollX(),
            GetFrameView().Y() - GetFrameView().ScrollY()));

    ClipRecorder clip_recorder(context, *GetFrameView().GetLayoutView(),
                               DisplayItem::kClipFrameToVisibleContentRect,
                               GetFrameView().VisibleContentRect());
    PaintContents(context, global_paint_flags, document_dirty_rect);
  }

  if (should_paint_scrollbars) {
    DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
    IntRect scroll_view_dirty_rect = rect.rect_;
    IntRect visible_area_with_scrollbars(
        GetFrameView().Location(),
        GetFrameView().VisibleContentRect(kIncludeScrollbars).Size());
    scroll_view_dirty_rect.Intersect(visible_area_with_scrollbars);
    scroll_view_dirty_rect.MoveBy(-GetFrameView().Location());

    Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
      if (const PropertyTreeState* contents_state =
              frame_view_->TotalPropertyTreeStateForContents()) {
        // The scrollbar's property nodes are similar to the frame view's
        // contents state but we want to exclude the content-specific
        // properties. This prevents the scrollbars from scrolling, for example.
        PaintChunkProperties properties(
            context.GetPaintController().CurrentPaintChunkProperties());
        properties.property_tree_state.SetTransform(
            frame_view_->PreTranslation());
        properties.property_tree_state.SetClip(
            frame_view_->ContentClip()->Parent());
        properties.property_tree_state.SetEffect(contents_state->Effect());
        scoped_paint_chunk_properties.emplace(context.GetPaintController(),
                                              *GetFrameView().GetLayoutView(),
                                              properties);
      }
    }

    TransformRecorder transform_recorder(
        context, *GetFrameView().GetLayoutView(),
        AffineTransform::Translation(GetFrameView().X(), GetFrameView().Y()));

    ClipRecorder recorder(
        context, *GetFrameView().GetLayoutView(),
        DisplayItem::kClipFrameScrollbars,
        IntRect(IntPoint(), visible_area_with_scrollbars.Size()));

    PaintScrollbars(context, scroll_view_dirty_rect);
  }
}

void FramePainter::PaintContents(GraphicsContext& context,
                                 const GlobalPaintFlags global_paint_flags,
                                 const IntRect& rect) {
  Document* document = GetFrameView().GetFrame().GetDocument();

  if (GetFrameView().ShouldThrottleRendering() || !document->IsActive())
    return;

  LayoutView* layout_view = GetFrameView().GetLayoutView();
  if (!layout_view) {
    DLOG(ERROR) << "called FramePainter::paint with nil layoutObject";
    return;
  }

  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!GetFrameView().CheckDoesNotNeedLayout())
    return;

  // TODO(wangxianzhu): The following check should be stricter, but currently
  // this is blocked by the svg root issue (crbug.com/442939).
  DCHECK(document->Lifecycle().GetState() >=
         DocumentLifecycle::kCompositingClean);

  FramePaintTiming frame_paint_timing(context, &GetFrameView().GetFrame());
  TRACE_EVENT1(
      "devtools.timeline,rail", "Paint", "data",
      InspectorPaintEvent::Data(layout_view, LayoutRect(rect), nullptr));

  bool is_top_level_painter = !in_paint_contents_;
  in_paint_contents_ = true;

  FontCachePurgePreventer font_cache_purge_preventer;

  // TODO(jchaffraix): GlobalPaintFlags should be const during a paint
  // phase. Thus we should set this flag upfront (crbug.com/510280).
  GlobalPaintFlags updated_global_paint_flags = global_paint_flags;
  PaintLayerFlags root_layer_paint_flags = 0;
  if (document->Printing()) {
    updated_global_paint_flags |=
        kGlobalPaintFlattenCompositingLayers | kGlobalPaintPrinting;
    // This will prevent clipping the root PaintLayer to its visible content
    // rect when root layer scrolling is enabled.
    root_layer_paint_flags = kPaintLayerPaintingOverflowContents;
  }

  PaintLayer* root_layer = layout_view->Layer();

#if DCHECK_IS_ON()
  layout_view->AssertSubtreeIsLaidOut();
  LayoutObject::SetLayoutNeededForbiddenScope forbid_set_needs_layout(
      root_layer->GetLayoutObject());
#endif

  PaintLayerPainter layer_painter(*root_layer);

  float device_scale_factor = blink::DeviceScaleFactorDeprecated(
      root_layer->GetLayoutObject().GetFrame());
  context.SetDeviceScaleFactor(device_scale_factor);

  layer_painter.Paint(context, LayoutRect(rect), updated_global_paint_flags,
                      root_layer_paint_flags);

  if (root_layer->ContainsDirtyOverlayScrollbars()) {
    layer_painter.PaintOverlayScrollbars(context, LayoutRect(rect),
                                         updated_global_paint_flags);
  }

  // Regions may have changed as a result of the visibility/z-index of element
  // changing.
  if (document->AnnotatedRegionsDirty())
    GetFrameView().UpdateDocumentAnnotatedRegions();

  if (is_top_level_painter) {
    // Everything that happens after paintContents completions is considered
    // to be part of the next frame.
    GetMemoryCache()->UpdateFramePaintTimestamp();
    in_paint_contents_ = false;
  }

  probe::didPaint(layout_view->GetFrame(), nullptr, context, LayoutRect(rect));
}

void FramePainter::PaintScrollbars(GraphicsContext& context,
                                   const IntRect& rect) {
  if (GetFrameView().HorizontalScrollbar() &&
      !GetFrameView().LayerForHorizontalScrollbar())
    PaintScrollbar(context, *GetFrameView().HorizontalScrollbar(), rect);
  if (GetFrameView().VerticalScrollbar() &&
      !GetFrameView().LayerForVerticalScrollbar())
    PaintScrollbar(context, *GetFrameView().VerticalScrollbar(), rect);

  if (GetFrameView().LayerForScrollCorner() ||
      !GetFrameView().IsScrollCornerVisible()) {
    return;
  }

  PaintScrollCorner(context, GetFrameView().ScrollCornerRect());
}

void FramePainter::PaintScrollCorner(GraphicsContext& context,
                                     const IntRect& corner_rect) {
  if (GetFrameView().ScrollCorner()) {
    bool needs_background = GetFrameView().GetFrame().IsMainFrame();
    if (needs_background && !DrawingRecorder::UseCachedDrawingIfPossible(
                                context, *GetFrameView().ScrollCorner(),
                                DisplayItem::kScrollbarBackground)) {
      DrawingRecorder recorder(context, *GetFrameView().ScrollCorner(),
                               DisplayItem::kScrollbarBackground, corner_rect);
      context.FillRect(corner_rect, GetFrameView().BaseBackgroundColor());
    }
    ScrollbarPainter::PaintIntoRect(*GetFrameView().ScrollCorner(), context,
                                    corner_rect.Location(),
                                    LayoutRect(corner_rect));
    return;
  }

  ScrollbarTheme* theme = nullptr;

  if (GetFrameView().HorizontalScrollbar()) {
    theme = &GetFrameView().HorizontalScrollbar()->GetTheme();
  } else if (GetFrameView().VerticalScrollbar()) {
    theme = &GetFrameView().VerticalScrollbar()->GetTheme();
  } else {
    NOTREACHED();
  }

  theme->PaintScrollCorner(context, *GetFrameView().GetLayoutView(),
                           corner_rect);
}

void FramePainter::PaintScrollbar(GraphicsContext& context,
                                  Scrollbar& bar,
                                  const IntRect& rect) {
  bool needs_background =
      bar.IsCustomScrollbar() && GetFrameView().GetFrame().IsMainFrame();
  if (needs_background) {
    IntRect to_fill = bar.FrameRect();
    to_fill.Intersect(rect);
    if (!to_fill.IsEmpty() &&
        !DrawingRecorder::UseCachedDrawingIfPossible(
            context, bar, DisplayItem::kScrollbarBackground)) {
      DrawingRecorder recorder(context, bar, DisplayItem::kScrollbarBackground,
                               to_fill);
      context.FillRect(to_fill, GetFrameView().BaseBackgroundColor());
    }
  }

  bar.Paint(context, CullRect(rect));
}

const LocalFrameView& FramePainter::GetFrameView() {
  DCHECK(frame_view_);
  return *frame_view_;
}

}  // namespace blink

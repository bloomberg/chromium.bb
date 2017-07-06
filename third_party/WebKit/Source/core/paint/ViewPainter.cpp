// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ViewPainter.h"

#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ScrollRecorder.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void ViewPainter::Paint(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset) {
  // If we ever require layout but receive a paint anyway, something has gone
  // horribly wrong.
  DCHECK(!layout_view_.NeedsLayout());
  // LayoutViews should never be called to paint with an offset not on device
  // pixels.
  DCHECK(LayoutPoint(IntPoint(paint_offset.X().ToInt(),
                              paint_offset.Y().ToInt())) == paint_offset);

  const LocalFrameView* frame_view = layout_view_.GetFrameView();
  if (frame_view->ShouldThrottleRendering())
    return;

  layout_view_.PaintObject(paint_info, paint_offset);
  BlockPainter(layout_view_)
      .PaintOverflowControlsIfNeeded(paint_info, paint_offset);
}

void ViewPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info) {
  if (paint_info.SkipRootBackground())
    return;

  // This function overrides background painting for the LayoutView.
  // View background painting is special in the following ways:
  // 1. The view paints background for the root element, the background
  //    positioning respects the positioning and transformation of the root
  //    element.
  // 2. CSS background-clip is ignored, the background layers always expand to
  //    cover the whole canvas. None of the stacking context effects (except
  //    transformation) on the root element affects the background.
  // 3. The main frame is also responsible for painting the user-agent-defined
  //    base background color. Conceptually it should be painted by the embedder
  //    but painting it here allows culling and pre-blending optimization when
  //    possible.

  GraphicsContext& context = paint_info.context;

  // The background rect always includes at least the visible content size.
  IntRect background_rect(IntRect(layout_view_.ViewRect()));

  if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    background_rect.Unite(layout_view_.DocumentRect());

  const DisplayItemClient* display_item_client = &layout_view_;

  Optional<ScrollRecorder> scroll_recorder;
  if (BoxPainter::
          IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
              &layout_view_, paint_info)) {
    // Layout overflow, combined with the visible content size.
    background_rect.Unite(layout_view_.DocumentRect());
    display_item_client =
        static_cast<const DisplayItemClient*>(layout_view_.Layer()
                                                  ->GetCompositedLayerMapping()
                                                  ->ScrollingContentsLayer());
    scroll_recorder.emplace(paint_info.context, layout_view_, paint_info.phase,
                            layout_view_.ScrolledContentOffset());
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, *display_item_client, DisplayItem::kDocumentBackground))
    return;

  const Document& document = layout_view_.GetDocument();
  const LocalFrameView& frame_view = *layout_view_.GetFrameView();
  bool is_main_frame = document.IsInMainFrame();
  bool paints_base_background =
      is_main_frame && (frame_view.BaseBackgroundColor().Alpha() > 0);
  bool should_clear_canvas =
      paints_base_background &&
      (document.GetSettings() &&
       document.GetSettings()->GetShouldClearDocumentBackground());
  Color base_background_color =
      paints_base_background ? frame_view.BaseBackgroundColor() : Color();
  Color root_background_color =
      layout_view_.Style()->VisitedDependentColor(CSSPropertyBackgroundColor);
  const LayoutObject* root_object =
      document.documentElement() ? document.documentElement()->GetLayoutObject()
                                 : nullptr;

  DrawingRecorder recorder(context, *display_item_client,
                           DisplayItem::kDocumentBackground, background_rect);

  // Special handling for print economy mode.
  bool force_background_to_white =
      BoxPainter::ShouldForceWhiteBackgroundForPrintEconomy(
          document, layout_view_.StyleRef());
  if (force_background_to_white) {
    // If for any reason the view background is not transparent, paint white
    // instead, otherwise keep transparent as is.
    if (paints_base_background || root_background_color.Alpha() ||
        layout_view_.Style()->BackgroundLayers().GetImage())
      context.FillRect(background_rect, Color::kWhite, SkBlendMode::kSrc);
    return;
  }

  // Compute the enclosing rect of the view, in root element space.
  //
  // For background colors we can simply paint the document rect in the default
  // space.  However for background image, the root element transform applies.
  // The strategy is to apply root element transform on the context and issue
  // draw commands in the local space, therefore we need to apply inverse
  // transform on the document rect to get to the root element space.
  bool background_renderable = true;
  TransformationMatrix transform;
  IntRect paint_rect = background_rect;
  if (!root_object || !root_object->IsBox()) {
    background_renderable = false;
  } else if (root_object->HasLayer()) {
    const PaintLayer& root_layer =
        *ToLayoutBoxModelObject(root_object)->Layer();
    LayoutPoint offset;
    root_layer.ConvertToLayerCoords(nullptr, offset);
    transform.Translate(offset.X(), offset.Y());
    transform.Multiply(
        root_layer.RenderableTransform(paint_info.GetGlobalPaintFlags()));

    if (!transform.IsInvertible()) {
      background_renderable = false;
    } else {
      bool is_clamped;
      paint_rect = transform.Inverse()
                       .ProjectQuad(FloatQuad(background_rect), &is_clamped)
                       .EnclosingBoundingBox();
      background_renderable = !is_clamped;
    }
  }

  if (!background_renderable) {
    if (base_background_color.Alpha()) {
      context.FillRect(
          background_rect, base_background_color,
          should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
    } else if (should_clear_canvas) {
      context.FillRect(background_rect, Color(), SkBlendMode::kClear);
    }
    return;
  }

  BoxPainter::FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      BoxPainter(layout_view_)
          .CalculateFillLayerOcclusionCulling(
              reversed_paint_list, layout_view_.Style()->BackgroundLayers(),
              layout_view_.GetDocument(), layout_view_.StyleRef());
  DCHECK(reversed_paint_list.size());

  // If the root background color is opaque, isolation group can be skipped
  // because the canvas
  // will be cleared by root background color.
  if (!root_background_color.HasAlpha())
    should_draw_background_in_separate_buffer = false;

  // We are going to clear the canvas with transparent pixels, isolation group
  // can be skipped.
  if (!base_background_color.Alpha() && should_clear_canvas)
    should_draw_background_in_separate_buffer = false;

  if (should_draw_background_in_separate_buffer) {
    if (base_background_color.Alpha()) {
      context.FillRect(
          background_rect, base_background_color,
          should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
    }
    context.BeginLayer();
  }

  Color combined_background_color =
      should_draw_background_in_separate_buffer
          ? root_background_color
          : base_background_color.Blend(root_background_color);

  if (combined_background_color != frame_view.BaseBackgroundColor())
    context.GetPaintController().SetFirstPainted();

  if (combined_background_color.Alpha()) {
    if (!combined_background_color.HasAlpha() &&
        RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      recorder.SetKnownToBeOpaque();
    context.FillRect(
        background_rect, combined_background_color,
        (should_draw_background_in_separate_buffer || should_clear_canvas)
            ? SkBlendMode::kSrc
            : SkBlendMode::kSrcOver);
  } else if (should_clear_canvas &&
             !should_draw_background_in_separate_buffer) {
    context.FillRect(background_rect, Color(), SkBlendMode::kClear);
  }

  BackgroundImageGeometry geometry(layout_view_);
  for (auto it = reversed_paint_list.rbegin(); it != reversed_paint_list.rend();
       ++it) {
    DCHECK((*it)->Clip() == kBorderFillBox);

    bool should_paint_in_viewport_space =
        (*it)->Attachment() == kFixedBackgroundAttachment;
    if (should_paint_in_viewport_space) {
      BoxPainter::PaintFillLayer(layout_view_, paint_info, Color(), **it,
                                 LayoutRect(LayoutRect::InfiniteIntRect()),
                                 kBackgroundBleedNone, geometry);
    } else {
      context.Save();
      // TODO(trchen): We should be able to handle 3D-transformed root
      // background with slimming paint by using transform display items.
      context.ConcatCTM(transform.ToAffineTransform());
      BoxPainter::PaintFillLayer(layout_view_, paint_info, Color(), **it,
                                 LayoutRect(paint_rect), kBackgroundBleedNone,
                                 geometry);
      context.Restore();
    }
  }

  if (should_draw_background_in_separate_buffer)
    context.EndLayer();
}

}  // namespace blink

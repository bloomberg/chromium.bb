// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void ViewPainter::Paint(const PaintInfo& paint_info) {
  // If we ever require layout but receive a paint anyway, something has gone
  // horribly wrong.
  DCHECK(!layout_view_.NeedsLayout());
  DCHECK(!layout_view_.GetFrameView()->ShouldThrottleRendering());

  BlockPainter(layout_view_).Paint(paint_info);
}

void ViewPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info) {
  if (layout_view_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  bool has_touch_action_rect =
      layout_view_.HasEffectiveWhitelistedTouchAction();
  if (!layout_view_.HasBoxDecorationBackground() && !has_touch_action_rect)
    return;

  // The background rect always includes at least the visible content size.
  IntRect background_rect(PixelSnappedIntRect(layout_view_.BackgroundRect()));

  // When printing, paint the entire unclipped scrolling content area.
  if (paint_info.IsPrinting())
    background_rect.Unite(layout_view_.DocumentRect());

  const DisplayItemClient* background_client = &layout_view_;

  base::Optional<ScopedPaintChunkProperties> scoped_scroll_property;
  if (BoxModelObjectPainter::IsPaintingScrollingBackground(&layout_view_,
                                                           paint_info)) {
    // Layout overflow, combined with the visible content size.
    auto document_rect = layout_view_.DocumentRect();
    // DocumentRect is relative to ScrollOrigin. Add ScrollOrigin to let it be
    // in the space of ContentsProperties(). See ScrollTranslation in
    // object_paint_properties.h for details.
    document_rect.MoveBy(layout_view_.ScrollOrigin());
    background_rect.Unite(document_rect);
    background_client = &layout_view_.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
    scoped_scroll_property.emplace(
        paint_info.context.GetPaintController(),
        layout_view_.FirstFragment().ContentsProperties(), *background_client,
        DisplayItem::kDocumentBackground);
  }

  if (layout_view_.HasBoxDecorationBackground()) {
    PaintBoxDecorationBackgroundInternal(paint_info, background_rect,
                                         *background_client);
  }
  if (has_touch_action_rect) {
    BoxPainter(layout_view_)
        .RecordHitTestData(paint_info, LayoutRect(background_rect),
                           *background_client);
  }
}

// This function handles background painting for the LayoutView.
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
void ViewPainter::PaintBoxDecorationBackgroundInternal(
    const PaintInfo& paint_info,
    const IntRect& background_rect,
    const DisplayItemClient& background_client) {
  // TODO(pdr): Can this check be removed? It is not hit in any test.
  if (paint_info.SkipRootBackground())
    return;

  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, background_client, DisplayItem::kDocumentBackground)) {
    return;
  }
  DrawingRecorder recorder(context, background_client,
                           DisplayItem::kDocumentBackground);

  const Document& document = layout_view_.GetDocument();
  const LocalFrameView& frame_view = *layout_view_.GetFrameView();
  bool paints_base_background = document.IsInMainFrame() &&
                                (frame_view.BaseBackgroundColor().Alpha() > 0);
  Color base_background_color =
      paints_base_background ? frame_view.BaseBackgroundColor() : Color();
  Color root_background_color = layout_view_.StyleRef().VisitedDependentColor(
      GetCSSPropertyBackgroundColor());
  const LayoutObject* root_object =
      document.documentElement() ? document.documentElement()->GetLayoutObject()
                                 : nullptr;

  // Special handling for print economy mode.
  bool force_background_to_white =
      BoxModelObjectPainter::ShouldForceWhiteBackgroundForPrintEconomy(
          document, layout_view_.StyleRef());
  if (force_background_to_white) {
    // If for any reason the view background is not transparent, paint white
    // instead, otherwise keep transparent as is.
    if (paints_base_background || root_background_color.Alpha() ||
        layout_view_.StyleRef().BackgroundLayers().AnyLayerHasImage())
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
  // Local / scroll positioned background images will be painted into scrolling
  // contents layer with root layer scrolling. Therefore we need to switch both
  // the background_rect and context to documentElement visual space.
  bool background_renderable = true;
  TransformationMatrix transform;
  IntRect paint_rect = background_rect;
  if (!root_object || !root_object->IsBox()) {
    background_renderable = false;
  } else if (root_object->HasLayer()) {
    if (BoxModelObjectPainter::IsPaintingScrollingBackground(&layout_view_,
                                                             paint_info)) {
      transform.Translate(layout_view_.ScrolledContentOffset().Width(),
                          layout_view_.ScrolledContentOffset().Height());
    }
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

  bool should_clear_canvas =
      paints_base_background &&
      (document.GetSettings() &&
       document.GetSettings()->GetShouldClearDocumentBackground());
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

  BoxPainterBase::FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      BoxModelObjectPainter(layout_view_)
          .CalculateFillLayerOcclusionCulling(
              reversed_paint_list, layout_view_.StyleRef().BackgroundLayers());
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
        RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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
  BoxModelObjectPainter box_model_painter(layout_view_);
  for (auto it = reversed_paint_list.rbegin(); it != reversed_paint_list.rend();
       ++it) {
    DCHECK((*it)->Clip() == EFillBox::kBorder);

    bool should_paint_in_viewport_space =
        (*it)->Attachment() == EFillAttachment::kFixed;
    if (should_paint_in_viewport_space) {
      box_model_painter.PaintFillLayer(paint_info, Color(), **it,
                                       LayoutRect(background_rect),
                                       kBackgroundBleedNone, geometry);
    } else {
      context.Save();
      // TODO(trchen): We should be able to handle 3D-transformed root
      // background with slimming paint by using transform display items.
      context.ConcatCTM(transform.ToAffineTransform());
      box_model_painter.PaintFillLayer(paint_info, Color(), **it,
                                       LayoutRect(paint_rect),
                                       kBackgroundBleedNone, geometry);
      context.Restore();
    }
  }

  if (should_draw_background_in_separate_buffer)
    context.EndLayer();
}

}  // namespace blink

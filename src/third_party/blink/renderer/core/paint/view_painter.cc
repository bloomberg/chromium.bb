// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
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

// Behind the root element of the main frame of the page, there is an infinite
// canvas. This is by default white, but it can be overridden by
// BaseBackgroundColor on the LocalFrameView.
// https://drafts.fxtf.org/compositing/#rootgroup
void ViewPainter::PaintRootGroup(const PaintInfo& paint_info,
                                 const IntRect& pixel_snapped_background_rect,
                                 const Document& document,
                                 const DisplayItemClient& client,
                                 const PropertyTreeState& state) {
  if (!document.IsInMainFrame())
    return;
  bool should_clear_canvas =
      document.GetSettings() &&
      document.GetSettings()->GetShouldClearDocumentBackground();

  Color base_background_color =
      layout_view_.GetFrameView()->BaseBackgroundColor();

  ScopedPaintChunkProperties frame_view_background_state(
      paint_info.context.GetPaintController(), state, client,
      DisplayItem::kDocumentRootBackdrop);
  GraphicsContext& context = paint_info.context;
  if (!DrawingRecorder::UseCachedDrawingIfPossible(
          context, client, DisplayItem::kDocumentRootBackdrop)) {
    DrawingRecorder recorder(context, client,
                             DisplayItem::kDocumentRootBackdrop);
    context.FillRect(
        pixel_snapped_background_rect, base_background_color,
        should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
  }
}

void ViewPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info) {
  if (layout_view_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  bool has_touch_action_rect = layout_view_.HasEffectiveAllowedTouchAction();
  bool painting_scrolling_background =
      BoxDecorationData::IsPaintingScrollingBackground(paint_info,
                                                       layout_view_);
  bool paints_scroll_hit_test =
      !painting_scrolling_background &&
      layout_view_.FirstFragment().PaintProperties()->Scroll();
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Pre-CompositeAfterPaint, there is no need to emit scroll hit test
    // display items for composited scrollers because these display items are
    // only used to create non-fast scrollable regions for non-composited
    // scrollers. With CompositeAfterPaint, we always paint the scroll hit
    // test display items but ignore the non-fast region if the scroll was
    // composited in PaintArtifactCompositor::UpdateNonFastScrollableRegions.
    if (layout_view_.HasLayer() &&
        layout_view_.Layer()->GetCompositedLayerMapping() &&
        layout_view_.Layer()
            ->GetCompositedLayerMapping()
            ->HasScrollingLayer()) {
      paints_scroll_hit_test = false;
    }
  }

  if (!layout_view_.HasBoxDecorationBackground() && !has_touch_action_rect &&
      !paints_scroll_hit_test)
    return;

  // The background rect always includes at least the visible content size.
  PhysicalRect background_rect(layout_view_.BackgroundRect());

  // When printing or painting a preview, paint the entire unclipped scrolling
  // content area.
  if (paint_info.IsPrinting() ||
      !layout_view_.GetFrameView()->GetFrame().ClipsContent()) {
    background_rect.Unite(layout_view_.DocumentRect());
  }

  const DisplayItemClient* background_client = &layout_view_;

  if (painting_scrolling_background) {
    // Layout overflow, combined with the visible content size.
    auto document_rect = layout_view_.DocumentRect();
    // DocumentRect is relative to ScrollOrigin. Add ScrollOrigin to let it be
    // in the space of ContentsProperties(). See ScrollTranslation in
    // object_paint_properties.h for details.
    document_rect.Move(PhysicalOffset(layout_view_.ScrollOrigin()));
    background_rect.Unite(document_rect);
    background_client = &layout_view_.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
  }

  IntRect pixel_snapped_background_rect(PixelSnappedIntRect(background_rect));

  const Document& document = layout_view_.GetDocument();

  PropertyTreeState root_element_background_painting_state =
      layout_view_.FirstFragment().ContentsProperties();

  base::Optional<ScopedPaintChunkProperties> scoped_properties;

  bool painted_separate_backdrop = false;
  bool painted_separate_effect = false;

  bool should_apply_root_background_behavior =
      layout_view_.GetDocument().IsHTMLDocument() ||
      layout_view_.GetDocument().IsXHTMLDocument();

  bool should_paint_background = !paint_info.SkipRootBackground() &&
                                 layout_view_.HasBoxDecorationBackground();

  LayoutObject* root_object = nullptr;
  if (layout_view_.GetDocument().documentElement()) {
    root_object =
        layout_view_.GetDocument().documentElement()->GetLayoutObject();
  }

  // For HTML and XHTML documents, the root element may paint in a different
  // clip, effect or transform state than the LayoutView. For
  // example, the HTML element may have a clip-path, filter, blend-mode,
  // opacity or transform.
  //
  // In these cases, we should paint the background of the root element in
  // its LocalBorderBoxProperties() state, as part of the Root Element Group
  // [1]. In addition, for the main frame of the page, we also need to paint the
  // default backdrop color in the Root Group [2]. The Root Group paints in
  // the scrolling space of the LayoutView (i.e. its ContentsProperties()).
  //
  // [1] https://drafts.fxtf.org/compositing/#pagebackdrop
  // [2] https://drafts.fxtf.org/compositing/#rootgroup
  if (should_paint_background && painting_scrolling_background &&
      should_apply_root_background_behavior && root_object) {
    const PropertyTreeState& document_element_state =
        root_object->FirstFragment().LocalBorderBoxProperties();

    // As an optimization, only paint a separate PaintChunk for the
    // root group if its property tree state differs from root element
    // group's. Otherwise we can usually avoid both a separate
    // PaintChunk and a BeginLayer/EndLayer.
    if (document_element_state != root_element_background_painting_state) {
      if (&document_element_state.Effect() !=
          &root_element_background_painting_state.Effect())
        painted_separate_effect = true;

      root_element_background_painting_state = document_element_state;
      PaintRootGroup(paint_info, pixel_snapped_background_rect, document,
                     *background_client,
                     layout_view_.FirstFragment().ContentsProperties());
      painted_separate_backdrop = true;
    }
  }

  if (painting_scrolling_background) {
    scoped_properties.emplace(paint_info.context.GetPaintController(),
                              root_element_background_painting_state,
                              *background_client,
                              DisplayItem::kDocumentBackground);
  }

  if (should_paint_background) {
    PaintRootElementGroup(paint_info, pixel_snapped_background_rect,
                          root_element_background_painting_state,
                          *background_client, painted_separate_backdrop,
                          painted_separate_effect);
  }
  if (has_touch_action_rect) {
    BoxPainter(layout_view_)
        .RecordHitTestData(paint_info,
                           PhysicalRect(pixel_snapped_background_rect),
                           *background_client);
  }

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (paints_scroll_hit_test) {
    DCHECK(!painting_scrolling_background);
    BoxPainter(layout_view_)
        .RecordScrollHitTestData(paint_info, *background_client);
  }
}

// This function handles background painting for the LayoutView.
// View background painting is special in the following ways:
// 1. The view paints background for the root element, the background
//    positioning respects the positioning and transformation of the root
//    element. However, this method assumes that there is already an
//    PaintChunk being recorded with the LocalBorderBoxProperties of the
//    root element. Therefore the transform of the root element
//    are applied via PaintChunksToCcLayer, and not via the display list of the
//    PaintChunk itself.
// 2. CSS background-clip is ignored, the background layers always expand to
//    cover the whole canvas.
// 3. The main frame is also responsible for painting the user-agent-defined
//    base background color. Conceptually it should be painted by the embedder
//    but painting it here allows culling and pre-blending optimization when
//    possible.
void ViewPainter::PaintRootElementGroup(
    const PaintInfo& paint_info,
    const IntRect& pixel_snapped_background_rect,
    const PropertyTreeState& background_paint_state,
    const DisplayItemClient& background_client,
    bool painted_separate_backdrop,
    bool painted_separate_effect) {
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
  Color root_element_background_color =
      layout_view_.StyleRef().VisitedDependentColor(
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
    if (paints_base_background || root_element_background_color.Alpha() ||
        layout_view_.StyleRef().BackgroundLayers().AnyLayerHasImage()) {
      context.FillRect(pixel_snapped_background_rect, Color::kWhite,
                       SkBlendMode::kSrc);
    }
    return;
  }

  // Compute the enclosing rect of the view, in root element space.
  //
  // For background colors we can simply paint the document rect in the default
  // space. However, for background image, the root element paint offset and
  // transforms apply. The strategy is to issue draw commands in the root
  // element's local space, which requires mapping the document background rect.
  bool background_renderable = true;
  IntRect paint_rect = pixel_snapped_background_rect;
  // Offset for BackgroundImageGeometry to offset the image's origin. This makes
  // background tiling start at the root element's origin instead of the view.
  // This is different from the offset for painting, which is in |paint_rect|.
  LayoutPoint background_image_offset;
  if (!root_object || !root_object->IsBox()) {
    background_renderable = false;
  } else {
    const auto& view_contents_state =
        layout_view_.FirstFragment().ContentsProperties();
    if (view_contents_state != background_paint_state) {
      GeometryMapper::SourceToDestinationRect(
          view_contents_state.Transform(), background_paint_state.Transform(),
          paint_rect);
      if (paint_rect.IsEmpty())
        background_renderable = false;
      // With transforms, paint offset is encoded in paint property nodes but we
      // can use the |paint_rect|'s adjusted location as the offset from the
      // view to the root element.
      background_image_offset = paint_rect.Location();
    } else {
      background_image_offset =
          -root_object->FirstFragment().PaintOffset().ToLayoutPoint();
    }
  }

  bool should_clear_canvas =
      paints_base_background &&
      (document.GetSettings() &&
       document.GetSettings()->GetShouldClearDocumentBackground());

  if (!background_renderable) {
    if (!painted_separate_backdrop) {
      if (base_background_color.Alpha()) {
        context.FillRect(
            pixel_snapped_background_rect, base_background_color,
            should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
      } else if (should_clear_canvas) {
        context.FillRect(pixel_snapped_background_rect, Color(),
                         SkBlendMode::kClear);
      }
    }
    return;
  }

  BoxPainterBase::FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      BoxModelObjectPainter(layout_view_)
          .CalculateFillLayerOcclusionCulling(
              reversed_paint_list, layout_view_.StyleRef().BackgroundLayers());
  DCHECK(reversed_paint_list.size());

  if (painted_separate_effect) {
    should_draw_background_in_separate_buffer = true;
  } else {
    // If the root background color is opaque, isolation group can be skipped
    // because the canvas
    // will be cleared by root background color.
    if (!root_element_background_color.HasAlpha())
      should_draw_background_in_separate_buffer = false;

    // We are going to clear the canvas with transparent pixels, isolation group
    // can be skipped.
    if (!base_background_color.Alpha() && should_clear_canvas)
      should_draw_background_in_separate_buffer = false;
  }

  // Only use BeginLayer if not only we should draw in a separate buffer, but
  // we also didn't paint a separate backdrop. Separate backdrops are always
  // painted when there is any effect on the root element, such as a blend
  // mode. An extra BeginLayer will result in incorrect blend isolation if
  // it is added on top of any effect on the root element.
  if (should_draw_background_in_separate_buffer && !painted_separate_effect) {
    if (base_background_color.Alpha()) {
      context.FillRect(
          paint_rect, base_background_color,
          should_clear_canvas ? SkBlendMode::kSrc : SkBlendMode::kSrcOver);
    }
    context.BeginLayer();
  }

  Color combined_background_color =
      should_draw_background_in_separate_buffer
          ? root_element_background_color
          : base_background_color.Blend(root_element_background_color);

  if (combined_background_color != frame_view.BaseBackgroundColor())
    context.GetPaintController().SetFirstPainted();

  if (combined_background_color.Alpha()) {
    context.FillRect(
        paint_rect, combined_background_color,
        (should_draw_background_in_separate_buffer || should_clear_canvas)
            ? SkBlendMode::kSrc
            : SkBlendMode::kSrcOver);
  } else if (should_clear_canvas &&
             !should_draw_background_in_separate_buffer) {
    context.FillRect(paint_rect, Color(), SkBlendMode::kClear);
  }

  BackgroundImageGeometry geometry(layout_view_, background_image_offset);
  BoxModelObjectPainter box_model_painter(layout_view_);
  for (const auto* fill_layer : base::Reversed(reversed_paint_list)) {
    DCHECK(fill_layer->Clip() == EFillBox::kBorder);
    box_model_painter.PaintFillLayer(paint_info, Color(), *fill_layer,
                                     PhysicalRect(paint_rect),
                                     kBackgroundBleedNone, geometry);
  }

  if (should_draw_background_in_separate_buffer && !painted_separate_effect)
    context.EndLayer();
}

}  // namespace blink

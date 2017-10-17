// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPainter.h"

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTheme.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/BoxModelObjectPainter.h"
#include "core/paint/BoxPainterBase.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ThemePainter.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

void BoxPainter::Paint(const PaintInfo& paint_info,
                       const LayoutPoint& paint_offset) {
  ObjectPainter(layout_box_).CheckPaintOffset(paint_info, paint_offset);
  // Default implementation. Just pass paint through to the children.
  PaintChildren(paint_info, paint_offset + layout_box_.Location());
}

void BoxPainter::PaintChildren(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  PaintInfo child_info(paint_info);
  for (LayoutObject* child = layout_box_.SlowFirstChild(); child;
       child = child->NextSibling())
    child->Paint(child_info, paint_offset);
}

void BoxPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info,
                                              const LayoutPoint& paint_offset) {
  LayoutRect paint_rect;
  Optional<ScrollRecorder> scroll_recorder;
  if (BoxModelObjectPainter::
          IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
              &layout_box_, paint_info)) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    paint_rect = layout_box_.LayoutOverflowRect();
    scroll_recorder.emplace(paint_info.context, layout_box_, paint_info.phase,
                            layout_box_.ScrolledContentOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.Expand(layout_box_.BorderBoxOutsets());
  } else {
    paint_rect = layout_box_.BorderBoxRect();
  }

  paint_rect.MoveBy(paint_offset);
  PaintBoxDecorationBackgroundWithRect(paint_info, paint_offset, paint_rect);
}

LayoutRect BoxPainter::BoundsForDrawingRecorder(
    const PaintInfo& paint_info,
    const LayoutPoint& adjusted_paint_offset) {
  LayoutRect bounds =
      BoxModelObjectPainter::
              IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
                  &layout_box_, paint_info)
          ? layout_box_.LayoutOverflowRect()
          : layout_box_.SelfVisualOverflowRect();
  bounds.MoveBy(adjusted_paint_offset);
  return bounds;
}

void BoxPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutRect& paint_rect) {
  bool painting_overflow_contents = BoxModelObjectPainter::
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &layout_box_, paint_info);
  const ComputedStyle& style = layout_box_.StyleRef();

  Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data, and for
  // delayed-invalidation object because it may change before it's actually
  // invalidated.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      (style.Appearance() == kMediaSliderPart ||
       layout_box_.FullPaintInvalidationReason() ==
           PaintInvalidationReason::kDelayedFull)) {
    cache_skipper.emplace(paint_info.context);
  }

  const DisplayItemClient& display_item_client =
      painting_overflow_contents ? static_cast<const DisplayItemClient&>(
                                       *layout_box_.Layer()
                                            ->GetCompositedLayerMapping()
                                            ->ScrollingContentsLayer())
                                 : layout_box_;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(
      paint_info.context, display_item_client,
      DisplayItem::kBoxDecorationBackground,
      FloatRect(BoundsForDrawingRecorder(paint_info, paint_offset)));
  BoxDecorationData box_decoration_data(layout_box_);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      LayoutRect(EnclosingIntRect(paint_rect)) == paint_rect &&
      layout_box_.BackgroundIsKnownToBeOpaqueInRect(
          BoundsForDrawingRecorder(paint_info, LayoutPoint())))
    recorder.SetKnownToBeOpaque();

  if (!painting_overflow_contents) {
    // FIXME: Should eventually give the theme control over whether the box
    // shadow should paint, since controls could have custom shadows of their
    // own.
    BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect, style);

    if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(paint_rect);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
        paint_info.context.BeginLayer();
    }
  }

  // If we have a native theme appearance, paint that before painting our
  // background.  The theme will tell us whether or not we should also paint the
  // CSS background.
  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.has_appearance &&
      !theme_painter.Paint(layout_box_, paint_info, snapped_paint_rect);
  bool should_paint_background =
      !theme_painted && (!paint_info.SkipRootBackground() ||
                         paint_info.PaintContainer() != &layout_box_);
  if (should_paint_background) {
    PaintBackground(paint_info, paint_rect,
                    box_decoration_data.background_color,
                    box_decoration_data.bleed_avoidance);

    if (box_decoration_data.has_appearance)
      theme_painter.PaintDecorations(layout_box_.GetNode(), style, paint_info,
                                     snapped_paint_rect);
  }

  if (!painting_overflow_contents) {
    BoxPainterBase::PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect,
                                                      style);

    // The theme will tell us whether or not we should also paint the CSS
    // border.
    if (box_decoration_data.has_border_decoration &&
        (!box_decoration_data.has_appearance ||
         (!theme_painted &&
          LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box_.GetNode(), style, paint_info, snapped_paint_rect))) &&
        !(layout_box_.IsTable() &&
          ToLayoutTable(&layout_box_)->ShouldCollapseBorders())) {
      BoxPainterBase::PaintBorder(
          layout_box_, layout_box_.GetDocument(), layout_box_.GeneratingNode(),
          paint_info, paint_rect, style, box_decoration_data.bleed_avoidance);
    }
  }

  if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
    paint_info.context.EndLayer();
}

void BoxPainter::PaintBackground(const PaintInfo& paint_info,
                                 const LayoutRect& paint_rect,
                                 const Color& background_color,
                                 BackgroundBleedAvoidance bleed_avoidance) {
  if (layout_box_.IsDocumentElement())
    return;
  if (layout_box_.BackgroundStolenForBeingBody())
    return;
  if (layout_box_.BackgroundIsKnownToBeObscured())
    return;
  BackgroundImageGeometry geometry(layout_box_);
  BoxModelObjectPainter box_model_painter(layout_box_);
  box_model_painter.PaintFillLayers(paint_info, background_color,
                                    layout_box_.Style()->BackgroundLayers(),
                                    paint_rect, geometry, bleed_avoidance);
}

void BoxPainter::PaintMask(const PaintInfo& paint_info,
                           const LayoutPoint& paint_offset) {
  if (layout_box_.Style()->Visibility() != EVisibility::kVisible ||
      paint_info.phase != PaintPhase::kMask)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_box_, paint_info.phase))
    return;

  LayoutRect visual_overflow_rect(layout_box_.VisualOverflowRect());
  visual_overflow_rect.MoveBy(paint_offset);

  DrawingRecorder recorder(paint_info.context, layout_box_, paint_info.phase,
                           visual_overflow_rect);
  LayoutRect paint_rect = LayoutRect(paint_offset, layout_box_.Size());
  PaintMaskImages(paint_info, paint_rect);
}

void BoxPainter::PaintMaskImages(const PaintInfo& paint_info,
                                 const LayoutRect& paint_rect) {
  // Figure out if we need to push a transparency layer to render our mask.
  bool push_transparency_layer = false;
  bool flatten_compositing_layers =
      paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers;
  bool mask_blending_applied_by_compositor =
      !flatten_compositing_layers && layout_box_.HasLayer() &&
      layout_box_.Layer()->MaskBlendingAppliedByCompositor();

  bool all_mask_images_loaded = true;

  if (!mask_blending_applied_by_compositor) {
    push_transparency_layer = true;
    StyleImage* mask_box_image = layout_box_.Style()->MaskBoxImage().GetImage();
    const FillLayer& mask_layers = layout_box_.Style()->MaskLayers();

    // Don't render a masked element until all the mask images have loaded, to
    // prevent a flash of unmasked content.
    if (mask_box_image)
      all_mask_images_loaded &= mask_box_image->IsLoaded();

    all_mask_images_loaded &= mask_layers.ImagesAreLoaded();

    paint_info.context.BeginLayer(1, SkBlendMode::kDstIn);
  }

  if (all_mask_images_loaded) {
    BackgroundImageGeometry geometry(layout_box_);
    BoxModelObjectPainter box_model_painter(layout_box_);
    box_model_painter.PaintFillLayers(paint_info, Color::kTransparent,
                                      layout_box_.Style()->MaskLayers(),
                                      paint_rect, geometry);
    NinePieceImagePainter::Paint(
        paint_info.context, layout_box_, layout_box_.GetDocument(),
        layout_box_.GeneratingNode(), paint_rect, layout_box_.StyleRef(),
        layout_box_.StyleRef().MaskBoxImage());
  }

  if (push_transparency_layer)
    paint_info.context.EndLayer();
}

void BoxPainter::PaintClippingMask(const PaintInfo& paint_info,
                                   const LayoutPoint& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kClippingMask);

  if (layout_box_.Style()->Visibility() != EVisibility::kVisible)
    return;

  if (!layout_box_.Layer() ||
      layout_box_.Layer()->GetCompositingState() != kPaintsIntoOwnBacking)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_box_, paint_info.phase))
    return;

  IntRect paint_rect =
      PixelSnappedIntRect(LayoutRect(paint_offset, layout_box_.Size()));
  DrawingRecorder recorder(paint_info.context, layout_box_, paint_info.phase,
                           paint_rect);
  paint_info.context.FillRect(paint_rect, Color::kBlack);
}

}  // namespace blink

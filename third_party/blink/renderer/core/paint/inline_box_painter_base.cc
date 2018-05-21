// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_box_painter_base.h"

#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void InlineBoxPainterBase::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    LayoutRect adjusted_frame_rect,
    BackgroundImageGeometry geometry,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  // Shadow comes first and is behind the background and border.
  PaintNormalBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  Color background_color =
      line_style_.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  PaintFillLayers(paint_info, background_color, line_style_.BackgroundLayers(),
                  adjusted_frame_rect, geometry);

  PaintInsetBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  IntRect adjusted_clip_rect;
  BorderPaintingType border_painting_type =
      GetBorderPaintType(adjusted_frame_rect, adjusted_clip_rect);
  switch (border_painting_type) {
    case kDontPaintBorders:
      break;
    case kPaintBordersWithoutClip:
      BoxPainterBase::PaintBorder(
          image_observer_, *document_, node_, paint_info, adjusted_frame_rect,
          line_style_, kBackgroundBleedNone, include_logical_left_edge,
          include_logical_right_edge);
      break;
    case kPaintBordersWithClip:
      // FIXME: What the heck do we do with RTL here? The math we're using is
      // obviously not right, but it isn't even clear how this should work at
      // all.
      LayoutRect image_strip_paint_rect =
          PaintRectForImageStrip(adjusted_frame_rect, TextDirection::kLtr);
      GraphicsContextStateSaver state_saver(paint_info.context);
      paint_info.context.Clip(adjusted_clip_rect);
      BoxPainterBase::PaintBorder(image_observer_, *document_, node_,
                                  paint_info, image_strip_paint_rect,
                                  line_style_);
      break;
  }
}

void InlineBoxPainterBase::PaintFillLayers(const PaintInfo& paint_info,
                                           const Color& c,
                                           const FillLayer& fill_layer,
                                           const LayoutRect& rect,
                                           BackgroundImageGeometry& geometry,
                                           SkBlendMode op) {
  // FIXME: This should be a for loop or similar. It's a little non-trivial to
  // do so, however, since the layers need to be painted in reverse order.
  if (fill_layer.Next())
    PaintFillLayers(paint_info, c, *fill_layer.Next(), rect, geometry, op);
  PaintFillLayer(paint_info, c, fill_layer, rect, geometry, op);
}

void InlineBoxPainterBase::PaintFillLayer(const PaintInfo& paint_info,
                                          const Color& c,
                                          const FillLayer& fill_layer,
                                          const LayoutRect& paint_rect,
                                          BackgroundImageGeometry& geometry,
                                          SkBlendMode op) {
  StyleImage* img = fill_layer.GetImage();
  bool has_fill_image = img && img->CanRender();

  if ((!has_fill_image && !style_.HasBorderRadius()) ||
      !object_has_multiple_boxes_) {
    box_painter_->PaintFillLayer(paint_info, c, fill_layer, paint_rect,
                                 kBackgroundBleedNone, geometry, op, false);
    return;
  }

  // Handle fill images that spans multiple lines.
  LayoutRect rect = style_.BoxDecorationBreak() != EBoxDecorationBreak::kClone
                        ? PaintRectForImageStrip(paint_rect, style_.Direction())
                        : paint_rect;

  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.Clip(PixelSnappedIntRect(paint_rect));
  box_painter_->PaintFillLayer(paint_info, c, fill_layer, rect,
                               kBackgroundBleedNone, geometry, op, true,
                               paint_rect.Size());
}

void InlineBoxPainterBase::PaintNormalBoxShadow(const PaintInfo& info,
                                                const ComputedStyle& s,
                                                const LayoutRect& paint_rect) {
  BoxPainterBase::PaintNormalBoxShadow(
      info, paint_rect, s, include_logical_left_edge_for_box_shadow_,
      include_logical_right_edge_for_box_shadow_);
}

void InlineBoxPainterBase::PaintInsetBoxShadow(const PaintInfo& info,
                                               const ComputedStyle& s,
                                               const LayoutRect& paint_rect) {
  BoxPainterBase::PaintInsetBoxShadowWithBorderRect(
      info, paint_rect, s, include_logical_left_edge_for_box_shadow_,
      include_logical_right_edge_for_box_shadow_);
}

}  // namespace blink

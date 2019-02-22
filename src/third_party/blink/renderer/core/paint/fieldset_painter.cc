// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fieldset_painter.h"

#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/fieldset_paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

FieldsetPaintInfo CreateFieldsetPaintInfo(const LayoutBox& fieldset,
                                          const LayoutBox& legend) {
  LayoutRectOutsets fieldset_borders(
      fieldset.BorderTop(), fieldset.BorderRight(),
      LayoutUnit(),  // bottom border will always be left alone.
      fieldset.BorderLeft());
  return FieldsetPaintInfo(fieldset.StyleRef(), fieldset.Size(),
                           fieldset_borders, legend.FrameRect());
}

}  // anonymous namespace

void FieldsetPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutRect paint_rect(paint_offset, layout_fieldset_.Size());
  LayoutBox* legend = layout_fieldset_.FindInFlowLegend();
  if (!legend)
    return BoxPainter(layout_fieldset_)
        .PaintBoxDecorationBackground(paint_info, paint_offset);

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_fieldset_, paint_info.phase))
    return;

  FieldsetPaintInfo fieldset_paint_info =
      CreateFieldsetPaintInfo(layout_fieldset_, *legend);
  paint_rect.Contract(fieldset_paint_info.border_outsets);

  DrawingRecorder recorder(paint_info.context, layout_fieldset_,
                           paint_info.phase);
  BoxDecorationData box_decoration_data(layout_fieldset_);

  BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect,
                                       layout_fieldset_.StyleRef());
  BackgroundImageGeometry geometry(layout_fieldset_);
  BoxModelObjectPainter(layout_fieldset_)
      .PaintFillLayers(paint_info, box_decoration_data.background_color,
                       layout_fieldset_.StyleRef().BackgroundLayers(),
                       paint_rect, geometry);
  BoxPainterBase::PaintInsetBoxShadowWithBorderRect(
      paint_info, paint_rect, layout_fieldset_.StyleRef());

  if (!box_decoration_data.has_border_decoration)
    return;

  // Create a clipping region around the legend and paint the border as normal
  GraphicsContext& graphics_context = paint_info.context;
  GraphicsContextStateSaver state_saver(graphics_context);

  LayoutRect legend_cutout_rect = fieldset_paint_info.legend_cutout_rect;
  legend_cutout_rect.MoveBy(paint_offset);
  graphics_context.ClipOut(PixelSnappedIntRect(legend_cutout_rect));

  Node* node = nullptr;
  const LayoutObject* layout_object = &layout_fieldset_;
  for (; layout_object && !node; layout_object = layout_object->Parent())
    node = layout_object->GeneratingNode();
  BoxPainterBase::PaintBorder(layout_fieldset_, layout_fieldset_.GetDocument(),
                              node, paint_info, paint_rect,
                              layout_fieldset_.StyleRef());
}

void FieldsetPainter::PaintMask(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) {
  if (layout_fieldset_.StyleRef().Visibility() != EVisibility::kVisible ||
      paint_info.phase != PaintPhase::kMask)
    return;

  LayoutRect paint_rect = LayoutRect(paint_offset, layout_fieldset_.Size());
  LayoutBox* legend = layout_fieldset_.FindInFlowLegend();
  if (!legend)
    return BoxPainter(layout_fieldset_).PaintMask(paint_info, paint_offset);

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_fieldset_, paint_info.phase))
    return;

  FieldsetPaintInfo fieldset_paint_info =
      CreateFieldsetPaintInfo(layout_fieldset_, *legend);
  paint_rect.Contract(fieldset_paint_info.border_outsets);

  DrawingRecorder recorder(paint_info.context, layout_fieldset_,
                           paint_info.phase);
  BoxPainter(layout_fieldset_).PaintMaskImages(paint_info, paint_rect);
}

}  // namespace blink

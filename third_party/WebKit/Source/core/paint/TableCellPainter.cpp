// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableCellPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

static const CollapsedBorderValue& CollapsedLeftBorder(
    const ComputedStyle& style_for_cell_flow,
    const LayoutTableCell::CollapsedBorderValues& values) {
  if (style_for_cell_flow.IsHorizontalWritingMode()) {
    return style_for_cell_flow.IsLeftToRightDirection() ? values.StartBorder()
                                                        : values.EndBorder();
  }
  return style_for_cell_flow.IsFlippedBlocksWritingMode()
             ? values.AfterBorder()
             : values.BeforeBorder();
}

static const CollapsedBorderValue& CollapsedRightBorder(
    const ComputedStyle& style_for_cell_flow,
    const LayoutTableCell::CollapsedBorderValues& values) {
  if (style_for_cell_flow.IsHorizontalWritingMode()) {
    return style_for_cell_flow.IsLeftToRightDirection() ? values.EndBorder()
                                                        : values.StartBorder();
  }
  return style_for_cell_flow.IsFlippedBlocksWritingMode()
             ? values.BeforeBorder()
             : values.AfterBorder();
}

static const CollapsedBorderValue& CollapsedTopBorder(
    const ComputedStyle& style_for_cell_flow,
    const LayoutTableCell::CollapsedBorderValues& values) {
  if (style_for_cell_flow.IsHorizontalWritingMode())
    return values.BeforeBorder();
  return style_for_cell_flow.IsLeftToRightDirection() ? values.StartBorder()
                                                      : values.EndBorder();
}

static const CollapsedBorderValue& CollapsedBottomBorder(
    const ComputedStyle& style_for_cell_flow,
    const LayoutTableCell::CollapsedBorderValues& values) {
  if (style_for_cell_flow.IsHorizontalWritingMode())
    return values.AfterBorder();
  return style_for_cell_flow.IsLeftToRightDirection() ? values.EndBorder()
                                                      : values.StartBorder();
}

void TableCellPainter::Paint(const PaintInfo& paint_info,
                             const LayoutPoint& paint_offset) {
  BlockPainter(layout_table_cell_).Paint(paint_info, paint_offset);
}

static EBorderStyle CollapsedBorderStyle(EBorderStyle style) {
  if (style == EBorderStyle::kOutset)
    return EBorderStyle::kGroove;
  if (style == EBorderStyle::kInset)
    return EBorderStyle::kRidge;
  return style;
}

const DisplayItemClient& TableCellPainter::DisplayItemClientForBorders() const {
  // TODO(wkorman): We may need to handle PaintInvalidationDelayedFull.
  // http://crbug.com/657186
  return layout_table_cell_.UsesCompositedCellDisplayItemClients()
             ? static_cast<const DisplayItemClient&>(
                   *layout_table_cell_.GetCollapsedBorderValues())
             : layout_table_cell_;
}

void TableCellPainter::PaintCollapsedBorders(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const CollapsedBorderValue& current_border_value) {
  if (layout_table_cell_.Style()->Visibility() != EVisibility::kVisible)
    return;

  LayoutPoint adjusted_paint_offset =
      paint_offset + layout_table_cell_.Location();
  if (!BlockPainter(layout_table_cell_)
           .IntersectsPaintRect(paint_info, adjusted_paint_offset))
    return;

  const LayoutTableCell::CollapsedBorderValues* values =
      layout_table_cell_.GetCollapsedBorderValues();
  if (!values)
    return;

  const ComputedStyle& style_for_cell_flow =
      layout_table_cell_.StyleForCellFlow();
  const CollapsedBorderValue& left_border_value =
      CollapsedLeftBorder(style_for_cell_flow, *values);
  const CollapsedBorderValue& right_border_value =
      CollapsedRightBorder(style_for_cell_flow, *values);
  const CollapsedBorderValue& top_border_value =
      CollapsedTopBorder(style_for_cell_flow, *values);
  const CollapsedBorderValue& bottom_border_value =
      CollapsedBottomBorder(style_for_cell_flow, *values);

  int display_item_type = DisplayItem::kTableCollapsedBorderBase;
  if (top_border_value.ShouldPaint(current_border_value))
    display_item_type |= DisplayItem::kTableCollapsedBorderTop;
  if (bottom_border_value.ShouldPaint(current_border_value))
    display_item_type |= DisplayItem::kTableCollapsedBorderBottom;
  if (left_border_value.ShouldPaint(current_border_value))
    display_item_type |= DisplayItem::kTableCollapsedBorderLeft;
  if (right_border_value.ShouldPaint(current_border_value))
    display_item_type |= DisplayItem::kTableCollapsedBorderRight;
  if (display_item_type == DisplayItem::kTableCollapsedBorderBase)
    return;

  int top_width = top_border_value.Width();
  int bottom_width = bottom_border_value.Width();
  int left_width = left_border_value.Width();
  int right_width = right_border_value.Width();

  // Adjust our x/y/width/height so that we paint the collapsed borders at the
  // correct location.
  LayoutRect paint_rect =
      PaintRectNotIncludingVisualOverflow(adjusted_paint_offset);
  IntRect border_rect = PixelSnappedIntRect(
      paint_rect.X() - left_width / 2, paint_rect.Y() - top_width / 2,
      paint_rect.Width() + left_width / 2 + (right_width + 1) / 2,
      paint_rect.Height() + top_width / 2 + (bottom_width + 1) / 2);

  GraphicsContext& graphics_context = paint_info.context;
  const DisplayItemClient& client = DisplayItemClientForBorders();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          graphics_context, client,
          static_cast<DisplayItem::Type>(display_item_type)))
    return;

  DrawingRecorder recorder(graphics_context, client,
                           static_cast<DisplayItem::Type>(display_item_type),
                           border_rect);

  // We never paint diagonals at the joins.  We simply let the border with the
  // highest precedence paint on top of borders with lower precedence.
  if (display_item_type & DisplayItem::kTableCollapsedBorderTop) {
    ObjectPainter::DrawLineForBoxSide(
        graphics_context, border_rect.X(), border_rect.Y(), border_rect.MaxX(),
        border_rect.Y() + top_width, kBSTop, top_border_value.GetColor(),
        CollapsedBorderStyle(top_border_value.Style()), 0, 0, true);
  }
  if (display_item_type & DisplayItem::kTableCollapsedBorderBottom) {
    ObjectPainter::DrawLineForBoxSide(
        graphics_context, border_rect.X(), border_rect.MaxY() - bottom_width,
        border_rect.MaxX(), border_rect.MaxY(), kBSBottom,
        bottom_border_value.GetColor(),
        CollapsedBorderStyle(bottom_border_value.Style()), 0, 0, true);
  }
  if (display_item_type & DisplayItem::kTableCollapsedBorderLeft) {
    ObjectPainter::DrawLineForBoxSide(
        graphics_context, border_rect.X(), border_rect.Y(),
        border_rect.X() + left_width, border_rect.MaxY(), kBSLeft,
        left_border_value.GetColor(),
        CollapsedBorderStyle(left_border_value.Style()), 0, 0, true);
  }
  if (display_item_type & DisplayItem::kTableCollapsedBorderRight) {
    ObjectPainter::DrawLineForBoxSide(
        graphics_context, border_rect.MaxX() - right_width, border_rect.Y(),
        border_rect.MaxX(), border_rect.MaxY(), kBSRight,
        right_border_value.GetColor(),
        CollapsedBorderStyle(right_border_value.Style()), 0, 0, true);
  }
}

void TableCellPainter::PaintContainerBackgroundBehindCell(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutObject& background_object) {
  DCHECK(background_object != layout_table_cell_);

  if (layout_table_cell_.Style()->Visibility() != EVisibility::kVisible)
    return;

  LayoutTable* table = layout_table_cell_.Table();
  if (!table->ShouldCollapseBorders() &&
      layout_table_cell_.Style()->EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(
      paint_offset + layout_table_cell_.Location());
  PaintBackground(paint_info, paint_rect, background_object);
}

void TableCellPainter::PaintBackground(const PaintInfo& paint_info,
                                       const LayoutRect& paint_rect,
                                       const LayoutObject& background_object) {
  if (layout_table_cell_.BackgroundStolenForBeingBody())
    return;

  Color c = background_object.ResolveColor(CSSPropertyBackgroundColor);
  const FillLayer& bg_layer = background_object.StyleRef().BackgroundLayers();
  if (bg_layer.HasImage() || c.Alpha()) {
    // We have to clip here because the background would paint
    // on top of the borders otherwise.  This only matters for cells and rows.
    bool should_clip = background_object.HasLayer() &&
                       (background_object == layout_table_cell_ ||
                        background_object == layout_table_cell_.Parent()) &&
                       layout_table_cell_.Table()->ShouldCollapseBorders();
    GraphicsContextStateSaver state_saver(paint_info.context, should_clip);
    if (should_clip) {
      LayoutRect clip_rect(paint_rect.Location(), layout_table_cell_.Size());
      clip_rect.Expand(layout_table_cell_.BorderInsets());
      paint_info.context.Clip(PixelSnappedIntRect(clip_rect));
    }
    BoxPainter(layout_table_cell_)
        .PaintFillLayers(paint_info, c, bg_layer, paint_rect,
                         kBackgroundBleedNone, SkBlendMode::kSrcOver,
                         &background_object);
  }
}

void TableCellPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutTable* table = layout_table_cell_.Table();
  const ComputedStyle& style = layout_table_cell_.StyleRef();
  if (!table->ShouldCollapseBorders() &&
      style.EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  bool needs_to_paint_border =
      style.HasBorderDecoration() && !table->ShouldCollapseBorders();
  if (!style.HasBackground() && !style.BoxShadow() && !needs_to_paint_border)
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_,
          DisplayItem::kBoxDecorationBackground))
    return;

  LayoutRect visual_overflow_rect = layout_table_cell_.VisualOverflowRect();
  visual_overflow_rect.MoveBy(paint_offset);
  // TODO(chrishtr): the pixel-snapping here is likely incorrect.
  LayoutObjectDrawingRecorder recorder(
      paint_info.context, layout_table_cell_,
      DisplayItem::kBoxDecorationBackground,
      PixelSnappedIntRect(visual_overflow_rect));

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);

  BoxPainter::PaintNormalBoxShadow(paint_info, paint_rect, style);
  PaintBackground(paint_info, paint_rect, layout_table_cell_);
  // TODO(wangxianzhu): Calculate the inset shadow bounds by insetting paintRect
  // by half widths of collapsed borders.
  BoxPainter::PaintInsetBoxShadow(paint_info, paint_rect, style);

  if (!needs_to_paint_border)
    return;

  BoxPainter::PaintBorder(layout_table_cell_, layout_table_cell_.GetDocument(),
                          layout_table_cell_.GeneratingNode(), paint_info,
                          paint_rect, style);
}

void TableCellPainter::PaintMask(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  if (layout_table_cell_.Style()->Visibility() != EVisibility::kVisible ||
      paint_info.phase != kPaintPhaseMask)
    return;

  LayoutTable* table_elt = layout_table_cell_.Table();
  if (!table_elt->ShouldCollapseBorders() &&
      layout_table_cell_.Style()->EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_, paint_info.phase))
    return;

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);
  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_table_cell_,
                                       paint_info.phase, paint_rect);
  BoxPainter(layout_table_cell_).PaintMaskImages(paint_info, paint_rect);
}

LayoutRect TableCellPainter::PaintRectNotIncludingVisualOverflow(
    const LayoutPoint& paint_offset) {
  return LayoutRect(paint_offset,
                    LayoutSize(layout_table_cell_.PixelSnappedSize()));
}

}  // namespace blink

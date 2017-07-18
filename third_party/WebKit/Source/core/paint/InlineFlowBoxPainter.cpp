// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/InlineFlowBoxPainter.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxModelObjectPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void InlineFlowBoxPainter::Paint(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset,
                                 const LayoutUnit line_top,
                                 const LayoutUnit line_bottom) {
  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  LayoutRect overflow_rect(
      inline_flow_box_.VisualOverflowRect(line_top, line_bottom));
  inline_flow_box_.FlipForWritingMode(overflow_rect);
  overflow_rect.MoveBy(paint_offset);

  if (!paint_info.GetCullRect().IntersectsCullRect(overflow_rect))
    return;

  if (paint_info.phase == kPaintPhaseMask) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, inline_flow_box_,
            DisplayItem::PaintPhaseToDrawingType(paint_info.phase)))
      return;
    DrawingRecorder recorder(
        paint_info.context, inline_flow_box_,
        DisplayItem::PaintPhaseToDrawingType(paint_info.phase),
        PixelSnappedIntRect(overflow_rect));
    PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_info.phase == kPaintPhaseForeground) {
    // Paint our background, border and box-shadow.
    PaintBoxDecorationBackground(paint_info, paint_offset, overflow_rect);
  }

  // Paint our children.
  PaintInfo child_info(paint_info);
  for (InlineBox* curr = inline_flow_box_.FirstChild(); curr;
       curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsText() ||
        !curr->BoxModelObject().HasSelfPaintingLayer())
      curr->Paint(child_info, paint_offset, line_top, line_bottom);
  }
}

void InlineFlowBoxPainter::PaintFillLayers(const PaintInfo& paint_info,
                                           const Color& c,
                                           const FillLayer& fill_layer,
                                           const LayoutRect& rect,
                                           SkBlendMode op) {
  // FIXME: This should be a for loop or similar. It's a little non-trivial to
  // do so, however, since the layers need to be painted in reverse order.
  if (fill_layer.Next())
    PaintFillLayers(paint_info, c, *fill_layer.Next(), rect, op);
  PaintFillLayer(paint_info, c, fill_layer, rect, op);
}

void InlineFlowBoxPainter::PaintFillLayer(const PaintInfo& paint_info,
                                          const Color& c,
                                          const FillLayer& fill_layer,
                                          const LayoutRect& rect,
                                          SkBlendMode op) {
  LayoutBoxModelObject* box_model = ToLayoutBoxModelObject(
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.BoxModelObject()));
  BackgroundImageGeometry geometry(*box_model);
  StyleImage* img = fill_layer.GetImage();
  bool has_fill_image = img && img->CanRender();
  BoxModelObjectPainter box_model_painter(*box_model);
  if ((!has_fill_image &&
       !inline_flow_box_.GetLineLayoutItem().Style()->HasBorderRadius()) ||
      (!inline_flow_box_.PrevLineBox() && !inline_flow_box_.NextLineBox()) ||
      !inline_flow_box_.Parent()) {
    box_model_painter.PaintFillLayer(paint_info, c, fill_layer, rect,
                                     kBackgroundBleedNone, geometry, op,
                                     &inline_flow_box_, rect.Size());
  } else if (inline_flow_box_.GetLineLayoutItem()
                 .Style()
                 ->BoxDecorationBreak() == EBoxDecorationBreak::kClone) {
    GraphicsContextStateSaver state_saver(paint_info.context);
    paint_info.context.Clip(PixelSnappedIntRect(rect));
    box_model_painter.PaintFillLayer(paint_info, c, fill_layer, rect,
                                     kBackgroundBleedNone, geometry, op,
                                     &inline_flow_box_, rect.Size());
  } else {
    // We have a fill image that spans multiple lines.
    // FIXME: frameSize ought to be the same as rect.size().
    LayoutSize frame_size(inline_flow_box_.Width(), inline_flow_box_.Height());
    LayoutRect image_strip_paint_rect = PaintRectForImageStrip(
        rect.Location(), frame_size,
        inline_flow_box_.GetLineLayoutItem().Style()->Direction());
    GraphicsContextStateSaver state_saver(paint_info.context);
    // TODO(chrishtr): this should likely be pixel-snapped.
    paint_info.context.Clip(PixelSnappedIntRect(rect));
    box_model_painter.PaintFillLayer(
        paint_info, c, fill_layer, image_strip_paint_rect, kBackgroundBleedNone,
        geometry, op, &inline_flow_box_, rect.Size());
  }
}

inline bool InlineFlowBoxPainter::ShouldForceIncludeLogicalEdges() const {
  return (!inline_flow_box_.PrevLineBox() && !inline_flow_box_.NextLineBox()) ||
         !inline_flow_box_.Parent();
}

inline bool InlineFlowBoxPainter::IncludeLogicalLeftEdgeForBoxShadow() const {
  return ShouldForceIncludeLogicalEdges() ||
         inline_flow_box_.IncludeLogicalLeftEdge();
}

inline bool InlineFlowBoxPainter::IncludeLogicalRightEdgeForBoxShadow() const {
  return ShouldForceIncludeLogicalEdges() ||
         inline_flow_box_.IncludeLogicalRightEdge();
}

void InlineFlowBoxPainter::PaintNormalBoxShadow(const PaintInfo& info,
                                                const ComputedStyle& s,
                                                const LayoutRect& paint_rect) {
  BoxPainter::PaintNormalBoxShadow(info, paint_rect, s,
                                   IncludeLogicalLeftEdgeForBoxShadow(),
                                   IncludeLogicalRightEdgeForBoxShadow());
}

void InlineFlowBoxPainter::PaintInsetBoxShadow(const PaintInfo& info,
                                               const ComputedStyle& s,
                                               const LayoutRect& paint_rect) {
  BoxPainter::PaintInsetBoxShadow(info, paint_rect, s,
                                  IncludeLogicalLeftEdgeForBoxShadow(),
                                  IncludeLogicalRightEdgeForBoxShadow());
}

static LayoutRect ClipRectForNinePieceImageStrip(const InlineFlowBox& box,
                                                 const NinePieceImage& image,
                                                 const LayoutRect& paint_rect) {
  LayoutRect clip_rect(paint_rect);
  const ComputedStyle& style = box.GetLineLayoutItem().StyleRef();
  LayoutRectOutsets outsets = style.ImageOutsets(image);
  if (box.IsHorizontal()) {
    clip_rect.SetY(paint_rect.Y() - outsets.Top());
    clip_rect.SetHeight(paint_rect.Height() + outsets.Top() + outsets.Bottom());
    if (box.IncludeLogicalLeftEdge()) {
      clip_rect.SetX(paint_rect.X() - outsets.Left());
      clip_rect.SetWidth(paint_rect.Width() + outsets.Left());
    }
    if (box.IncludeLogicalRightEdge())
      clip_rect.SetWidth(clip_rect.Width() + outsets.Right());
  } else {
    clip_rect.SetX(paint_rect.X() - outsets.Left());
    clip_rect.SetWidth(paint_rect.Width() + outsets.Left() + outsets.Right());
    if (box.IncludeLogicalLeftEdge()) {
      clip_rect.SetY(paint_rect.Y() - outsets.Top());
      clip_rect.SetHeight(paint_rect.Height() + outsets.Top());
    }
    if (box.IncludeLogicalRightEdge())
      clip_rect.SetHeight(clip_rect.Height() + outsets.Bottom());
  }
  return clip_rect;
}

LayoutRect InlineFlowBoxPainter::PaintRectForImageStrip(
    const LayoutPoint& paint_offset,
    const LayoutSize& frame_size,
    TextDirection direction) const {
  // We have a fill/border/mask image that spans multiple lines.
  // We need to adjust the offset by the width of all previous lines.
  // Think of background painting on inlines as though you had one long line, a
  // single continuous strip. Even though that strip has been broken up across
  // multiple lines, you still paint it as though you had one single line. This
  // means each line has to pick up the background where the previous line left
  // off.
  LayoutUnit logical_offset_on_line;
  LayoutUnit total_logical_width;
  if (direction == TextDirection::kLtr) {
    for (const InlineFlowBox* curr = inline_flow_box_.PrevLineBox(); curr;
         curr = curr->PrevLineBox())
      logical_offset_on_line += curr->LogicalWidth();
    total_logical_width = logical_offset_on_line;
    for (const InlineFlowBox* curr = &inline_flow_box_; curr;
         curr = curr->NextLineBox())
      total_logical_width += curr->LogicalWidth();
  } else {
    for (const InlineFlowBox* curr = inline_flow_box_.NextLineBox(); curr;
         curr = curr->NextLineBox())
      logical_offset_on_line += curr->LogicalWidth();
    total_logical_width = logical_offset_on_line;
    for (const InlineFlowBox* curr = &inline_flow_box_; curr;
         curr = curr->PrevLineBox())
      total_logical_width += curr->LogicalWidth();
  }
  LayoutUnit strip_x =
      paint_offset.X() -
      (inline_flow_box_.IsHorizontal() ? logical_offset_on_line : LayoutUnit());
  LayoutUnit strip_y =
      paint_offset.Y() -
      (inline_flow_box_.IsHorizontal() ? LayoutUnit() : logical_offset_on_line);
  LayoutUnit strip_width = inline_flow_box_.IsHorizontal() ? total_logical_width
                                                           : frame_size.Width();
  LayoutUnit strip_height = inline_flow_box_.IsHorizontal()
                                ? frame_size.Height()
                                : total_logical_width;
  return LayoutRect(strip_x, strip_y, strip_width, strip_height);
}

InlineFlowBoxPainter::BorderPaintingType
InlineFlowBoxPainter::GetBorderPaintType(const LayoutRect& adjusted_frame_rect,
                                         IntRect& adjusted_clip_rect) const {
  adjusted_clip_rect = PixelSnappedIntRect(adjusted_frame_rect);
  if (inline_flow_box_.Parent() &&
      inline_flow_box_.GetLineLayoutItem().Style()->HasBorderDecoration()) {
    const NinePieceImage& border_image =
        inline_flow_box_.GetLineLayoutItem().Style()->BorderImage();
    StyleImage* border_image_source = border_image.GetImage();
    bool has_border_image =
        border_image_source && border_image_source->CanRender();
    if (has_border_image && !border_image_source->IsLoaded())
      return kDontPaintBorders;

    // The simple case is where we either have no border image or we are the
    // only box for this object.  In those cases only a single call to draw is
    // required.
    if (!has_border_image ||
        (!inline_flow_box_.PrevLineBox() && !inline_flow_box_.NextLineBox()))
      return kPaintBordersWithoutClip;

    // We have a border image that spans multiple lines.
    adjusted_clip_rect = PixelSnappedIntRect(ClipRectForNinePieceImageStrip(
        inline_flow_box_, border_image, adjusted_frame_rect));
    return kPaintBordersWithClip;
  }
  return kDontPaintBorders;
}

static inline Node* GetNode(const LayoutObject* box_model) {
  Node* node = nullptr;
  for (const LayoutObject* obj = box_model; obj && !node; obj = obj->Parent())
    node = obj->GeneratingNode();
  return node;
}

void InlineFlowBoxPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutRect& cull_rect) {
  DCHECK(paint_info.phase == kPaintPhaseForeground);
  if (inline_flow_box_.GetLineLayoutItem().Style()->Visibility() !=
      EVisibility::kVisible)
    return;

  // You can use p::first-line to specify a background. If so, the root line
  // boxes for a line may actually have to paint a background.
  LayoutObject* inline_flow_box_layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.GetLineLayoutItem());
  const ComputedStyle* style_to_use =
      inline_flow_box_.GetLineLayoutItem().Style(
          inline_flow_box_.IsFirstLineStyle());
  bool should_paint_box_decoration_background;
  if (inline_flow_box_.Parent())
    should_paint_box_decoration_background =
        inline_flow_box_layout_object->HasBoxDecorationBackground();
  else
    should_paint_box_decoration_background =
        inline_flow_box_.IsFirstLineStyle() &&
        style_to_use != inline_flow_box_.GetLineLayoutItem().Style();

  if (!should_paint_box_decoration_background)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, inline_flow_box_,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, inline_flow_box_,
                           DisplayItem::kBoxDecorationBackground,
                           PixelSnappedIntRect(cull_rect));

  LayoutRect frame_rect = FrameRectClampedToLineTopAndBottomIfNeeded();

  // Move x/y to our coordinates.
  LayoutRect local_rect(frame_rect);
  inline_flow_box_.FlipForWritingMode(local_rect);
  LayoutPoint adjusted_paint_offset = paint_offset + local_rect.Location();

  LayoutRect adjusted_frame_rect =
      LayoutRect(adjusted_paint_offset, frame_rect.Size());

  IntRect adjusted_clip_rect;
  BorderPaintingType border_painting_type =
      GetBorderPaintType(adjusted_frame_rect, adjusted_clip_rect);

  // Shadow comes first and is behind the background and border.
  PaintNormalBoxShadow(paint_info, *style_to_use, adjusted_frame_rect);

  Color background_color = inline_flow_box_layout_object->ResolveColor(
      *style_to_use, CSSPropertyBackgroundColor);
  PaintFillLayers(paint_info, background_color,
                  style_to_use->BackgroundLayers(), adjusted_frame_rect);
  PaintInsetBoxShadow(paint_info, *style_to_use, adjusted_frame_rect);

  const LayoutObject* box_model = ToLayoutBoxModelObject(
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.BoxModelObject()));

  switch (border_painting_type) {
    case kDontPaintBorders:
      break;
    case kPaintBordersWithoutClip:
      BoxPainter::PaintBorder(
          *box_model, box_model->GetDocument(), GetNode(box_model), paint_info,
          adjusted_frame_rect,
          inline_flow_box_.GetLineLayoutItem().StyleRef(
              inline_flow_box_.IsFirstLineStyle()),
          kBackgroundBleedNone, inline_flow_box_.IncludeLogicalLeftEdge(),
          inline_flow_box_.IncludeLogicalRightEdge());
      break;
    case kPaintBordersWithClip:
      // FIXME: What the heck do we do with RTL here? The math we're using is
      // obviously not right, but it isn't even clear how this should work at
      // all.
      LayoutRect image_strip_paint_rect = PaintRectForImageStrip(
          adjusted_paint_offset, frame_rect.Size(), TextDirection::kLtr);
      GraphicsContextStateSaver state_saver(paint_info.context);
      paint_info.context.Clip(adjusted_clip_rect);
      BoxPainter::PaintBorder(*box_model, box_model->GetDocument(),
                              GetNode(box_model), paint_info,
                              image_strip_paint_rect,
                              inline_flow_box_.GetLineLayoutItem().StyleRef(
                                  inline_flow_box_.IsFirstLineStyle()));
      break;
  }
}

void InlineFlowBoxPainter::PaintMask(const PaintInfo& paint_info,
                                     const LayoutPoint& paint_offset) {
  if (inline_flow_box_.GetLineLayoutItem().Style()->Visibility() !=
          EVisibility::kVisible ||
      paint_info.phase != kPaintPhaseMask)
    return;

  LayoutRect frame_rect = FrameRectClampedToLineTopAndBottomIfNeeded();

  // Move x/y to our coordinates.
  LayoutRect local_rect(frame_rect);
  inline_flow_box_.FlipForWritingMode(local_rect);
  LayoutPoint adjusted_paint_offset = paint_offset + local_rect.Location();

  const NinePieceImage& mask_nine_piece_image =
      inline_flow_box_.GetLineLayoutItem().Style()->MaskBoxImage();
  StyleImage* mask_box_image =
      inline_flow_box_.GetLineLayoutItem().Style()->MaskBoxImage().GetImage();

  // Figure out if we need to push a transparency layer to render our mask.
  bool push_transparency_layer = false;
  bool flatten_compositing_layers =
      paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers;
  bool mask_blending_applied_by_compositor =
      !flatten_compositing_layers &&
      inline_flow_box_.GetLineLayoutItem().HasLayer() &&
      inline_flow_box_.BoxModelObject()
          .Layer()
          ->MaskBlendingAppliedByCompositor();
  SkBlendMode composite_op = SkBlendMode::kSrcOver;
  if (!mask_blending_applied_by_compositor) {
    if ((mask_box_image && inline_flow_box_.GetLineLayoutItem()
                               .Style()
                               ->MaskLayers()
                               .HasImage()) ||
        inline_flow_box_.GetLineLayoutItem().Style()->MaskLayers().Next()) {
      push_transparency_layer = true;
      paint_info.context.BeginLayer(1.0f, SkBlendMode::kDstIn);
    } else {
      // TODO(fmalita): passing a dst-in xfer mode down to
      // paintFillLayers/paintNinePieceImage seems dangerous: it is only correct
      // if applied atomically (single draw call). While the heuristic above
      // presumably ensures that is the case, this approach seems super fragile.
      // We should investigate dropping this optimization in favour of the more
      // robust layer branch above.
      composite_op = SkBlendMode::kDstIn;
    }
  }

  LayoutRect paint_rect = LayoutRect(adjusted_paint_offset, frame_rect.Size());
  PaintFillLayers(paint_info, Color::kTransparent,
                  inline_flow_box_.GetLineLayoutItem().Style()->MaskLayers(),
                  paint_rect, composite_op);

  bool has_box_image = mask_box_image && mask_box_image->CanRender();
  if (!has_box_image || !mask_box_image->IsLoaded()) {
    if (push_transparency_layer)
      paint_info.context.EndLayer();
    return;  // Don't paint anything while we wait for the image to load.
  }

  LayoutBoxModelObject* box_model = ToLayoutBoxModelObject(
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.BoxModelObject()));
  // The simple case is where we are the only box for this object. In those
  // cases only a single call to draw is required.
  if (!inline_flow_box_.PrevLineBox() && !inline_flow_box_.NextLineBox()) {
    NinePieceImagePainter::Paint(
        paint_info.context, *box_model, box_model->GetDocument(),
        GetNode(box_model), paint_rect,
        inline_flow_box_.GetLineLayoutItem().StyleRef(), mask_nine_piece_image,
        composite_op);
  } else {
    // We have a mask image that spans multiple lines.
    // FIXME: What the heck do we do with RTL here? The math we're using is
    // obviously not right, but it isn't even clear how this should work at all.
    LayoutRect image_strip_paint_rect = PaintRectForImageStrip(
        adjusted_paint_offset, frame_rect.Size(), TextDirection::kLtr);
    FloatRect clip_rect(ClipRectForNinePieceImageStrip(
        inline_flow_box_, mask_nine_piece_image, paint_rect));
    GraphicsContextStateSaver state_saver(paint_info.context);
    // TODO(chrishtr): this should be pixel-snapped.
    paint_info.context.Clip(clip_rect);
    NinePieceImagePainter::Paint(
        paint_info.context, *box_model, box_model->GetDocument(),
        GetNode(box_model), image_strip_paint_rect,
        inline_flow_box_.GetLineLayoutItem().StyleRef(), mask_nine_piece_image,
        composite_op);
  }

  if (push_transparency_layer)
    paint_info.context.EndLayer();
}

// This method should not be needed. See crbug.com/530659.
LayoutRect InlineFlowBoxPainter::FrameRectClampedToLineTopAndBottomIfNeeded()
    const {
  LayoutRect rect(inline_flow_box_.FrameRect());

  bool no_quirks_mode =
      inline_flow_box_.GetLineLayoutItem().GetDocument().InNoQuirksMode();
  if (!no_quirks_mode && !inline_flow_box_.HasTextChildren() &&
      !(inline_flow_box_.DescendantsHaveSameLineHeightAndBaseline() &&
        inline_flow_box_.HasTextDescendants())) {
    const RootInlineBox& root_box = inline_flow_box_.Root();
    LayoutUnit logical_top =
        inline_flow_box_.IsHorizontal() ? rect.Y() : rect.X();
    LayoutUnit logical_height =
        inline_flow_box_.IsHorizontal() ? rect.Height() : rect.Width();
    LayoutUnit bottom =
        std::min(root_box.LineBottom(), logical_top + logical_height);
    logical_top = std::max(root_box.LineTop(), logical_top);
    logical_height = bottom - logical_top;
    if (inline_flow_box_.IsHorizontal()) {
      rect.SetY(logical_top);
      rect.SetHeight(logical_height);
    } else {
      rect.SetX(logical_top);
      rect.SetWidth(logical_height);
    }
  }
  return rect;
}

}  // namespace blink

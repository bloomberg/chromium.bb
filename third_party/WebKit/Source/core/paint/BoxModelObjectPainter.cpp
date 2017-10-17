// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxModelObjectPainter.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

namespace {

Node* GeneratingNodeForObject(const LayoutBoxModelObject& box_model) {
  Node* node = nullptr;
  const LayoutObject* layout_object = &box_model;
  for (; layout_object && !node; layout_object = layout_object->Parent()) {
    node = layout_object->GeneratingNode();
  }
  return node;
}

}  // anonymous namespace

BoxModelObjectPainter::BoxModelObjectPainter(const LayoutBoxModelObject& box,
                                             const InlineFlowBox* flow_box,
                                             const LayoutSize& flow_box_size)
    : BoxPainterBase(box,
                     &box.GetDocument(),
                     box.StyleRef(),
                     GeneratingNodeForObject(box),
                     box.BorderBoxOutsets(),
                     box.PaddingOutsets()),
      box_model_(box),
      flow_box_(flow_box),
      flow_box_size_(flow_box_size) {}

bool BoxModelObjectPainter::
    IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
        const LayoutBoxModelObject* box_model_,
        const PaintInfo& paint_info) {
  return paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents &&
         !(paint_info.PaintFlags() &
           kPaintLayerPaintingCompositingBackgroundPhase) &&
         box_model_ == paint_info.PaintContainer();
}

void BoxModelObjectPainter::PaintFillLayerTextFillBox(
    GraphicsContext& context,
    const BoxPainterBase::FillLayerInfo& info,
    Image* image,
    SkBlendMode composite_op,
    const BackgroundImageGeometry& geometry,
    const LayoutRect& rect,
    LayoutRect scrolled_paint_rect) {
  // First figure out how big the mask has to be. It should be no bigger
  // than what we need to actually render, so we should intersect the dirty
  // rect with the border box of the background.
  IntRect mask_rect = PixelSnappedIntRect(rect);

  // We draw the background into a separate layer, to be later masked with
  // yet another layer holding the text content.
  GraphicsContextStateSaver background_clip_state_saver(context, false);
  background_clip_state_saver.Save();
  context.Clip(mask_rect);
  context.BeginLayer();

  PaintFillLayerBackground(context, info, image, composite_op, geometry,
                           scrolled_paint_rect);

  // Create the text mask layer and draw the text into the mask. We do this by
  // painting using a special paint phase that signals to InlineTextBoxes that
  // they should just add their contents to the clip.
  context.BeginLayer(1, SkBlendMode::kDstIn);
  PaintInfo paint_info(context, mask_rect, PaintPhase::kTextClip,
                       kGlobalPaintNormalPhase, 0);
  if (flow_box_) {
    const RootInlineBox& root = flow_box_->Root();
    flow_box_->Paint(paint_info,
                     LayoutPoint(scrolled_paint_rect.X() - flow_box_->X(),
                                 scrolled_paint_rect.Y() - flow_box_->Y()),
                     root.LineTop(), root.LineBottom());
  } else {
    // FIXME: this should only have an effect for the line box list within
    // |box_model_|. Change this to create a LineBoxListPainter directly.
    LayoutSize local_offset = box_model_.IsBox()
                                  ? ToLayoutBox(&box_model_)->LocationOffset()
                                  : LayoutSize();
    box_model_.Paint(paint_info, scrolled_paint_rect.Location() - local_offset);
  }

  context.EndLayer();  // Text mask layer.
  context.EndLayer();  // Background layer.
}

FloatRoundedRect BoxModelObjectPainter::GetBackgroundRoundedRect(
    const LayoutRect& border_rect,
    bool include_logical_left_edge,
    bool include_logical_right_edge) const {
  FloatRoundedRect border = BoxPainterBase::GetBackgroundRoundedRect(
      border_rect, include_logical_left_edge, include_logical_right_edge);
  if (flow_box_ && (flow_box_->NextLineBox() || flow_box_->PrevLineBox())) {
    FloatRoundedRect segment_border = box_model_.StyleRef().GetRoundedBorderFor(
        LayoutRect(LayoutPoint(), LayoutSize(FlooredIntSize(flow_box_size_))),
        include_logical_left_edge, include_logical_right_edge);
    border.SetRadii(segment_border.GetRadii());
  }
  return border;
}

LayoutRect BoxModelObjectPainter::AdjustForScrolledContent(
    const PaintInfo& paint_info,
    const BoxPainterBase::FillLayerInfo& info,
    const LayoutRect& rect) {
  LayoutRect scrolled_paint_rect = rect;
  GraphicsContext& context = paint_info.context;
  if (info.is_clipped_with_local_scrolling &&
      !IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &box_model_, paint_info)) {
    // Clip to the overflow area.
    const LayoutBox& this_box = ToLayoutBox(box_model_);
    // TODO(chrishtr): this should be pixel-snapped.
    context.Clip(FloatRect(this_box.OverflowClipRect(rect.Location())));

    // Adjust the paint rect to reflect a scrolled content box with borders at
    // the ends.
    IntSize offset = this_box.ScrolledContentOffset();
    scrolled_paint_rect.Move(-offset);
    LayoutRectOutsets border = BorderOutsets(info);
    scrolled_paint_rect.SetWidth(border.Left() + this_box.ScrollWidth() +
                                 border.Right());
    scrolled_paint_rect.SetHeight(this_box.BorderTop() +
                                  this_box.ScrollHeight() +
                                  this_box.BorderBottom());
  }
  return scrolled_paint_rect;
}

BoxPainterBase::FillLayerInfo BoxModelObjectPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance) const {
  return BoxPainterBase::FillLayerInfo(
      box_model_.GetDocument(), box_model_.StyleRef(),
      box_model_.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      (flow_box_ ? flow_box_->IncludeLogicalLeftEdge() : true),
      (flow_box_ ? flow_box_->IncludeLogicalRightEdge() : true));
}

}  // namespace blink

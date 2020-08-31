// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

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

LayoutSize LogicalOffsetOnLine(const InlineFlowBox& flow_box) {
  // Compute the offset of the passed flow box when seen as part of an
  // unbroken continuous strip (c.f box-decoration-break: slice.)
  LayoutUnit logical_offset_on_line;
  if (flow_box.IsLeftToRightDirection()) {
    for (const InlineFlowBox* curr = flow_box.PrevForSameLayoutObject(); curr;
         curr = curr->PrevForSameLayoutObject())
      logical_offset_on_line += curr->LogicalWidth();
  } else {
    for (const InlineFlowBox* curr = flow_box.NextForSameLayoutObject(); curr;
         curr = curr->NextForSameLayoutObject())
      logical_offset_on_line += curr->LogicalWidth();
  }
  LayoutSize logical_offset(logical_offset_on_line, LayoutUnit());
  return flow_box.IsHorizontal() ? logical_offset
                                 : logical_offset.TransposedSize();
}

}  // anonymous namespace

BoxModelObjectPainter::BoxModelObjectPainter(const LayoutBoxModelObject& box,
                                             const InlineFlowBox* flow_box)
    : BoxPainterBase(&box.GetDocument(),
                     box.StyleRef(),
                     GeneratingNodeForObject(box)),
      box_model_(box),
      flow_box_(flow_box) {}

void BoxModelObjectPainter::PaintTextClipMask(
    GraphicsContext& context,
    const IntRect& mask_rect,
    const PhysicalOffset& paint_offset,
    bool object_has_multiple_boxes) {
  PaintInfo paint_info(context, mask_rect, PaintPhase::kTextClip,
                       kGlobalPaintNormalPhase, 0);
  if (flow_box_) {
    LayoutSize local_offset = ToLayoutSize(flow_box_->Location());
    if (object_has_multiple_boxes &&
        box_model_.StyleRef().BoxDecorationBreak() ==
            EBoxDecorationBreak::kSlice) {
      local_offset -= LogicalOffsetOnLine(*flow_box_);
    }
    const RootInlineBox& root = flow_box_->Root();
    flow_box_->Paint(paint_info, paint_offset.ToLayoutPoint() - local_offset,
                     root.LineTop(), root.LineBottom());
  } else if (auto* layout_block = DynamicTo<LayoutBlock>(box_model_)) {
    layout_block->PaintObject(paint_info, paint_offset);
  } else {
    // We should go through the above path for LayoutInlines.
    DCHECK(!box_model_.IsLayoutInline());
    // Other types of objects don't have anything meaningful to paint for text
    // clip mask.
  }
}

PhysicalRect BoxModelObjectPainter::AdjustRectForScrolledContent(
    const PaintInfo& paint_info,
    const BoxPainterBase::FillLayerInfo& info,
    const PhysicalRect& rect) {
  if (!info.is_clipped_with_local_scrolling)
    return rect;

  const auto& this_box = ToLayoutBox(box_model_);
  if (BoxDecorationData::IsPaintingScrollingBackground(paint_info, this_box))
    return rect;

  PhysicalRect scrolled_paint_rect = rect;
  GraphicsContext& context = paint_info.context;
  // Clip to the overflow area.
  // TODO(chrishtr): this should be pixel-snapped.
  context.Clip(FloatRect(this_box.OverflowClipRect(rect.offset)));

  // Adjust the paint rect to reflect a scrolled content box with borders at
  // the ends.
  PhysicalOffset offset(this_box.PixelSnappedScrolledContentOffset());
  scrolled_paint_rect.Move(-offset);
  LayoutRectOutsets border = AdjustedBorderOutsets(info);
  scrolled_paint_rect.SetWidth(border.Left() + this_box.ScrollWidth() +
                               border.Right());
  scrolled_paint_rect.SetHeight(this_box.BorderTop() + this_box.ScrollHeight() +
                                this_box.BorderBottom());
  return scrolled_paint_rect;
}

LayoutRectOutsets BoxModelObjectPainter::ComputeBorders() const {
  return box_model_.BorderBoxOutsets();
}

LayoutRectOutsets BoxModelObjectPainter::ComputePadding() const {
  return box_model_.PaddingOutsets();
}

BoxPainterBase::FillLayerInfo BoxModelObjectPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_scrolling_background) const {
  return BoxPainterBase::FillLayerInfo(
      box_model_.GetDocument(), box_model_.StyleRef(),
      box_model_.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      LayoutObject::ShouldRespectImageOrientation(&box_model_),
      (flow_box_ ? flow_box_->IncludeLogicalLeftEdge() : true),
      (flow_box_ ? flow_box_->IncludeLogicalRightEdge() : true),
      box_model_.IsLayoutInline(), is_painting_scrolling_background);
}

bool BoxModelObjectPainter::IsPaintingScrollingBackground(
    const PaintInfo& paint_info) const {
  if (!box_model_.IsBox())
    return false;

  const auto& this_box = ToLayoutBox(box_model_);
  return BoxDecorationData::IsPaintingScrollingBackground(paint_info, this_box);
}

}  // namespace blink

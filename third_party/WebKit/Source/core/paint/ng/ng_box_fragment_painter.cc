// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_box_fragment_painter.h"

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ng/ng_text_fragment_painter.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

namespace {
LayoutRectOutsets BoxStrutToLayoutRectOutsets(
    const NGPixelSnappedPhysicalBoxStrut& box_strut) {
  return LayoutRectOutsets(
      LayoutUnit(box_strut.top), LayoutUnit(box_strut.right),
      LayoutUnit(box_strut.bottom), LayoutUnit(box_strut.left));
}
}  // anonymous namespace

NGBoxFragmentPainter::NGBoxFragmentPainter(const NGPhysicalBoxFragment& box)
    : BoxPainterBase(box,
                     &box.GetLayoutObject()->GetDocument(),
                     box.Style(),
                     box.GetLayoutObject()->GeneratingNode(),
                     BoxStrutToLayoutRectOutsets(box.BorderWidths()),
                     LayoutRectOutsets()),
      box_fragment_(box) {}

void NGBoxFragmentPainter::Paint(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  // TODO(layout-dev): This results in double painting of decorations (once for
  // box itself and once for the anonymous fragment). We should either get rid
  // of the anonymous fragment or unset the style for it.
  if (paint_info.phase == kPaintPhaseForeground)
    PaintBoxDecorationBackground(paint_info, paint_offset);
  PaintChildren(box_fragment_.Children(), paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintChildren(const PaintInfo& paint_info,
                                         const LayoutPoint& paint_offset) {
  PaintChildren(box_fragment_.Children(), paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutRect paint_rect;
  if (!IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          box_fragment_, paint_info)) {
    // TODO(eae): We need better converters for ng geometry types. Long term we
    // probably want to change the paint code to take NGPhysical* but that is a
    // much bigger change.
    NGPhysicalSize size = box_fragment_.Size();
    paint_rect = LayoutRect(LayoutPoint(), LayoutSize(size.width, size.height));
  }

  paint_rect.MoveBy(paint_offset);
  PaintBoxDecorationBackgroundWithRect(paint_info, paint_offset, paint_rect);
}

void NGBoxFragmentPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutRect& paint_rect) {
  bool painting_overflow_contents =
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          box_fragment_, paint_info);
  const ComputedStyle& style = box_fragment_.Style();

  // TODO(layout-dev): Implement support for painting overflow contents.
  const DisplayItemClient& display_item_client = box_fragment_;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(
      paint_info.context, display_item_client,
      DisplayItem::kBoxDecorationBackground,
      FloatRect(BoundsForDrawingRecorder(paint_info, paint_offset)));
  BoxDecorationData box_decoration_data(box_fragment_);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (!painting_overflow_contents) {
    // FIXME: Should eventually give the theme control over whether the box
    // shadow should paint, since controls could have custom shadows of their
    // own.
    PaintNormalBoxShadow(paint_info, paint_rect, style);

    if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(paint_rect);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
        paint_info.context.BeginLayer();
    }
  }

  // TODO(layout-dev): Support theme painting.

  // TODO(eae): Support SkipRootBackground painting.
  bool should_paint_background = true;
  if (should_paint_background) {
    PaintBackground(paint_info, paint_rect,
                    box_decoration_data.background_color,
                    box_decoration_data.bleed_avoidance);
  }

  if (!painting_overflow_contents) {
    PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect, style);

    if (box_decoration_data.has_border_decoration) {
      Node* generating_node = box_fragment_.GetLayoutObject()->GeneratingNode();
      const Document& document = box_fragment_.GetLayoutObject()->GetDocument();
      PaintBorder(box_fragment_, document, generating_node, paint_info,
                  paint_rect, style, box_decoration_data.bleed_avoidance);
    }
  }

  if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
    paint_info.context.EndLayer();
}

void NGBoxFragmentPainter::PaintChildren(
    const Vector<RefPtr<NGPhysicalFragment>>& children,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  PaintInfo child_info(paint_info);

  for (const auto& child : children) {
    LayoutPoint child_offset =
        paint_offset + LayoutSize(child->Offset().left, child->Offset().top);
    if (child->Type() == NGPhysicalBoxFragment::kFragmentLineBox) {
      const NGPhysicalLineBoxFragment& line_box_fragment =
          ToNGPhysicalLineBoxFragment(*child.Get());
      PaintChildren(line_box_fragment.Children(), child_info, paint_offset);
    } else if (child->Type() == NGPhysicalBoxFragment::kFragmentBox) {
      const NGPhysicalBoxFragment& box_fragment =
          ToNGPhysicalBoxFragment(*child.Get());
      PaintInfo child_paint_info(paint_info);
      NGBoxFragmentPainter(box_fragment).Paint(child_paint_info, child_offset);

      // TODO(layout-dev): Implement support for this.
    } else if (child->Type() == NGPhysicalBoxFragment::kFragmentText) {
      PaintText(ToNGPhysicalTextFragment(*child.Get()), paint_info,
                paint_offset);
    }
  }
}

void NGBoxFragmentPainter::PaintText(
    const NGPhysicalTextFragment& text_fragment,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutRect overflow_rect(box_fragment_.VisualOverflowRect());
  overflow_rect.MoveBy(paint_offset);
  DrawingRecorder recorder(
      paint_info.context, text_fragment,
      DisplayItem::PaintPhaseToDrawingType(paint_info.phase),
      PixelSnappedIntRect(overflow_rect));

  const Document& document = box_fragment_.GetLayoutObject()->GetDocument();
  NGTextFragmentPainter text_painter(text_fragment);
  text_painter.Paint(document, paint_info, paint_offset);
}

bool NGBoxFragmentPainter::
    IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
        const NGPhysicalFragment& fragment,
        const PaintInfo& paint_info) {
  // TODO(layout-dev): Implement once we have support for scrolling.
  return false;
}

LayoutRect NGBoxFragmentPainter::BoundsForDrawingRecorder(
    const PaintInfo& paint_info,
    const LayoutPoint& adjusted_paint_offset) {
  // TODO(layout-dev): This should be layout overflow, not visual.
  LayoutRect bounds = box_fragment_.VisualOverflowRect();
  bounds.MoveBy(adjusted_paint_offset);
  return bounds;
}

void NGBoxFragmentPainter::PaintFillLayerTextFillBox(
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
  PaintInfo paint_info(context, mask_rect, kPaintPhaseTextClip,
                       kGlobalPaintNormalPhase, 0);

  // TODO(eae): Paint text child fragments.

  context.EndLayer();  // Text mask layer.
  context.EndLayer();  // Background layer.
}

LayoutRect NGBoxFragmentPainter::AdjustForScrolledContent(
    const PaintInfo&,
    const BoxPainterBase::FillLayerInfo&,
    const LayoutRect& rect) {
  return rect;
}

BoxPainterBase::FillLayerInfo NGBoxFragmentPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance) const {
  return BoxPainterBase::FillLayerInfo(
      box_fragment_.GetLayoutObject()->GetDocument(), box_fragment_.Style(),
      box_fragment_.HasOverflowClip(), color, bg_layer, bleed_avoidance, true,
      true);
}

void NGBoxFragmentPainter::PaintBackground(
    const PaintInfo& paint_info,
    const LayoutRect& paint_rect,
    const Color& background_color,
    BackgroundBleedAvoidance bleed_avoidance) {
  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      box_fragment_.GetLayoutObject()));
  PaintFillLayers(paint_info, background_color,
                  box_fragment_.Style().BackgroundLayers(), paint_rect,
                  geometry, bleed_avoidance);
}

}  // namespace blink

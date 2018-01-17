// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_box_fragment_painter.h"

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/HitTestLocation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/ng/geometry/ng_border_edges.h"
#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/layout_ng_mixin.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/AdjustPaintOffsetScope.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintPhase.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/ng/ng_box_clipper.h"
#include "core/paint/ng/ng_fragment_painter.h"
#include "core/paint/ng/ng_paint_fragment.h"
#include "core/paint/ng/ng_text_fragment_painter.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

namespace {
LayoutRectOutsets BoxStrutToLayoutRectOutsets(
    const NGPixelSnappedPhysicalBoxStrut& box_strut) {
  return LayoutRectOutsets(
      LayoutUnit(box_strut.top), LayoutUnit(box_strut.right),
      LayoutUnit(box_strut.bottom), LayoutUnit(box_strut.left));
}
}  // anonymous namespace

NGBoxFragmentPainter::NGBoxFragmentPainter(const NGPaintFragment& box)
    : BoxPainterBase(
          box,
          &box.GetLayoutObject()->GetDocument(),
          box.Style(),
          box.GetLayoutObject()->GeneratingNode(),
          BoxStrutToLayoutRectOutsets(box.PhysicalFragment().BorderWidths()),
          LayoutRectOutsets()),
      box_fragment_(box),
      border_edges_(
          NGBorderEdges::FromPhysical(box.PhysicalFragment().BorderEdges(),
                                      box.Style().GetWritingMode())),
      is_inline_(false) {
  DCHECK(box.PhysicalFragment().IsBox());
}

void NGBoxFragmentPainter::Paint(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  const NGPhysicalFragment& fragment = box_fragment_.PhysicalFragment();
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  DCHECK(layout_object && layout_object->IsBox());
  if (!fragment.IsPlacedByLayoutNG()) {
    // |fragment.Offset()| is valid only when it is placed by LayoutNG parent.
    // Use LayoutBox::Location() if not. crbug.com/788590
    AdjustPaintOffsetScope adjustment(ToLayoutBox(*layout_object), paint_info,
                                      paint_offset);
    PaintWithAdjustedOffset(adjustment.MutablePaintInfo(),
                            adjustment.AdjustedPaintOffset());
    return;
  }

  AdjustPaintOffsetScope adjustment(box_fragment_, paint_info, paint_offset);
  PaintWithAdjustedOffset(adjustment.MutablePaintInfo(),
                          adjustment.AdjustedPaintOffset());
}

void NGBoxFragmentPainter::PaintInlineBox(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutPoint& block_paint_offset) {
  DCHECK(box_fragment_.GetLayoutObject() &&
         box_fragment_.GetLayoutObject()->IsLayoutInline());
  is_inline_ = true;
  block_paint_offset_ = block_paint_offset;
  PaintInfo info(paint_info);
  PaintWithAdjustedOffset(
      info, paint_offset + box_fragment_.Offset().ToLayoutPoint());
}

void NGBoxFragmentPainter::PaintWithAdjustedOffset(
    PaintInfo& info,
    const LayoutPoint& paint_offset) {
  if (!IntersectsPaintRect(info, paint_offset))
    return;

  if (box_fragment_.PhysicalFragment().IsInlineBlock())
    return PaintInlineBlock(info, paint_offset);

  PaintPhase original_phase = info.phase;

  if (original_phase == PaintPhase::kOutline) {
    info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    PaintObject(info, paint_offset);
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly) {
    NGBoxClipper box_clipper(box_fragment_, info);
    PaintObject(info, paint_offset);
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    info.phase = PaintPhase::kSelfOutlineOnly;
    PaintObject(info, paint_offset);
  }

  // Our scrollbar widgets paint exactly when we tell them to, so that they work
  // properly with z-index. We paint after we painted the background/border, so
  // that the scrollbars will sit above the background/border.
  info.phase = original_phase;
  PaintOverflowControlsIfNeeded(info, paint_offset);
}

void NGBoxFragmentPainter::PaintObject(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  const PaintPhase paint_phase = paint_info.phase;
  const ComputedStyle& style = box_fragment_.Style();
  bool is_visible = style.Visibility() == EVisibility::kVisible;

  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    // TODO(eae): style.HasBoxDecorationBackground isn't good enough, it needs
    // to check the object as some objects may have box decoration background
    // other than from their own style.
    // PaintBoxDecorationBackground should be called here but is currently
    // called during the foreground phase instead for all box types, not just
    // for inline flow boxes.
    // The paint phase and inline/block box distinction needs cleanup, but
    // without this, borders on 'overflow: scroll' are clipped.
    if (is_visible && style.HasBoxDecorationBackground() && !is_inline_)
      PaintBoxDecorationBackground(paint_info, paint_offset);

    // Record the scroll hit test after the background so background squashing
    // is not affected. Hit test order would be equivalent if this were
    // immediately before the background.
    // if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    //  PaintScrollHitTestDisplayItem(paint_info);

    // We're done. We don't bother painting any children.
    if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_info.PaintRootBackgroundOnly())
    return;

  if (paint_phase == PaintPhase::kMask && is_visible)
    return PaintMask(paint_info, paint_offset);

  if (paint_phase == PaintPhase::kClippingMask && is_visible)
    return PaintClippingMask(paint_info, paint_offset);

  // TODO(eae): Add PDF URL painting for printing.
  // if (paint_phase == PaintPhase::kForeground && paint_info.IsPrinting())
  //  ObjectPainter(box_fragment_)
  //      .AddPDFURLRectIfNeeded(paint_info, paint_offset);

  // TODO(layout-dev): Add support for scrolling.
  Optional<PaintInfo> scrolled_paint_info;

  const PaintInfo& contents_paint_info =
      scrolled_paint_info ? *scrolled_paint_info : paint_info;

  // Paint our background, border and box-shadow.
  // TODO(eae): We should only paint box-decorations during the foreground phase
  // for inline boxes. Split this method into PaintBlock and PaintInline once we
  // can tell the two types of fragments apart or we've eliminated the extra
  // block wrapper fragments.
  if (paint_info.phase == PaintPhase::kForeground && is_inline_)
    PaintBoxDecorationBackground(paint_info, paint_offset);

  PaintContents(contents_paint_info, paint_offset);

  if (paint_phase == PaintPhase::kFloat ||
      paint_phase == PaintPhase::kSelection ||
      paint_phase == PaintPhase::kTextClip)
    PaintFloats(contents_paint_info, paint_offset);

  if (ShouldPaintSelfOutline(paint_phase))
    NGFragmentPainter(box_fragment_).PaintOutline(paint_info, paint_offset);

  // TODO(layout-dev): Implement once we have selections in LayoutNG.
  // If the caret's node's layout object's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  // if (paint_phase == PaintPhase::kForeground &&
  //     box_fragment_.ShouldPaintCarets())
  //  PaintCarets(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintInlineObject(const PaintInfo& paint_info,
                                             const LayoutPoint& paint_offset) {
  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  LayoutRect overflow_rect(box_fragment_.VisualOverflowRect());
  overflow_rect.MoveBy(paint_offset);

  if (!paint_info.GetCullRect().IntersectsCullRect(overflow_rect))
    return;

  if (paint_info.phase == PaintPhase::kMask) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, box_fragment_,
            DisplayItem::PaintPhaseToDrawingType(paint_info.phase)))
      return;
    DrawingRecorder recorder(
        paint_info.context, box_fragment_,
        DisplayItem::PaintPhaseToDrawingType(paint_info.phase));
    PaintMask(paint_info, paint_offset);
    return;
  }

  // Paint our background, border and box-shadow.
  if (paint_info.phase == PaintPhase::kForeground)
    PaintBoxDecorationBackground(paint_info, paint_offset);

  // Paint our children.
  PaintContents(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintContents(const PaintInfo& paint_info,
                                         const LayoutPoint& paint_offset) {
  PaintInfo descendants_info = paint_info.ForDescendants();
  if (is_inline_) {
    PaintInlineChildren(box_fragment_.Children(), descendants_info,
                        paint_offset);
    return;
  }
  PaintChildren(box_fragment_.Children(), descendants_info, paint_offset);
}

void NGBoxFragmentPainter::PaintFloats(const PaintInfo&, const LayoutPoint&) {
  // TODO(eae): Implement once we have a way to distinguish float fragments.
}

void NGBoxFragmentPainter::PaintMask(const PaintInfo&, const LayoutPoint&) {
  // TODO(eae): Implement.
}

void NGBoxFragmentPainter::PaintClippingMask(const PaintInfo&,
                                             const LayoutPoint&) {
  // TODO(eae): Implement.
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

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground);
  BoxDecorationData box_decoration_data(box_fragment_.PhysicalFragment());
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (!painting_overflow_contents) {
    PaintNormalBoxShadow(paint_info, paint_rect, style);

    if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(
          paint_rect, border_edges_.line_left, border_edges_.line_right);
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
                  paint_rect, style, box_decoration_data.bleed_avoidance,
                  border_edges_.line_left, border_edges_.line_right);
    }
  }

  if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
    paint_info.context.EndLayer();
}

static bool RequiresLegacyFallback(const NGPhysicalFragment& fragment) {
  // Fallback to LayoutObject if this is a root of NG block layout.
  // If this box is for this painter, LayoutNGBlockFlow will call back.
  if (fragment.IsBlockLayoutRoot())
    return true;

  // TODO(kojii): Review if this is still needed.
  LayoutObject* layout_object = fragment.GetLayoutObject();
  return layout_object->IsLayoutReplaced();
}

void NGBoxFragmentPainter::PaintInlineChildBoxUsingLegacyFallback(
    const NGPhysicalFragment& fragment,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutObject* layout_object = fragment.GetLayoutObject();
  DCHECK(layout_object);
  if (layout_object->IsLayoutNGMixin() &&
      ToLayoutBlockFlow(layout_object)->PaintFragment()) {
    // This object will use NGBoxFragmentPainter. NGBoxFragmentPainter expects
    // |paint_offset| relative to the parent, even when in inline context.
    layout_object->Paint(paint_info, paint_offset);
    return;
  }

  // When in inline context, pre-NG painters expect |paint_offset| of their
  // block container.
  if (layout_object->IsAtomicInlineLevel()) {
    // Pre-NG painters also expect callers to use |PaintAllPhasesAtomically()|
    // for atomic inlines.
    ObjectPainter(*layout_object)
        .PaintAllPhasesAtomically(paint_info, block_paint_offset_);
    return;
  }

  layout_object->Paint(paint_info, block_paint_offset_);
}

void NGBoxFragmentPainter::PaintAllPhasesAtomically(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  // Pass PaintPhaseSelection and PaintPhaseTextClip is handled by the regular
  // foreground paint implementation. We don't need complete painting for these
  // phases.
  PaintPhase phase = paint_info.phase;
  if (phase == PaintPhase::kSelection || phase == PaintPhase::kTextClip)
    return PaintObject(paint_info, paint_offset);

  if (phase != PaintPhase::kForeground)
    return;

  PaintInfo info(paint_info);
  info.phase = PaintPhase::kBlockBackground;
  PaintObject(info, paint_offset);

  info.phase = PaintPhase::kFloat;
  PaintObject(info, paint_offset);

  info.phase = PaintPhase::kForeground;
  PaintObject(info, paint_offset);

  info.phase = PaintPhase::kOutline;
  PaintObject(info, paint_offset);
}

void NGBoxFragmentPainter::PaintChildren(
    const Vector<std::unique_ptr<NGPaintFragment>>& children,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  for (const auto& child : children) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (child->HasSelfPaintingLayer())
        continue;
      if (RequiresLegacyFallback(fragment))
        fragment.GetLayoutObject()->Paint(paint_info, paint_offset);
      else
        NGBoxFragmentPainter(*child).Paint(paint_info, paint_offset);
    } else if (fragment.Type() == NGPhysicalFragment::kFragmentLineBox) {
      PaintLineBox(*child, paint_info, paint_offset);
    }
  }
}

void NGBoxFragmentPainter::PaintInlineChildren(
    const Vector<std::unique_ptr<NGPaintFragment>>& children,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  for (const auto& child : children) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (fragment.Type() == NGPhysicalFragment::kFragmentText) {
      PaintText(*child, paint_info, paint_offset);
    } else if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (child->HasSelfPaintingLayer())
        continue;
      if (RequiresLegacyFallback(fragment)) {
        PaintInlineChildBoxUsingLegacyFallback(fragment, paint_info,
                                               paint_offset);
      } else {
        NGBoxFragmentPainter(*child).PaintInlineBox(paint_info, paint_offset,
                                                    block_paint_offset_);
      }
    }
  }
}

void NGBoxFragmentPainter::PaintLineBox(
    const NGPaintFragment& line_box_fragment,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  // Line box fragments don't have LayoutObject and nothing to paint. Accumulate
  // its offset and paint children.
  block_paint_offset_ = paint_offset;
  PaintInlineChildren(
      line_box_fragment.Children(), paint_info,
      paint_offset + line_box_fragment.Offset().ToLayoutPoint());
}

void NGBoxFragmentPainter::PaintInlineBlock(const PaintInfo& paint_info,
                                            const LayoutPoint& paint_offset) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection)
    return;

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  DCHECK(paint_info.phase != PaintPhase::kTextClip);

  PaintAllPhasesAtomically(paint_info, paint_offset);
}

void NGBoxFragmentPainter::PaintText(const NGPaintFragment& text_fragment,
                                     const PaintInfo& paint_info,
                                     const LayoutPoint& paint_offset) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, text_fragment,
          DisplayItem::PaintPhaseToDrawingType(paint_info.phase)))
    return;

  DrawingRecorder recorder(
      paint_info.context, text_fragment,
      DisplayItem::PaintPhaseToDrawingType(paint_info.phase));

  NGTextFragmentPainter text_painter(text_fragment);
  text_painter.Paint(paint_info, paint_offset);
}

bool NGBoxFragmentPainter::
    IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
        const NGPaintFragment& fragment,
        const PaintInfo& paint_info) {
  // TODO(layout-dev): Implement once we have support for scrolling.
  return false;
}

// Clone of BlockPainter::PaintOverflowControlsIfNeeded
void NGBoxFragmentPainter::PaintOverflowControlsIfNeeded(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (box_fragment_.HasOverflowClip() &&
      box_fragment_.Style().Visibility() == EVisibility::kVisible &&
      ShouldPaintSelfBlockBackground(paint_info.phase) &&
      !paint_info.PaintRootBackgroundOnly()) {
    LayoutObject* layout_object =
        box_fragment_.PhysicalFragment().GetLayoutObject();
    if (layout_object->IsLayoutBlock()) {
      LayoutBlock* layout_block = ToLayoutBlock(layout_object);
      Optional<ClipRecorder> clip_recorder;
      if (!layout_block->Layer()->IsSelfPaintingLayer()) {
        LayoutRect clip_rect = layout_block->BorderBoxRect();
        clip_rect.MoveBy(paint_offset);
        clip_recorder.emplace(paint_info.context, *layout_block,
                              DisplayItem::kClipScrollbarsToBoxBounds,
                              PixelSnappedIntRect(clip_rect));
      }
      ScrollableAreaPainter(*layout_block->Layer()->GetScrollableArea())
          .PaintOverflowControls(
              paint_info.context, RoundedIntPoint(paint_offset),
              paint_info.GetCullRect(), false /* paintingOverlayControls */);
    }
  }
}

bool NGBoxFragmentPainter::IntersectsPaintRect(
    const PaintInfo& paint_info,
    const LayoutPoint& adjusted_paint_offset) const {
  // TODO(layout-dev): Add support for scrolling, see
  // BlockPainter::IntersectsPaintRect.
  LayoutRect overflow_rect(box_fragment_.VisualOverflowRect());
  overflow_rect.MoveBy(adjusted_paint_offset);
  return paint_info.GetCullRect().IntersectsCullRect(overflow_rect);
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
  PaintInfo paint_info(context, mask_rect, PaintPhase::kTextClip,
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
      box_fragment_.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      border_edges_.line_left, border_edges_.line_right);
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

bool NGBoxFragmentPainter::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  // TODO(eae): Switch to using NG geometry types.
  LayoutSize offset(box_fragment_.Offset().left, box_fragment_.Offset().top);
  LayoutPoint adjusted_location = accumulated_offset + offset;
  LayoutSize size(box_fragment_.Size().width, box_fragment_.Size().height);
  const ComputedStyle& style = box_fragment_.Style();

  bool hit_test_self = action == kHitTestForeground;

  // TODO(layout-dev): Add support for hit testing overflow controls once we
  // overflow has been implemented.
  // if (hit_test_self && HasOverflowClip() &&
  //   HitTestOverflowControl(result, location_in_container, adjusted_location))
  // return true;

  bool skip_children = false;
  if (box_fragment_.ShouldClipOverflow()) {
    // PaintLayer::HitTestContentsForFragments checked the fragments'
    // foreground rect for intersection if a layer is self painting,
    // so only do the overflow clip check here for non-self-painting layers.
    if (!box_fragment_.HasSelfPaintingLayer() &&
        !location_in_container.Intersects(box_fragment_.OverflowClipRect(
            adjusted_location, kExcludeOverlayScrollbarSizeForHitTesting))) {
      skip_children = true;
    }
    if (!skip_children && style.HasBorderRadius()) {
      LayoutRect bounds_rect(adjusted_location, size);
      skip_children = !location_in_container.Intersects(
          style.GetRoundedInnerBorderFor(bounds_rect));
    }
  }

  if (!skip_children &&
      HitTestChildren(result, box_fragment_.Children(), location_in_container,
                      adjusted_location, action)) {
    return true;
  }

  // TODO(eae): Implement once we support clipping in LayoutNG.
  // if (style.HasBorderRadius() &&
  //     HitTestClippedOutByBorder(location_in_container, adjusted_location))
  // return false;

  // Now hit test ourselves.
  if (hit_test_self && VisibleToHitTestRequest(result.GetHitTestRequest())) {
    LayoutRect bounds_rect(adjusted_location, size);
    if (location_in_container.Intersects(bounds_rect)) {
      Node* node = box_fragment_.GetNode();
      if (!result.InnerNode() && node) {
        LayoutPoint point =
            location_in_container.Point() - ToLayoutSize(adjusted_location);
        result.SetNodeAndPosition(node, point);
      }
      if (result.AddNodeToListBasedTestResult(node, location_in_container,
                                              bounds_rect) == kStopHitTesting) {
        return true;
      }
    }
  }

  return false;
}

bool NGBoxFragmentPainter::VisibleToHitTestRequest(
    const HitTestRequest& request) const {
  return box_fragment_.Style().Visibility() == EVisibility::kVisible &&
         (request.IgnorePointerEventsNone() ||
          box_fragment_.Style().PointerEvents() != EPointerEvents::kNone) &&
         !(box_fragment_.GetNode() && box_fragment_.GetNode()->IsInert());
}

bool NGBoxFragmentPainter::HitTestTextFragment(
    HitTestResult& result,
    const NGPhysicalFragment& text_fragment,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset) {
  LayoutSize offset(text_fragment.Offset().left, text_fragment.Offset().top);
  LayoutPoint adjusted_location = accumulated_offset + offset;
  LayoutSize size(text_fragment.Size().width, text_fragment.Size().height);
  LayoutRect border_rect(adjusted_location, size);
  const ComputedStyle& style = text_fragment.Style();

  if (style.HasBorderRadius()) {
    FloatRoundedRect border = style.GetRoundedBorderFor(
        border_rect,
        text_fragment.BorderEdges() & NGBorderEdges::Physical::kLeft,
        text_fragment.BorderEdges() & NGBorderEdges::Physical::kRight);
    if (!location_in_container.Intersects(border))
      return false;
  }

  // TODO(layout-dev): Clip to line-top/bottom.
  LayoutRect rect = LayoutRect(PixelSnappedIntRect(border_rect));
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      location_in_container.Intersects(rect)) {
    Node* node = text_fragment.GetNode();
    if (!result.InnerNode() && node) {
      LayoutPoint point =
          location_in_container.Point() - ToLayoutSize(accumulated_offset);
      result.SetNodeAndPosition(node, point);
    }

    if (result.AddNodeToListBasedTestResult(node, location_in_container,
                                            rect) == kStopHitTesting) {
      return true;
    }
  }

  return false;
}

bool NGBoxFragmentPainter::HitTestChildren(
    HitTestResult& result,
    const Vector<std::unique_ptr<NGPaintFragment>>& children,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  for (auto iter = children.rbegin(); iter != children.rend(); iter++) {
    const std::unique_ptr<NGPaintFragment>& child = *iter;

    // TODO(layout-dev): Handle self painting layers.
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    bool stop_hit_testing = false;
    if (fragment.Type() == NGPhysicalFragment::kFragmentBox) {
      if (RequiresLegacyFallback(fragment)) {
        stop_hit_testing = fragment.GetLayoutObject()->NodeAtPoint(
            result, location_in_container, accumulated_offset, action);
      } else {
        stop_hit_testing = NGBoxFragmentPainter(*child).NodeAtPoint(
            result, location_in_container, accumulated_offset, action);
      }

    } else if (fragment.Type() == NGPhysicalFragment::kFragmentLineBox) {
      stop_hit_testing =
          HitTestChildren(result, child->Children(), location_in_container,
                          accumulated_offset, action);

    } else if (fragment.Type() == NGPhysicalFragment::kFragmentText) {
      // TODO(eae): Should this hit test on the text itself or the containing
      // node?
      stop_hit_testing = HitTestTextFragment(
          result, fragment, location_in_container, accumulated_offset);
    }
    if (stop_hit_testing)
      return true;
  }

  return false;
}

}  // namespace blink

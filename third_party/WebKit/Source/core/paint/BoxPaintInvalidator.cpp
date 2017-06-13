// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPaintInvalidator.h"

#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

static LayoutRect ComputeRightDelta(const LayoutPoint& location,
                                    const LayoutSize& old_size,
                                    const LayoutSize& new_size,
                                    const LayoutUnit& extra_width) {
  LayoutUnit delta = new_size.Width() - old_size.Width();
  if (delta > 0) {
    return LayoutRect(location.X() + old_size.Width() - extra_width,
                      location.Y(), delta + extra_width, new_size.Height());
  }
  if (delta < 0) {
    return LayoutRect(location.X() + new_size.Width() - extra_width,
                      location.Y(), -delta + extra_width, old_size.Height());
  }
  return LayoutRect();
}

static LayoutRect ComputeBottomDelta(const LayoutPoint& location,
                                     const LayoutSize& old_size,
                                     const LayoutSize& new_size,
                                     const LayoutUnit& extra_height) {
  LayoutUnit delta = new_size.Height() - old_size.Height();
  if (delta > 0) {
    return LayoutRect(location.X(),
                      location.Y() + old_size.Height() - extra_height,
                      new_size.Width(), delta + extra_height);
  }
  if (delta < 0) {
    return LayoutRect(location.X(),
                      location.Y() + new_size.Height() - extra_height,
                      old_size.Width(), -delta + extra_height);
  }
  return LayoutRect();
}

void BoxPaintInvalidator::IncrementallyInvalidatePaint(
    PaintInvalidationReason reason,
    const LayoutRect& old_rect,
    const LayoutRect& new_rect) {
  DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  DCHECK(old_rect.Location() == new_rect.Location());
  DCHECK(old_rect.Size() != new_rect.Size());
  LayoutRect right_delta = ComputeRightDelta(
      new_rect.Location(), old_rect.Size(), new_rect.Size(),
      reason == PaintInvalidationReason::kIncremental ? box_.BorderRight()
                                                      : LayoutUnit());
  LayoutRect bottom_delta = ComputeBottomDelta(
      new_rect.Location(), old_rect.Size(), new_rect.Size(),
      reason == PaintInvalidationReason::kIncremental ? box_.BorderBottom()
                                                      : LayoutUnit());

  DCHECK(!right_delta.IsEmpty() || !bottom_delta.IsEmpty());
  ObjectPaintInvalidatorWithContext object_paint_invalidator(box_, context_);
  object_paint_invalidator.InvalidatePaintRectangleWithContext(right_delta,
                                                               reason);
  object_paint_invalidator.InvalidatePaintRectangleWithContext(bottom_delta,
                                                               reason);
}

PaintInvalidationReason BoxPaintInvalidator::ComputePaintInvalidationReason() {
  PaintInvalidationReason reason =
      ObjectPaintInvalidatorWithContext(box_, context_)
          .ComputePaintInvalidationReason();

  if (reason != PaintInvalidationReason::kIncremental)
    return reason;

  if (box_.IsLayoutView()) {
    const LayoutView& layout_view = ToLayoutView(box_);
    // In normal compositing mode, root background doesn't need to be
    // invalidated for box changes, because the background always covers the
    // whole document rect and clipping is done by
    // compositor()->m_containerLayer. Also the scrollbars are always
    // composited. There are no other box decoration on the LayoutView thus we
    // can safely exit here.
    if (layout_view.UsesCompositing() &&
        !RuntimeEnabledFeatures::RootLayerScrollingEnabled())
      return reason;
  }

  const ComputedStyle& style = box_.StyleRef();

  if ((style.BackgroundLayers().ThisOrNextLayersUseContentBox() ||
       style.MaskLayers().ThisOrNextLayersUseContentBox()) &&
      box_.PreviousContentBoxSize() != box_.ContentBoxRect().Size())
    return PaintInvalidationReason::kGeometry;

  LayoutSize old_border_box_size = box_.PreviousSize();
  LayoutSize new_border_box_size = box_.Size();
  bool border_box_changed = old_border_box_size != new_border_box_size;
  if (!border_box_changed && context_.old_visual_rect == box_.VisualRect())
    return PaintInvalidationReason::kNone;

  // If either border box changed or bounds changed, and old or new border box
  // doesn't equal old or new bounds, incremental invalidation is not
  // applicable. This captures the following cases:
  // - pixel snapping of paint invalidation bounds,
  // - scale, rotate, skew etc. transforms,
  // - visual overflows.
  if (context_.old_visual_rect !=
          LayoutRect(context_.old_location, old_border_box_size) ||
      box_.VisualRect() !=
          LayoutRect(context_.new_location, new_border_box_size)) {
    return PaintInvalidationReason::kGeometry;
  }

  DCHECK(border_box_changed);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // Incremental invalidation is not applicable if there is border in the
    // direction of border box size change because we don't know the border
    // width when issuing incremental raster invalidations.
    if (box_.BorderRight() || box_.BorderBottom())
      return PaintInvalidationReason::kGeometry;
  }

  if (style.HasVisualOverflowingEffect() || style.HasAppearance() ||
      style.HasFilterInducingProperty() || style.HasMask())
    return PaintInvalidationReason::kGeometry;

  if (style.HasBorderRadius())
    return PaintInvalidationReason::kGeometry;

  if (old_border_box_size.Width() != new_border_box_size.Width() &&
      box_.MustInvalidateBackgroundOrBorderPaintOnWidthChange())
    return PaintInvalidationReason::kGeometry;
  if (old_border_box_size.Height() != new_border_box_size.Height() &&
      box_.MustInvalidateBackgroundOrBorderPaintOnHeightChange())
    return PaintInvalidationReason::kGeometry;

  // Needs to repaint frame boundaries.
  if (box_.IsFrameSet())
    return PaintInvalidationReason::kGeometry;

  // Needs to repaint column rules.
  if (box_.IsLayoutMultiColumnSet())
    return PaintInvalidationReason::kGeometry;

  return PaintInvalidationReason::kIncremental;
}

bool BoxPaintInvalidator::BackgroundGeometryDependsOnLayoutOverflowRect() {
  return !box_.IsDocumentElement() && !box_.BackgroundStolenForBeingBody() &&
         box_.StyleRef()
             .BackgroundLayers()
             .ThisOrNextLayersHaveLocalAttachment();
}

// Background positioning in layout overflow rect doesn't mean it will
// paint onto the scrolling contents layer because some conditions prevent
// it from that. We may also treat non-local solid color backgrounds as local
// and paint onto the scrolling contents layer.
// See PaintLayer::canPaintBackgroundOntoScrollingContentsLayer().
bool BoxPaintInvalidator::BackgroundPaintsOntoScrollingContentsLayer() {
  if (box_.IsDocumentElement() || box_.BackgroundStolenForBeingBody())
    return false;
  if (!box_.HasLayer())
    return false;
  if (auto* mapping = box_.Layer()->GetCompositedLayerMapping())
    return mapping->BackgroundPaintsOntoScrollingContentsLayer();
  return false;
}

bool BoxPaintInvalidator::ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
    const LayoutRect& old_layout_overflow,
    const LayoutRect& new_layout_overflow) {
  DCHECK(old_layout_overflow != new_layout_overflow);
  if (new_layout_overflow.IsEmpty() || old_layout_overflow.IsEmpty())
    return true;
  if (new_layout_overflow.Location() != old_layout_overflow.Location())
    return true;
  if (new_layout_overflow.Width() != old_layout_overflow.Width() &&
      box_.MustInvalidateFillLayersPaintOnHeightChange(
          box_.StyleRef().BackgroundLayers()))
    return true;
  if (new_layout_overflow.Height() != old_layout_overflow.Height() &&
      box_.MustInvalidateFillLayersPaintOnHeightChange(
          box_.StyleRef().BackgroundLayers()))
    return true;
  return false;
}

void BoxPaintInvalidator::InvalidateScrollingContentsBackgroundIfNeeded() {
  bool paints_onto_scrolling_contents_layer =
      BackgroundPaintsOntoScrollingContentsLayer();
  if (!paints_onto_scrolling_contents_layer &&
      !BackgroundGeometryDependsOnLayoutOverflowRect())
    return;

  const LayoutRect& old_layout_overflow = box_.PreviousLayoutOverflowRect();
  LayoutRect new_layout_overflow = box_.LayoutOverflowRect();

  bool should_fully_invalidate_on_scrolling_contents_layer = false;
  if (box_.BackgroundChangedSinceLastPaintInvalidation()) {
    if (!paints_onto_scrolling_contents_layer) {
      // The box should have been set needing full invalidation on style change.
      DCHECK(box_.ShouldDoFullPaintInvalidation());
      return;
    }
    should_fully_invalidate_on_scrolling_contents_layer = true;
  } else {
    // Check change of layout overflow for full or incremental invalidation.
    if (new_layout_overflow == old_layout_overflow)
      return;
    bool should_fully_invalidate =
        ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
            old_layout_overflow, new_layout_overflow);
    if (!paints_onto_scrolling_contents_layer) {
      if (should_fully_invalidate) {
        box_.GetMutableForPainting()
            .SetShouldDoFullPaintInvalidationWithoutGeometryChange(
                PaintInvalidationReason::kBackground);
      }
      return;
    }
    should_fully_invalidate_on_scrolling_contents_layer =
        should_fully_invalidate;
  }

  // TODO(crbug.com/732611): Implement raster invalidation of background on
  // scrolling contents layer for SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    if (should_fully_invalidate_on_scrolling_contents_layer) {
      ObjectPaintInvalidatorWithContext(box_, context_)
          .FullyInvalidatePaint(
              PaintInvalidationReason::kBackgroundOnScrollingContentsLayer,
              old_layout_overflow, new_layout_overflow);
    } else {
      IncrementallyInvalidatePaint(
          PaintInvalidationReason::kBackgroundOnScrollingContentsLayer,
          old_layout_overflow, new_layout_overflow);
    }
  }

  context_.painting_layer->SetNeedsRepaint();
  // Currently we use CompositedLayerMapping as the DisplayItemClient to paint
  // background on the scrolling contents layer.
  ObjectPaintInvalidator(box_).InvalidateDisplayItemClient(
      *box_.Layer()->GetCompositedLayerMapping()->ScrollingContentsLayer(),
      PaintInvalidationReason::kBackgroundOnScrollingContentsLayer);
}

PaintInvalidationReason BoxPaintInvalidator::InvalidatePaint() {
  InvalidateScrollingContentsBackgroundIfNeeded();

  PaintInvalidationReason reason = ComputePaintInvalidationReason();
  if (reason == PaintInvalidationReason::kIncremental) {
    bool should_invalidate;
    if (box_.IsLayoutView() &&
        !RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      should_invalidate = context_.old_visual_rect != box_.VisualRect();
      if (should_invalidate &&
          !RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        IncrementallyInvalidatePaint(reason, context_.old_visual_rect,
                                     box_.VisualRect());
      }
    } else {
      should_invalidate = box_.PreviousSize() != box_.Size();
      if (should_invalidate &&
          !RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        IncrementallyInvalidatePaint(
            reason, LayoutRect(context_.old_location, box_.PreviousSize()),
            LayoutRect(context_.new_location, box_.Size()));
      }
    }
    if (should_invalidate) {
      context_.painting_layer->SetNeedsRepaint();
      box_.InvalidateDisplayItemClients(reason);
    } else {
      reason = PaintInvalidationReason::kNone;
    }

    // Though we have done incremental invalidation, we still need to call
    // ObjectPaintInvalidator with PaintInvalidationNone to do any other
    // required operations.
    reason = std::max(reason, ObjectPaintInvalidatorWithContext(box_, context_)
                                  .InvalidatePaintWithComputedReason(
                                      PaintInvalidationReason::kNone));
  } else {
    reason = ObjectPaintInvalidatorWithContext(box_, context_)
                 .InvalidatePaintWithComputedReason(reason);
  }

  if (PaintLayerScrollableArea* area = box_.GetScrollableArea())
    area->InvalidatePaintOfScrollControlsIfNeeded(context_);

  // This is for the next invalidatePaintIfNeeded so must be at the end.
  SavePreviousBoxGeometriesIfNeeded();

  return reason;
}

bool BoxPaintInvalidator::
    NeedsToSavePreviousContentBoxSizeOrLayoutOverflowRect() {
  // Don't save old box geometries if the paint rect is empty because we'll
  // fully invalidate once the paint rect becomes non-empty.
  if (box_.VisualRect().IsEmpty())
    return false;

  if (box_.PaintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return false;

  const ComputedStyle& style = box_.StyleRef();

  // Background and mask layers can depend on other boxes than border box. See
  // crbug.com/490533
  if ((style.BackgroundLayers().ThisOrNextLayersUseContentBox() ||
       style.MaskLayers().ThisOrNextLayersUseContentBox()) &&
      box_.ContentBoxRect().Size() != box_.Size())
    return true;
  if ((BackgroundGeometryDependsOnLayoutOverflowRect() ||
       BackgroundPaintsOntoScrollingContentsLayer()) &&
      box_.LayoutOverflowRect() != box_.BorderBoxRect())
    return true;

  return false;
}

void BoxPaintInvalidator::SavePreviousBoxGeometriesIfNeeded() {
  box_.GetMutableForPainting().SavePreviousSize();

  if (NeedsToSavePreviousContentBoxSizeOrLayoutOverflowRect()) {
    box_.GetMutableForPainting()
        .SavePreviousContentBoxSizeAndLayoutOverflowRect();
  } else {
    box_.GetMutableForPainting()
        .ClearPreviousContentBoxSizeAndLayoutOverflowRect();
  }
}

}  // namespace blink

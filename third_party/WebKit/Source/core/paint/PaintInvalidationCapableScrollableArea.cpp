// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInvalidationCapableScrollableArea.h"

#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/PaintInvalidationState.h"
#include "core/paint/FindPaintOffsetAndVisualRectNeedingUpdate.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"

namespace blink {

void PaintInvalidationCapableScrollableArea::WillRemoveScrollbar(
    Scrollbar& scrollbar,
    ScrollbarOrientation orientation) {
  if (!scrollbar.IsCustomScrollbar() &&
      !(orientation == kHorizontalScrollbar ? LayerForHorizontalScrollbar()
                                            : LayerForVerticalScrollbar())) {
    ObjectPaintInvalidator(*GetLayoutBox())
        .SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
            scrollbar, PaintInvalidationReason::kScrollControl);
  }

  ScrollableArea::WillRemoveScrollbar(scrollbar, orientation);
}

static LayoutRect ScrollControlVisualRect(
    const IntRect& scroll_control_rect,
    const LayoutBox& box,
    const PaintInvalidatorContext& context,
    const LayoutRect& previous_visual_rect) {
  LayoutRect visual_rect(scroll_control_rect);
#if DCHECK_IS_ON()
  FindVisualRectNeedingUpdateScope finder(box, context, previous_visual_rect,
                                          visual_rect);
#endif
  if (!context.NeedsVisualRectUpdate(box))
    return previous_visual_rect;

  // No need to apply any paint offset. Scroll controls paint in a different
  // transform space than their contained box (the scrollbarPaintOffset
  // transform node).
  if (!visual_rect.IsEmpty() &&
      !RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    // PaintInvalidatorContext::mapLocalRectToPaintInvalidationBacking() treats
    // the rect as in flipped block direction, but scrollbar controls don't
    // flip for block direction, so flip here to undo the flip in the function.
    box.FlipForWritingMode(visual_rect);
    context.MapLocalRectToVisualRectInBacking(box, visual_rect);
  }
  return visual_rect;
}

// Returns true if the scroll control is invalidated.
static bool InvalidatePaintOfScrollControlIfNeeded(
    const LayoutRect& new_visual_rect,
    const LayoutRect& previous_visual_rect,
    bool needs_paint_invalidation,
    LayoutBox& box,
    const LayoutBoxModelObject& paint_invalidation_container) {
  bool should_invalidate_new_rect = needs_paint_invalidation;
  if (new_visual_rect != previous_visual_rect) {
    ObjectPaintInvalidator(box).InvalidatePaintUsingContainer(
        paint_invalidation_container, previous_visual_rect,
        PaintInvalidationReason::kScrollControl);
    should_invalidate_new_rect = true;
  }
  if (should_invalidate_new_rect) {
    ObjectPaintInvalidator(box).InvalidatePaintUsingContainer(
        paint_invalidation_container, new_visual_rect,
        PaintInvalidationReason::kScrollControl);
    return true;
  }
  return false;
}

static LayoutRect InvalidatePaintOfScrollbarIfNeeded(
    Scrollbar* scrollbar,
    GraphicsLayer* graphics_layer,
    bool& previously_was_overlay,
    const LayoutRect& previous_visual_rect,
    bool needs_paint_invalidation_arg,
    LayoutBox& box,
    const PaintInvalidatorContext& context) {
  bool is_overlay = scrollbar && scrollbar->IsOverlayScrollbar();

  LayoutRect new_visual_rect;
  // Calculate visual rect of the scrollbar, except overlay composited
  // scrollbars because we invalidate the graphics layer only.
  if (scrollbar && !(graphics_layer && is_overlay)) {
    new_visual_rect = ScrollControlVisualRect(scrollbar->FrameRect(), box,
                                              context, previous_visual_rect);
  }

  bool needs_paint_invalidation = needs_paint_invalidation_arg;
  if (needs_paint_invalidation && graphics_layer) {
    // If the scrollbar needs paint invalidation but didn't change location/size
    // or the scrollbar is an overlay scrollbar (visual rect is empty),
    // invalidating the graphics layer is enough (which has been done in
    // ScrollableArea::setScrollbarNeedsPaintInvalidation()).
    // Otherwise invalidatePaintOfScrollControlIfNeeded() below will invalidate
    // the old and new location of the scrollbar on the box's paint invalidation
    // container to ensure newly expanded/shrunk areas of the box to be
    // invalidated.
    needs_paint_invalidation = false;
    DCHECK(!graphics_layer->DrawsContent() ||
           graphics_layer->GetPaintController().CacheIsEmpty());
  }

  // Invalidate the box's display item client if the box's padding box size is
  // affected by change of the non-overlay scrollbar width. We detect change of
  // visual rect size instead of change of scrollbar width change, which may
  // have some false-positives (e.g. the scrollbar changed length but not width)
  // but won't invalidate more than expected because in the false-positive case
  // the box must have changed size and have been invalidated.
  const LayoutBoxModelObject& paint_invalidation_container =
      *context.paint_invalidation_container;
  LayoutSize new_scrollbar_used_space_in_box;
  if (!is_overlay)
    new_scrollbar_used_space_in_box = new_visual_rect.Size();
  LayoutSize previous_scrollbar_used_space_in_box;
  if (!previously_was_overlay)
    previous_scrollbar_used_space_in_box = previous_visual_rect.Size();
  if (new_scrollbar_used_space_in_box != previous_scrollbar_used_space_in_box) {
    context.painting_layer->SetNeedsRepaint();
    ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
        box, PaintInvalidationReason::kGeometry);
  }

  bool invalidated = InvalidatePaintOfScrollControlIfNeeded(
      new_visual_rect, previous_visual_rect, needs_paint_invalidation, box,
      paint_invalidation_container);

  previously_was_overlay = is_overlay;

  if (!invalidated || !scrollbar || graphics_layer)
    return new_visual_rect;

  context.painting_layer->SetNeedsRepaint();
  ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
      *scrollbar, PaintInvalidationReason::kScrollControl);
  if (scrollbar->IsCustomScrollbar()) {
    ToLayoutScrollbar(scrollbar)
        ->InvalidateDisplayItemClientsOfScrollbarParts();
  }

  return new_visual_rect;
}

void PaintInvalidationCapableScrollableArea::
    InvalidatePaintOfScrollControlsIfNeeded(
        const PaintInvalidatorContext& context) {
  LayoutBox& box = *GetLayoutBox();
  SetHorizontalScrollbarVisualRect(InvalidatePaintOfScrollbarIfNeeded(
      HorizontalScrollbar(), LayerForHorizontalScrollbar(),
      horizontal_scrollbar_previously_was_overlay_,
      horizontal_scrollbar_visual_rect_,
      HorizontalScrollbarNeedsPaintInvalidation(), box, context));
  SetVerticalScrollbarVisualRect(InvalidatePaintOfScrollbarIfNeeded(
      VerticalScrollbar(), LayerForVerticalScrollbar(),
      vertical_scrollbar_previously_was_overlay_,
      vertical_scrollbar_visual_rect_,
      VerticalScrollbarNeedsPaintInvalidation(), box, context));

  LayoutRect scroll_corner_and_resizer_visual_rect =
      ScrollControlVisualRect(ScrollCornerAndResizerRect(), box, context,
                              scroll_corner_and_resizer_visual_rect_);
  const LayoutBoxModelObject& paint_invalidation_container =
      *context.paint_invalidation_container;
  if (InvalidatePaintOfScrollControlIfNeeded(
          scroll_corner_and_resizer_visual_rect,
          scroll_corner_and_resizer_visual_rect_,
          ScrollCornerNeedsPaintInvalidation(), box,
          paint_invalidation_container)) {
    SetScrollCornerAndResizerVisualRect(scroll_corner_and_resizer_visual_rect);
    if (LayoutScrollbarPart* scroll_corner = this->ScrollCorner()) {
      ObjectPaintInvalidator(*scroll_corner)
          .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationReason::kScrollControl);
    }
    if (LayoutScrollbarPart* resizer = this->Resizer()) {
      ObjectPaintInvalidator(*resizer)
          .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationReason::kScrollControl);
    }
  }

  ClearNeedsPaintInvalidationForScrollControls();
}

void PaintInvalidationCapableScrollableArea::
    InvalidatePaintOfScrollControlsIfNeeded(
        const PaintInvalidationState& paint_invalidation_state) {
  InvalidatePaintOfScrollControlsIfNeeded(
      PaintInvalidatorContextAdapter(paint_invalidation_state));
}

void PaintInvalidationCapableScrollableArea::ClearPreviousVisualRects() {
  SetHorizontalScrollbarVisualRect(LayoutRect());
  SetVerticalScrollbarVisualRect(LayoutRect());
  SetScrollCornerAndResizerVisualRect(LayoutRect());
}

void PaintInvalidationCapableScrollableArea::SetHorizontalScrollbarVisualRect(
    const LayoutRect& rect) {
  horizontal_scrollbar_visual_rect_ = rect;
  if (Scrollbar* scrollbar = HorizontalScrollbar())
    scrollbar->SetVisualRect(rect);
}

void PaintInvalidationCapableScrollableArea::SetVerticalScrollbarVisualRect(
    const LayoutRect& rect) {
  vertical_scrollbar_visual_rect_ = rect;
  if (Scrollbar* scrollbar = VerticalScrollbar())
    scrollbar->SetVisualRect(rect);
}

void PaintInvalidationCapableScrollableArea::
    SetScrollCornerAndResizerVisualRect(const LayoutRect& rect) {
  scroll_corner_and_resizer_visual_rect_ = rect;
  if (LayoutScrollbarPart* scroll_corner = this->ScrollCorner())
    scroll_corner->SetVisualRect(rect);
  if (LayoutScrollbarPart* resizer = this->Resizer())
    resizer->SetVisualRect(rect);
}

void PaintInvalidationCapableScrollableArea::
    ScrollControlWasSetNeedsPaintInvalidation() {
  GetLayoutBox()->SetMayNeedPaintInvalidation();
}

void PaintInvalidationCapableScrollableArea::DidScrollWithScrollbar(
    ScrollbarPart part,
    ScrollbarOrientation orientation) {
  UseCounter::Feature scrollbar_use_uma;
  switch (part) {
    case kBackButtonStartPart:
    case kForwardButtonStartPart:
    case kBackButtonEndPart:
    case kForwardButtonEndPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? UseCounter::kScrollbarUseVerticalScrollbarButton
               : UseCounter::kScrollbarUseHorizontalScrollbarButton);
      break;
    case kThumbPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? UseCounter::kScrollbarUseVerticalScrollbarThumb
               : UseCounter::kScrollbarUseHorizontalScrollbarThumb);
      break;
    case kBackTrackPart:
    case kForwardTrackPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? UseCounter::kScrollbarUseVerticalScrollbarTrack
               : UseCounter::kScrollbarUseHorizontalScrollbarTrack);
      break;
    default:
      return;
  }

  UseCounter::Count(GetLayoutBox()->GetDocument(), scrollbar_use_uma);
}

}  // namespace blink

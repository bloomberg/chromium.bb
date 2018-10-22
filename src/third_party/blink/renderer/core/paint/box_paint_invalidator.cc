// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

PaintInvalidationReason BoxPaintInvalidator::ComputePaintInvalidationReason() {
  PaintInvalidationReason reason =
      ObjectPaintInvalidatorWithContext(box_, context_)
          .ComputePaintInvalidationReason();

  if (reason != PaintInvalidationReason::kIncremental)
    return reason;

  const ComputedStyle& style = box_.StyleRef();

  if ((style.BackgroundLayers().ThisOrNextLayersUseContentBox() ||
       style.MaskLayers().ThisOrNextLayersUseContentBox()) &&
      box_.PreviousPhysicalContentBoxRect() != box_.PhysicalContentBoxRect()) {
    return PaintInvalidationReason::kGeometry;
  }

  LayoutSize old_border_box_size = box_.PreviousSize();
  LayoutSize new_border_box_size = box_.Size();
  bool border_box_changed = old_border_box_size != new_border_box_size;
  if (!border_box_changed &&
      context_.old_visual_rect == context_.fragment_data->VisualRect())
    return PaintInvalidationReason::kNone;

  // If either border box changed or bounds changed, and old or new border box
  // doesn't equal old or new bounds, incremental invalidation is not
  // applicable. This captures the following cases:
  // - pixel snapping, or not snapping e.g. for some visual overflowing effects,
  // - scale, rotate, skew etc. transforms,
  // - visual (ink) overflows.
  if (context_.old_visual_rect !=
          LayoutRect(context_.old_paint_offset, old_border_box_size) ||
      context_.fragment_data->VisualRect() !=
          LayoutRect(context_.fragment_data->PaintOffset(),
                     new_border_box_size)) {
    return PaintInvalidationReason::kGeometry;
  }

  DCHECK(border_box_changed);

  // Incremental invalidation is not applicable if there is border in the
  // direction of border box size change because we don't know the border
  // width when issuing incremental raster invalidations.
  if (box_.BorderRight() || box_.BorderBottom())
    return PaintInvalidationReason::kGeometry;

  if (style.HasVisualOverflowingEffect() || style.HasAppearance() ||
      style.HasFilterInducingProperty() || style.HasMask() || style.ClipPath())
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

bool BoxPaintInvalidator::BackgroundGeometryDependsOnLayoutOverflowRect()
    const {
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
    const LayoutRect& new_layout_overflow) const {
  if (new_layout_overflow == old_layout_overflow)
    return false;

  // TODO(pdr): This check can likely be removed because size changes are
  // caught below.
  if (new_layout_overflow.IsEmpty() || old_layout_overflow.IsEmpty())
    return true;

  if (new_layout_overflow.Location() != old_layout_overflow.Location()) {
    auto& layers = box_.StyleRef().BackgroundLayers();
    // The background should invalidate on most location changes but we can
    // avoid invalidation in a common case if the background is a single color
    // that fully covers the overflow area.
    // TODO(pdr): Check all background layers instead of skipping this if there
    // are multiple backgrounds.
    if (layers.Next() || layers.GetImage() ||
        layers.RepeatX() != EFillRepeat::kRepeatFill ||
        layers.RepeatY() != EFillRepeat::kRepeatFill)
      return true;
  }

  if (new_layout_overflow.Width() != old_layout_overflow.Width() &&
      box_.MustInvalidateFillLayersPaintOnWidthChange(
          box_.StyleRef().BackgroundLayers()))
    return true;
  if (new_layout_overflow.Height() != old_layout_overflow.Height() &&
      box_.MustInvalidateFillLayersPaintOnHeightChange(
          box_.StyleRef().BackgroundLayers()))
    return true;

  return false;
}

bool BoxPaintInvalidator::ViewBackgroundShouldFullyInvalidate() const {
  DCHECK(box_.IsLayoutView());
  // Fixed attachment background is handled in LayoutView::layout().
  // TODO(wangxianzhu): Combine code for fixed-attachment background.
  if (box_.StyleRef().HasEntirelyFixedBackground())
    return false;

  // LayoutView's non-fixed-attachment background is positioned in the
  // document element and needs to invalidate if the size changes.
  // See: https://drafts.csswg.org/css-backgrounds-3/#root-background.
  if (BackgroundGeometryDependsOnLayoutOverflowRect()) {
    Element* document_element = box_.GetDocument().documentElement();
    if (document_element) {
      const auto* document_background = document_element->GetLayoutObject();
      if (document_background && document_background->IsBox()) {
        const auto* document_background_box = ToLayoutBox(document_background);
        if (ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
                document_background_box->PreviousPhysicalLayoutOverflowRect(),
                document_background_box->PhysicalLayoutOverflowRect())) {
          return true;
        }
      }
    }
  }
  return false;
}

BoxPaintInvalidator::BackgroundInvalidationType
BoxPaintInvalidator::ComputeBackgroundInvalidation() {
  if (box_.BackgroundChangedSinceLastPaintInvalidation())
    return BackgroundInvalidationType::kFull;

  if (box_.IsLayoutView() && ViewBackgroundShouldFullyInvalidate())
    return BackgroundInvalidationType::kFull;

  bool layout_overflow_change_causes_invalidation =
      (BackgroundGeometryDependsOnLayoutOverflowRect() ||
       BackgroundPaintsOntoScrollingContentsLayer());

  if (!layout_overflow_change_causes_invalidation)
    return BackgroundInvalidationType::kNone;

  const LayoutRect& old_layout_overflow =
      box_.PreviousPhysicalLayoutOverflowRect();
  LayoutRect new_layout_overflow = box_.PhysicalLayoutOverflowRect();
  if (ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
          old_layout_overflow, new_layout_overflow))
    return BackgroundInvalidationType::kFull;

  if (new_layout_overflow != old_layout_overflow) {
    // Do incremental invalidation if possible.
    if (old_layout_overflow.Location() == new_layout_overflow.Location())
      return BackgroundInvalidationType::kIncremental;
    return BackgroundInvalidationType::kFull;
  }
  return BackgroundInvalidationType::kNone;
}

void BoxPaintInvalidator::InvalidateBackground() {
  BackgroundInvalidationType background_invalidation_type =
      ComputeBackgroundInvalidation();

  if (BackgroundPaintsOntoScrollingContentsLayer()) {
    if (background_invalidation_type != BackgroundInvalidationType::kNone) {
      auto reason =
          background_invalidation_type == BackgroundInvalidationType::kFull
              ? PaintInvalidationReason::kBackgroundOnScrollingContentsLayer
              : PaintInvalidationReason::kIncremental;
      context_.painting_layer->SetNeedsRepaint();
      ObjectPaintInvalidator(box_).InvalidateDisplayItemClient(
          *box_.Layer()->GetCompositedLayerMapping()->ScrollingContentsLayer(),
          reason);
    }
  } else if (background_invalidation_type ==
             BackgroundInvalidationType::kFull) {
    box_.GetMutableForPainting()
        .SetShouldDoFullPaintInvalidationWithoutGeometryChange(
            PaintInvalidationReason::kBackground);
  }
}

void BoxPaintInvalidator::InvalidatePaint() {
  InvalidateBackground();

  ObjectPaintInvalidatorWithContext(box_, context_)
      .InvalidatePaintWithComputedReason(ComputePaintInvalidationReason());

  if (PaintLayerScrollableArea* area = box_.GetScrollableArea())
    area->InvalidatePaintOfScrollControlsIfNeeded(context_);

  // This is for the next invalidatePaintIfNeeded so must be at the end.
  SavePreviousBoxGeometriesIfNeeded();
}

bool BoxPaintInvalidator::
    NeedsToSavePreviousContentBoxRectOrLayoutOverflowRect() {
  // The LayoutView depends on the document element's layout overflow rect (see:
  // ViewBackgroundShouldFullyInvalidate) and needs to invalidate before the
  // document element invalidates. There are few document elements so the
  // previous layout overflow rect is always saved, rather than duplicating the
  // logic save-if-needed logic for this special case.
  if (box_.IsDocumentElement())
    return true;

  // Replaced elements are clipped to the content box thus we need to check
  // for its size.
  if (box_.IsLayoutReplaced())
    return true;

  // Don't save old box geometries if the paint rect is empty because we'll
  // fully invalidate once the paint rect becomes non-empty.
  if (context_.fragment_data->VisualRect().IsEmpty())
    return false;

  if (box_.PaintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return false;

  const ComputedStyle& style = box_.StyleRef();

  // Background and mask layers can depend on other boxes than border box. See
  // crbug.com/490533
  if ((style.BackgroundLayers().ThisOrNextLayersUseContentBox() ||
       style.MaskLayers().ThisOrNextLayersUseContentBox()) &&
      box_.ContentSize() != box_.Size())
    return true;
  if ((BackgroundGeometryDependsOnLayoutOverflowRect() ||
       BackgroundPaintsOntoScrollingContentsLayer()) &&
      box_.LayoutOverflowRect() != box_.BorderBoxRect())
    return true;

  return false;
}

void BoxPaintInvalidator::SavePreviousBoxGeometriesIfNeeded() {
  box_.GetMutableForPainting().SavePreviousSize();

  if (NeedsToSavePreviousContentBoxRectOrLayoutOverflowRect()) {
    box_.GetMutableForPainting()
        .SavePreviousContentBoxRectAndLayoutOverflowRect();
  } else {
    box_.GetMutableForPainting()
        .ClearPreviousContentBoxRectAndLayoutOverflowRect();
  }
}

}  // namespace blink

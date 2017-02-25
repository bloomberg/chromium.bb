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

static LayoutRect computeRightDelta(const LayoutPoint& location,
                                    const LayoutSize& oldSize,
                                    const LayoutSize& newSize,
                                    const LayoutUnit& extraWidth) {
  LayoutUnit delta = newSize.width() - oldSize.width();
  if (delta > 0) {
    return LayoutRect(location.x() + oldSize.width() - extraWidth, location.y(),
                      delta + extraWidth, newSize.height());
  }
  if (delta < 0) {
    return LayoutRect(location.x() + newSize.width() - extraWidth, location.y(),
                      -delta + extraWidth, oldSize.height());
  }
  return LayoutRect();
}

static LayoutRect computeBottomDelta(const LayoutPoint& location,
                                     const LayoutSize& oldSize,
                                     const LayoutSize& newSize,
                                     const LayoutUnit& extraHeight) {
  LayoutUnit delta = newSize.height() - oldSize.height();
  if (delta > 0) {
    return LayoutRect(location.x(),
                      location.y() + oldSize.height() - extraHeight,
                      newSize.width(), delta + extraHeight);
  }
  if (delta < 0) {
    return LayoutRect(location.x(),
                      location.y() + newSize.height() - extraHeight,
                      oldSize.width(), -delta + extraHeight);
  }
  return LayoutRect();
}

bool BoxPaintInvalidator::incrementallyInvalidatePaint(
    PaintInvalidationReason reason,
    const LayoutRect& oldRect,
    const LayoutRect& newRect) {
  DCHECK(oldRect.location() == newRect.location());
  LayoutRect rightDelta = computeRightDelta(
      newRect.location(), oldRect.size(), newRect.size(),
      reason == PaintInvalidationIncremental ? m_box.borderRight()
                                             : LayoutUnit());
  LayoutRect bottomDelta = computeBottomDelta(
      newRect.location(), oldRect.size(), newRect.size(),
      reason == PaintInvalidationIncremental ? m_box.borderBottom()
                                             : LayoutUnit());

  if (rightDelta.isEmpty() && bottomDelta.isEmpty())
    return false;

  ObjectPaintInvalidatorWithContext objectPaintInvalidator(m_box, m_context);
  objectPaintInvalidator.invalidatePaintRectangleWithContext(rightDelta,
                                                             reason);
  objectPaintInvalidator.invalidatePaintRectangleWithContext(bottomDelta,
                                                             reason);
  return true;
}

PaintInvalidationReason BoxPaintInvalidator::computePaintInvalidationReason() {
  PaintInvalidationReason reason =
      ObjectPaintInvalidatorWithContext(m_box, m_context)
          .computePaintInvalidationReason();

  if (reason != PaintInvalidationIncremental)
    return reason;

  if (m_box.isLayoutView()) {
    const LayoutView& layoutView = toLayoutView(m_box);
    // In normal compositing mode, root background doesn't need to be
    // invalidated for box changes, because the background always covers the
    // whole document rect and clipping is done by
    // compositor()->m_containerLayer. Also the scrollbars are always
    // composited. There are no other box decoration on the LayoutView thus we
    // can safely exit here.
    if (layoutView.usesCompositing() &&
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      return reason;
  }

  const ComputedStyle& style = m_box.styleRef();

  if ((style.backgroundLayers().thisOrNextLayersUseContentBox() ||
       style.maskLayers().thisOrNextLayersUseContentBox()) &&
      m_box.previousContentBoxSize() != m_box.contentBoxRect().size())
    return PaintInvalidationContentBoxChange;

  LayoutSize oldBorderBoxSize = m_box.previousSize();
  LayoutSize newBorderBoxSize = m_box.size();
  bool borderBoxChanged = oldBorderBoxSize != newBorderBoxSize;
  if (!borderBoxChanged && m_context.oldVisualRect == m_context.newVisualRect)
    return PaintInvalidationNone;

  // If either border box changed or bounds changed, and old or new border box
  // doesn't equal old or new bounds, incremental invalidation is not
  // applicable. This captures the following cases:
  // - pixel snapping of paint invalidation bounds,
  // - scale, rotate, skew etc. transforms,
  // - visual overflows.
  if (m_context.oldVisualRect !=
          LayoutRect(m_context.oldLocation, oldBorderBoxSize) ||
      m_context.newVisualRect !=
          LayoutRect(m_context.newLocation, newBorderBoxSize)) {
    return borderBoxChanged ? PaintInvalidationBorderBoxChange
                            : PaintInvalidationBoundsChange;
  }

  DCHECK(borderBoxChanged);

  if (style.hasVisualOverflowingEffect() || style.hasAppearance() ||
      style.hasFilterInducingProperty() || style.resize() != RESIZE_NONE ||
      style.hasMask())
    return PaintInvalidationBorderBoxChange;

  if (style.hasBorderRadius())
    return PaintInvalidationBorderBoxChange;

  if (oldBorderBoxSize.width() != newBorderBoxSize.width() &&
      m_box.mustInvalidateBackgroundOrBorderPaintOnWidthChange())
    return PaintInvalidationBorderBoxChange;
  if (oldBorderBoxSize.height() != newBorderBoxSize.height() &&
      m_box.mustInvalidateBackgroundOrBorderPaintOnHeightChange())
    return PaintInvalidationBorderBoxChange;

  // Needs to repaint frame boundaries.
  if (m_box.isFrameSet())
    return PaintInvalidationBorderBoxChange;

  // Needs to repaint column rules.
  if (m_box.isLayoutMultiColumnSet())
    return PaintInvalidationBorderBoxChange;

  return PaintInvalidationIncremental;
}

bool BoxPaintInvalidator::backgroundGeometryDependsOnLayoutOverflowRect() {
  return !m_box.isDocumentElement() && !m_box.backgroundStolenForBeingBody() &&
         m_box.styleRef()
             .backgroundLayers()
             .thisOrNextLayersHaveLocalAttachment();
}

// Background positioning in layout overflow rect doesn't mean it will
// paint onto the scrolling contents layer because some conditions prevent
// it from that. We may also treat non-local solid color backgrounds as local
// and paint onto the scrolling contents layer.
// See PaintLayer::canPaintBackgroundOntoScrollingContentsLayer().
bool BoxPaintInvalidator::backgroundPaintsOntoScrollingContentsLayer() {
  if (m_box.isDocumentElement() || m_box.backgroundStolenForBeingBody())
    return false;
  if (!m_box.hasLayer())
    return false;
  if (auto* mapping = m_box.layer()->compositedLayerMapping())
    return mapping->backgroundPaintsOntoScrollingContentsLayer();
  return false;
}

bool BoxPaintInvalidator::shouldFullyInvalidateBackgroundOnLayoutOverflowChange(
    const LayoutRect& oldLayoutOverflow,
    const LayoutRect& newLayoutOverflow) {
  DCHECK(oldLayoutOverflow != newLayoutOverflow);
  if (newLayoutOverflow.isEmpty() || oldLayoutOverflow.isEmpty())
    return true;
  if (newLayoutOverflow.location() != oldLayoutOverflow.location())
    return true;
  if (newLayoutOverflow.width() != oldLayoutOverflow.width() &&
      m_box.mustInvalidateFillLayersPaintOnHeightChange(
          m_box.styleRef().backgroundLayers()))
    return true;
  if (newLayoutOverflow.height() != oldLayoutOverflow.height() &&
      m_box.mustInvalidateFillLayersPaintOnHeightChange(
          m_box.styleRef().backgroundLayers()))
    return true;
  return false;
}

void BoxPaintInvalidator::invalidateScrollingContentsBackgroundIfNeeded() {
  bool paintsOntoScrollingContentsLayer =
      backgroundPaintsOntoScrollingContentsLayer();
  if (!paintsOntoScrollingContentsLayer &&
      !backgroundGeometryDependsOnLayoutOverflowRect())
    return;

  const LayoutRect& oldLayoutOverflow = m_box.previousLayoutOverflowRect();
  LayoutRect newLayoutOverflow = m_box.layoutOverflowRect();

  bool shouldFullyInvalidateOnScrollingContentsLayer = false;
  if (m_box.backgroundChangedSinceLastPaintInvalidation()) {
    if (!paintsOntoScrollingContentsLayer) {
      // The box should have been set needing full invalidation on style change.
      DCHECK(m_box.shouldDoFullPaintInvalidation());
      return;
    }
    shouldFullyInvalidateOnScrollingContentsLayer = true;
  } else {
    // Check change of layout overflow for full or incremental invalidation.
    if (newLayoutOverflow == oldLayoutOverflow)
      return;
    bool shouldFullyInvalidate =
        shouldFullyInvalidateBackgroundOnLayoutOverflowChange(
            oldLayoutOverflow, newLayoutOverflow);
    if (!paintsOntoScrollingContentsLayer) {
      if (shouldFullyInvalidate) {
        m_box.getMutableForPainting().setShouldDoFullPaintInvalidation(
            PaintInvalidationLayoutOverflowBoxChange);
      }
      return;
    }
    shouldFullyInvalidateOnScrollingContentsLayer = shouldFullyInvalidate;
  }

  if (shouldFullyInvalidateOnScrollingContentsLayer) {
    ObjectPaintInvalidatorWithContext(m_box, m_context)
        .fullyInvalidatePaint(
            PaintInvalidationBackgroundOnScrollingContentsLayer,
            oldLayoutOverflow, newLayoutOverflow);
  } else {
    incrementallyInvalidatePaint(
        PaintInvalidationBackgroundOnScrollingContentsLayer, oldLayoutOverflow,
        newLayoutOverflow);
  }

  m_context.paintingLayer->setNeedsRepaint();
  // Currently we use CompositedLayerMapping as the DisplayItemClient to paint
  // background on the scrolling contents layer.
  ObjectPaintInvalidator(m_box).invalidateDisplayItemClient(
      *m_box.layer()->compositedLayerMapping()->scrollingContentsLayer(),
      PaintInvalidationBackgroundOnScrollingContentsLayer);
}

PaintInvalidationReason BoxPaintInvalidator::invalidatePaintIfNeeded() {
  invalidateScrollingContentsBackgroundIfNeeded();

  PaintInvalidationReason reason = computePaintInvalidationReason();
  if (reason == PaintInvalidationIncremental) {
    bool invalidated;
    if (m_box.isLayoutView() &&
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      invalidated = incrementallyInvalidatePaint(
          reason, m_context.oldVisualRect, m_context.newVisualRect);
    } else {
      invalidated = incrementallyInvalidatePaint(
          reason, LayoutRect(m_context.oldLocation, m_box.previousSize()),
          LayoutRect(m_context.newLocation, m_box.size()));
    }
    if (invalidated) {
      m_context.paintingLayer->setNeedsRepaint();
      m_box.invalidateDisplayItemClients(reason);
    } else {
      reason = PaintInvalidationNone;
    }

    // Though we have done incremental invalidation, we still need to call
    // ObjectPaintInvalidator with PaintInvalidationNone to do any other
    // required operations.
    reason = std::max(
        reason,
        ObjectPaintInvalidatorWithContext(m_box, m_context)
            .invalidatePaintIfNeededWithComputedReason(PaintInvalidationNone));
  } else {
    reason = ObjectPaintInvalidatorWithContext(m_box, m_context)
                 .invalidatePaintIfNeededWithComputedReason(reason);
  }

  if (PaintLayerScrollableArea* area = m_box.getScrollableArea())
    area->invalidatePaintOfScrollControlsIfNeeded(m_context);

  // This is for the next invalidatePaintIfNeeded so must be at the end.
  savePreviousBoxGeometriesIfNeeded();

  return reason;
}

bool BoxPaintInvalidator::
    needsToSavePreviousContentBoxSizeOrLayoutOverflowRect() {
  // Don't save old box geometries if the paint rect is empty because we'll
  // fully invalidate once the paint rect becomes non-empty.
  if (m_context.newVisualRect.isEmpty())
    return false;

  if (m_box.paintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return false;

  const ComputedStyle& style = m_box.styleRef();

  // Background and mask layers can depend on other boxes than border box. See
  // crbug.com/490533
  if ((style.backgroundLayers().thisOrNextLayersUseContentBox() ||
       style.maskLayers().thisOrNextLayersUseContentBox()) &&
      m_box.contentBoxRect().size() != m_box.size())
    return true;
  if ((backgroundGeometryDependsOnLayoutOverflowRect() ||
       backgroundPaintsOntoScrollingContentsLayer()) &&
      m_box.layoutOverflowRect() != m_box.borderBoxRect())
    return true;

  return false;
}

void BoxPaintInvalidator::savePreviousBoxGeometriesIfNeeded() {
  m_box.getMutableForPainting().savePreviousSize();

  if (needsToSavePreviousContentBoxSizeOrLayoutOverflowRect()) {
    m_box.getMutableForPainting()
        .savePreviousContentBoxSizeAndLayoutOverflowRect();
  } else {
    m_box.getMutableForPainting()
        .clearPreviousContentBoxSizeAndLayoutOverflowRect();
  }
}

}  // namespace blink

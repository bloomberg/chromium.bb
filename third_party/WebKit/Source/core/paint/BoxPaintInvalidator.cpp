// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPaintInvalidator.h"

#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

struct PreviousBoxSizes {
  LayoutSize borderBoxSize;
  LayoutRect contentBoxRect;
  LayoutRect layoutOverflowRect;
};

typedef HashMap<const LayoutBox*, PreviousBoxSizes> PreviousBoxSizesMap;
static PreviousBoxSizesMap& previousBoxSizesMap() {
  DEFINE_STATIC_LOCAL(PreviousBoxSizesMap, map, ());
  return map;
}

void BoxPaintInvalidator::boxWillBeDestroyed(const LayoutBox& box) {
  previousBoxSizesMap().remove(&box);
}

static LayoutRect computeRightDelta(const LayoutPoint& location,
                                    const LayoutSize& oldSize,
                                    const LayoutSize& newSize,
                                    int extraWidth) {
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
                                     int extraHeight) {
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

bool BoxPaintInvalidator::incrementallyInvalidatePaint() {
  const LayoutRect& oldRect = m_context.oldBounds.rect;
  const LayoutRect& newRect = m_context.newBounds.rect;
  DCHECK(oldRect.location() == newRect.location());
  LayoutRect rightDelta =
      computeRightDelta(newRect.location(), oldRect.size(), newRect.size(), 0);
  LayoutRect bottomDelta =
      computeBottomDelta(newRect.location(), oldRect.size(), newRect.size(), 0);

  if (m_box.styleRef().hasBorder() || m_box.styleRef().hasBackground()) {
    LayoutSize oldBorderBoxSize = computePreviousBorderBoxSize(oldRect.size());
    LayoutSize newBorderBoxSize = m_box.size();
    DCHECK(m_context.oldLocation == m_context.newLocation);
    rightDelta.unite(computeRightDelta(m_context.newLocation, oldBorderBoxSize,
                                       newBorderBoxSize, m_box.borderRight()));
    bottomDelta.unite(computeBottomDelta(m_context.newLocation,
                                         oldBorderBoxSize, newBorderBoxSize,
                                         m_box.borderBottom()));
  }

  if (rightDelta.isEmpty() && bottomDelta.isEmpty())
    return false;

  invalidatePaintRectClippedByOldAndNewBounds(rightDelta);
  invalidatePaintRectClippedByOldAndNewBounds(bottomDelta);
  return true;
}

void BoxPaintInvalidator::invalidatePaintRectClippedByOldAndNewBounds(
    const LayoutRect& rect) {
  if (rect.isEmpty())
    return;

  ObjectPaintInvalidator objectPaintInvalidator(m_box);
  LayoutRect rectClippedByOldBounds =
      intersection(rect, m_context.oldBounds.rect);
  LayoutRect rectClippedByNewBounds =
      intersection(rect, m_context.newBounds.rect);
  // Invalidate only once if the clipped rects equal.
  if (rectClippedByOldBounds == rectClippedByNewBounds) {
    objectPaintInvalidator.invalidatePaintUsingContainer(
        *m_context.paintInvalidationContainer, rectClippedByOldBounds,
        PaintInvalidationIncremental);
    return;
  }
  // Invalidate the bigger one if one contains another. Otherwise invalidate
  // both.
  if (!rectClippedByNewBounds.contains(rectClippedByOldBounds))
    objectPaintInvalidator.invalidatePaintUsingContainer(
        *m_context.paintInvalidationContainer, rectClippedByOldBounds,
        PaintInvalidationIncremental);
  if (!rectClippedByOldBounds.contains(rectClippedByNewBounds))
    objectPaintInvalidator.invalidatePaintUsingContainer(
        *m_context.paintInvalidationContainer, rectClippedByNewBounds,
        PaintInvalidationIncremental);
}

PaintInvalidationReason BoxPaintInvalidator::computePaintInvalidationReason() {
  PaintInvalidationReason reason =
      ObjectPaintInvalidatorWithContext(m_box, m_context)
          .computePaintInvalidationReason();

  if (isImmediateFullPaintInvalidationReason(reason) ||
      reason == PaintInvalidationNone)
    return reason;

  if (m_box.mayNeedPaintInvalidationAnimatedBackgroundImage() &&
      !m_box.backgroundIsKnownToBeObscured())
    reason = PaintInvalidationDelayedFull;

  // If the current paint invalidation reason is PaintInvalidationDelayedFull,
  // then this paint invalidation can delayed if the LayoutBox in question is
  // not on-screen. The logic to decide whether this is appropriate exists at
  // the site of the original paint invalidation that chose
  // PaintInvalidationDelayedFull.
  if (reason == PaintInvalidationDelayedFull) {
    // Do regular full paint invalidation if the object is onscreen.
    return m_box.intersectsVisibleViewport() ? PaintInvalidationFull
                                             : PaintInvalidationDelayedFull;
  }

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

  if (reason == PaintInvalidationIncremental &&
      m_context.oldBounds.rect != m_context.newBounds.rect) {
    if (m_context.newBounds.coversExtraPixels ||
        m_context.oldBounds.coversExtraPixels) {
      // Incremental invalidation is not applicable because the difference
      // between oldBounds and newBounds may not cover all changed pixels along
      // the edges.
      return PaintInvalidationBoundsChange;
    }

    // If the transform is not identity or translation, incremental invalidation
    // is not applicable because the difference between oldBounds and newBounds
    // doesn't cover all area needing invalidation.
    // TODO(crbug.com/426111): Should also consider ancestor transforms
    // since paintInvalidationContainer. Combine this logic into the above
    // boundsCoversExtraPixels logic.
    if (m_context.paintInvalidationContainer != m_box && m_box.hasLayer() &&
        m_box.layer()->transform() &&
        !m_box.layer()->transform()->isIdentityOrTranslation())
      return PaintInvalidationBoundsChange;
  }

  const ComputedStyle& style = m_box.styleRef();
  if (style.backgroundLayers().thisOrNextLayersUseContentBox() ||
      style.maskLayers().thisOrNextLayersUseContentBox() ||
      style.boxSizing() == BoxSizingBorderBox) {
    if (previousBoxSizesMap().get(&m_box).contentBoxRect !=
        m_box.contentBoxRect())
      return PaintInvalidationContentBoxChange;
  }

  if (!style.hasBackground() && !style.hasBoxDecorations()) {
    if (reason == PaintInvalidationIncremental &&
        m_context.oldBounds.rect != m_context.newBounds.rect &&
        m_box.hasNonCompositedScrollbars())
      return PaintInvalidationBorderBoxChange;
    return reason;
  }

  if (style.backgroundLayers().thisOrNextLayersHaveLocalAttachment()) {
    if (previousBoxSizesMap().get(&m_box).layoutOverflowRect !=
        m_box.layoutOverflowRect())
      return PaintInvalidationLayoutOverflowBoxChange;
  }

  LayoutSize oldBorderBoxSize =
      computePreviousBorderBoxSize(m_context.oldBounds.rect.size());
  LayoutSize newBorderBoxSize = m_box.size();

  if (oldBorderBoxSize == newBorderBoxSize)
    return reason;

  // See another hasNonCompositedScrollbars() callsite above.
  if (m_box.hasNonCompositedScrollbars())
    return PaintInvalidationBorderBoxChange;

  if (style.hasVisualOverflowingEffect() || style.hasAppearance() ||
      style.hasFilterInducingProperty() || style.resize() != RESIZE_NONE)
    return PaintInvalidationBorderBoxChange;

  if (style.hasBorderRadius())
    return PaintInvalidationBorderBoxChange;

  if (oldBorderBoxSize.width() != newBorderBoxSize.width() &&
      m_box.mustInvalidateBackgroundOrBorderPaintOnWidthChange())
    return PaintInvalidationBorderBoxChange;
  if (oldBorderBoxSize.height() != newBorderBoxSize.height() &&
      m_box.mustInvalidateBackgroundOrBorderPaintOnHeightChange())
    return PaintInvalidationBorderBoxChange;

  return reason;
}

PaintInvalidationReason BoxPaintInvalidator::invalidatePaintIfNeeded() {
  PaintInvalidationReason reason = computePaintInvalidationReason();
  if (reason == PaintInvalidationIncremental) {
    if (incrementallyInvalidatePaint()) {
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
  savePreviousBoxSizesIfNeeded();

  return reason;
}

bool BoxPaintInvalidator::needsToSavePreviousBoxSizes() {
  LayoutSize paintInvalidationSize = m_context.newBounds.rect.size();
  // Don't save old box sizes if the paint rect is empty because we'll
  // full invalidate once the paint rect becomes non-empty.
  if (paintInvalidationSize.isEmpty())
    return false;

  const ComputedStyle& style = m_box.styleRef();

  // If we use border-box sizing we need to track changes in the size of the
  // content box.
  if (style.boxSizing() == BoxSizingBorderBox)
    return true;

  // We need the old box sizes only when the box has background, decorations, or
  // masks.
  // Main LayoutView paints base background, thus interested in box size.
  if (!m_box.isLayoutView() && !style.hasBackground() &&
      !style.hasBoxDecorations() && !style.hasMask())
    return false;

  // No need to save old border box size if we can use size of the old paint
  // rect as the old border box size in the next invalidation.
  if (paintInvalidationSize != m_box.size())
    return true;

  // Background and mask layers can depend on other boxes than border box. See
  // crbug.com/490533
  if (style.backgroundLayers().thisOrNextLayersUseContentBox() ||
      style.backgroundLayers().thisOrNextLayersHaveLocalAttachment() ||
      style.maskLayers().thisOrNextLayersUseContentBox())
    return true;

  return false;
}

void BoxPaintInvalidator::savePreviousBoxSizesIfNeeded() {
  if (!needsToSavePreviousBoxSizes()) {
    previousBoxSizesMap().remove(&m_box);
    return;
  }

  PreviousBoxSizes sizes = {m_box.size(), m_box.contentBoxRect(),
                            m_box.layoutOverflowRect()};
  previousBoxSizesMap().set(&m_box, sizes);
}

LayoutSize BoxPaintInvalidator::computePreviousBorderBoxSize(
    const LayoutSize& previousBoundsSize) {
  // PreviousBorderBoxSize is only valid when there is background or box
  // decorations.
  DCHECK(m_box.styleRef().hasBackground() ||
         m_box.styleRef().hasBoxDecorations());

  auto it = previousBoxSizesMap().find(&m_box);
  if (it != previousBoxSizesMap().end())
    return it->value.borderBoxSize;

  // We didn't save the old border box size because it was the same as the size
  // of oldBounds.
  return previousBoundsSize;
}

}  // namespace blink

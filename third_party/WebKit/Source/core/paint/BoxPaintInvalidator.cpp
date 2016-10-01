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

bool BoxPaintInvalidator::incrementallyInvalidatePaint() {
  bool result = ObjectPaintInvalidatorWithContext(m_box, m_context)
                    .incrementallyInvalidatePaint();

  bool hasBoxDecorations = m_box.styleRef().hasBoxDecorations();
  if (!m_box.styleRef().hasBackground() && !hasBoxDecorations)
    return result;

  const LayoutRect& oldBounds = m_context.oldBounds;
  const LayoutRect& newBounds = m_context.newBounds;

  LayoutSize oldBorderBoxSize = computePreviousBorderBoxSize(oldBounds.size());
  LayoutSize newBorderBoxSize = m_box.size();

  // If border m_box size didn't change, ObjectPaintInvalidatorWithContext::incrementallyInvalidatePaint() is good.
  if (oldBorderBoxSize == newBorderBoxSize)
    return result;

  // If size of the paint invalidation rect equals to size of border box,
  // ObjectPaintInvalidatorWithContext::incrementallyInvalidatePaint()
  // is good for boxes having background without box decorations.
  DCHECK(
      oldBounds.location() ==
      newBounds.location());  // Otherwise we won't do incremental invalidation.
  if (!hasBoxDecorations && m_context.newLocation == newBounds.location() &&
      oldBorderBoxSize == oldBounds.size() &&
      newBorderBoxSize == newBounds.size())
    return result;

  // Invalidate the right delta part and the right border of the old or new m_box which has smaller width.
  if (LayoutUnit deltaWidth =
          (oldBorderBoxSize.width() - newBorderBoxSize.width()).abs()) {
    LayoutUnit smallerWidth =
        std::min(oldBorderBoxSize.width(), newBorderBoxSize.width());
    LayoutUnit borderTopRightRadiusWidth = valueForLength(
        m_box.styleRef().borderTopRightRadius().width(), smallerWidth);
    LayoutUnit borderBottomRightRadiusWidth = valueForLength(
        m_box.styleRef().borderBottomRightRadius().width(), smallerWidth);
    LayoutUnit borderWidth = std::max(
        LayoutUnit(m_box.borderRight()),
        std::max(borderTopRightRadiusWidth, borderBottomRightRadiusWidth));
    LayoutRect rightDeltaRect(
        m_context.newLocation.x() + smallerWidth - borderWidth,
        m_context.newLocation.y(), deltaWidth + borderWidth,
        std::max(oldBorderBoxSize.height(), newBorderBoxSize.height()));
    invalidatePaintRectClippedByOldAndNewBounds(rightDeltaRect);
  }

  // Invalidate the bottom delta part and the bottom border of the old or new m_box which has smaller height.
  if (LayoutUnit deltaHeight =
          (oldBorderBoxSize.height() - newBorderBoxSize.height()).abs()) {
    LayoutUnit smallerHeight =
        std::min(oldBorderBoxSize.height(), newBorderBoxSize.height());
    LayoutUnit borderBottomLeftRadiusHeight = valueForLength(
        m_box.styleRef().borderBottomLeftRadius().height(), smallerHeight);
    LayoutUnit borderBottomRightRadiusHeight = valueForLength(
        m_box.styleRef().borderBottomRightRadius().height(), smallerHeight);
    LayoutUnit borderHeight = std::max(
        LayoutUnit(m_box.borderBottom()),
        std::max(borderBottomLeftRadiusHeight, borderBottomRightRadiusHeight));
    LayoutRect bottomDeltaRect(
        m_context.newLocation.x(),
        m_context.newLocation.y() + smallerHeight - borderHeight,
        std::max(oldBorderBoxSize.width(), newBorderBoxSize.width()),
        deltaHeight + borderHeight);
    invalidatePaintRectClippedByOldAndNewBounds(bottomDeltaRect);
  }

  return true;
}

void BoxPaintInvalidator::invalidatePaintRectClippedByOldAndNewBounds(
    const LayoutRect& rect) {
  if (rect.isEmpty())
    return;

  ObjectPaintInvalidator objectPaintInvalidator(m_box);
  LayoutRect rectClippedByOldBounds = intersection(rect, m_context.oldBounds);
  LayoutRect rectClippedByNewBounds = intersection(rect, m_context.newBounds);
  // Invalidate only once if the clipped rects equal.
  if (rectClippedByOldBounds == rectClippedByNewBounds) {
    objectPaintInvalidator.invalidatePaintUsingContainer(
        *m_context.paintInvalidationContainer, rectClippedByOldBounds,
        PaintInvalidationIncremental);
    return;
  }
  // Invalidate the bigger one if one contains another. Otherwise invalidate both.
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

  // If the current paint invalidation reason is PaintInvalidationDelayedFull, then this paint invalidation can delayed if the
  // LayoutBox in question is not on-screen. The logic to decide whether this is appropriate exists at the site of the original
  // paint invalidation that chose PaintInvalidationDelayedFull.
  if (reason == PaintInvalidationDelayedFull) {
    // Do regular full paint invalidation if the object is onscreen.
    return m_box.intersectsVisibleViewport() ? PaintInvalidationFull
                                             : PaintInvalidationDelayedFull;
  }

  if (m_box.isLayoutView()) {
    const LayoutView& layoutView = toLayoutView(m_box);
    // In normal compositing mode, root background doesn't need to be invalidated for
    // box changes, because the background always covers the whole document rect
    // and clipping is done by compositor()->m_containerLayer. Also the scrollbars
    // are always composited. There are no other box decoration on the LayoutView thus
    // we can safely exit here.
    if (layoutView.usesCompositing() &&
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      return reason;
  }

  // If the transform is not identity or translation, incremental invalidation is not applicable
  // because the difference between oldBounds and newBounds doesn't cover all area needing invalidation.
  // FIXME: Should also consider ancestor transforms since paintInvalidationContainer. crbug.com/426111.
  if (reason == PaintInvalidationIncremental &&
      m_context.oldBounds != m_context.newBounds &&
      m_context.paintInvalidationContainer != m_box && m_box.hasLayer() &&
      m_box.layer()->transform() &&
      !m_box.layer()->transform()->isIdentityOrTranslation())
    return PaintInvalidationBoundsChange;

  const ComputedStyle& style = m_box.styleRef();
  if (style.backgroundLayers().thisOrNextLayersUseContentBox() ||
      style.maskLayers().thisOrNextLayersUseContentBox() ||
      style.boxSizing() == BoxSizingBorderBox) {
    if (previousBoxSizesMap().get(&m_box).contentBoxRect !=
        m_box.contentBoxRect())
      return PaintInvalidationContentBoxChange;
  }

  if (!style.hasBackground() && !style.hasBoxDecorations()) {
    // We could let incremental invalidation cover non-composited scrollbars, but just
    // do a full invalidation because incremental invalidation will go away with slimming paint.
    if (reason == PaintInvalidationIncremental &&
        m_context.oldBounds != m_context.newBounds &&
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
      computePreviousBorderBoxSize(m_context.oldBounds.size());
  LayoutSize newBorderBoxSize = m_box.size();

  if (oldBorderBoxSize == newBorderBoxSize)
    return reason;

  // See another hasNonCompositedScrollbars() callsite above.
  if (m_box.hasNonCompositedScrollbars())
    return PaintInvalidationBorderBoxChange;

  if (style.hasVisualOverflowingEffect() || style.hasAppearance() ||
      style.hasFilterInducingProperty() || style.resize() != RESIZE_NONE)
    return PaintInvalidationBorderBoxChange;

  if (style.hasBorderRadius()) {
    // If a border-radius exists and width/height is smaller than radius width/height,
    // we need to fully invalidate to cover the changed radius.
    FloatRoundedRect oldRoundedRect = style.getRoundedBorderFor(
        LayoutRect(LayoutPoint(0, 0), oldBorderBoxSize));
    FloatRoundedRect newRoundedRect = style.getRoundedBorderFor(
        LayoutRect(LayoutPoint(0, 0), newBorderBoxSize));
    if (oldRoundedRect.getRadii() != newRoundedRect.getRadii())
      return PaintInvalidationBorderBoxChange;
  }

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
    // Though we have done our own version of incremental invalidation, we still need to call
    // ObjectPaintInvalidator with PaintInvalidationNone to do any other required operations.
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
  LayoutSize paintInvalidationSize = m_context.newBounds.size();
  // Don't save old box sizes if the paint rect is empty because we'll
  // full invalidate once the paint rect becomes non-empty.
  if (paintInvalidationSize.isEmpty())
    return false;

  const ComputedStyle& style = m_box.styleRef();

  // If we use border-box sizing we need to track changes in the size of the content box.
  if (style.boxSizing() == BoxSizingBorderBox)
    return true;

  // We need the old box sizes only when the box has background, decorations, or masks.
  // Main LayoutView paints base background, thus interested in box size.
  if (!m_box.isLayoutView() && !style.hasBackground() &&
      !style.hasBoxDecorations() && !style.hasMask())
    return false;

  // No need to save old border box size if we can use size of the old paint
  // rect as the old border box size in the next invalidation.
  if (paintInvalidationSize != m_box.size())
    return true;

  // Background and mask layers can depend on other boxes than border box. See crbug.com/490533
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
  // PreviousBorderBoxSize is only valid when there is background or box decorations.
  DCHECK(m_box.styleRef().hasBackground() ||
         m_box.styleRef().hasBoxDecorations());

  auto it = previousBoxSizesMap().find(&m_box);
  if (it != previousBoxSizesMap().end())
    return it->value.borderBoxSize;

  // We didn't save the old border box size because it was the same as the size of oldBounds.
  return previousBoundsSize;
}

}  // namespace blink

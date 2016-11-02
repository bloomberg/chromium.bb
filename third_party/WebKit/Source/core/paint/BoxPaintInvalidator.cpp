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
  LayoutRect rightDelta;
  LayoutRect bottomDelta;
  if (m_box.isLayoutView()) {
    // This corresponds to the special case in computePaintInvalidationReason()
    // for LayoutView in non-rootLayerScrolling mode. In rootLayerScrolling
    // mode, we'll do full paint invalidation (see crbug.com/660156).
    DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
    DCHECK(m_context.oldVisualRect.location() ==
           m_context.newVisualRect.location());
    rightDelta = computeRightDelta(m_context.newVisualRect.location(),
                                   m_context.oldVisualRect.size(),
                                   m_context.newVisualRect.size(), 0);
    bottomDelta = computeBottomDelta(m_context.newVisualRect.location(),
                                     m_context.oldVisualRect.size(),
                                     m_context.newVisualRect.size(), 0);
  } else {
    LayoutSize oldBorderBoxSize =
        computePreviousBorderBoxSize(m_context.oldVisualRect.size());
    LayoutSize newBorderBoxSize = m_box.size();
    DCHECK(m_context.oldLocation == m_context.newLocation);
    rightDelta = computeRightDelta(m_context.newLocation, oldBorderBoxSize,
                                   newBorderBoxSize, m_box.borderRight());
    bottomDelta = computeBottomDelta(m_context.newLocation, oldBorderBoxSize,
                                     newBorderBoxSize, m_box.borderBottom());
  }

  if (rightDelta.isEmpty() && bottomDelta.isEmpty())
    return false;

  ObjectPaintInvalidator objectPaintInvalidator(m_box);
  objectPaintInvalidator.invalidatePaintUsingContainer(
      *m_context.paintInvalidationContainer, rightDelta,
      PaintInvalidationIncremental);
  objectPaintInvalidator.invalidatePaintUsingContainer(
      *m_context.paintInvalidationContainer, bottomDelta,
      PaintInvalidationIncremental);
  return true;
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

  DCHECK(reason == PaintInvalidationIncremental);

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
  if (style.backgroundLayers().thisOrNextLayersUseContentBox() ||
      style.maskLayers().thisOrNextLayersUseContentBox() ||
      style.boxSizing() == BoxSizingBorderBox) {
    if (previousBoxSizesMap().get(&m_box).contentBoxRect !=
        m_box.contentBoxRect())
      return PaintInvalidationContentBoxChange;
  }

  if (style.backgroundLayers().thisOrNextLayersHaveLocalAttachment()) {
    if (previousBoxSizesMap().get(&m_box).layoutOverflowRect !=
        m_box.layoutOverflowRect())
      return PaintInvalidationLayoutOverflowBoxChange;
  }

  LayoutSize oldBorderBoxSize =
      computePreviousBorderBoxSize(m_context.oldVisualRect.size());
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

  if (m_box.hasNonCompositedScrollbars())
    return PaintInvalidationBorderBoxChange;

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

  return PaintInvalidationIncremental;
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
  LayoutSize paintInvalidationSize = m_context.newVisualRect.size();
  // Don't save old box sizes if the paint rect is empty because we'll
  // full invalidate once the paint rect becomes non-empty.
  if (paintInvalidationSize.isEmpty())
    return false;

  if (m_box.paintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return false;

  const ComputedStyle& style = m_box.styleRef();

  // If we use border-box sizing we need to track changes in the size of the
  // content box.
  if (style.boxSizing() == BoxSizingBorderBox)
    return true;

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
  auto it = previousBoxSizesMap().find(&m_box);
  if (it != previousBoxSizesMap().end())
    return it->value.borderBoxSize;

  // We didn't save the old border box size because it was the same as the size
  // of oldVisualRect.
  return previousBoundsSize;
}

}  // namespace blink

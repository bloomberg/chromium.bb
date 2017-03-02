// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObservation.h"

#include "core/dom/ElementRareData.h"
#include "core/dom/IntersectionObserver.h"
#include "core/layout/IntersectionGeometry.h"

namespace blink {

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer,
                                                 Element& target,
                                                 bool shouldReportRootBounds)
    : m_observer(observer),
      m_target(&target),
      m_shouldReportRootBounds(shouldReportRootBounds),
      // Note that the spec says the initial value of m_lastThresholdIndex
      // should be -1, but since m_lastThresholdIndex is unsigned, we use a
      // different sentinel value.
      m_lastThresholdIndex(kMaxThresholdIndex - 1) {}

void IntersectionObservation::computeIntersectionObservations(
    DOMHighResTimeStamp timestamp) {
  DCHECK(observer());
  if (!m_target)
    return;
  Vector<Length> rootMargin(4);
  rootMargin[0] = m_observer->topMargin();
  rootMargin[1] = m_observer->rightMargin();
  rootMargin[2] = m_observer->bottomMargin();
  rootMargin[3] = m_observer->leftMargin();
  IntersectionGeometry geometry(m_observer->root(), *target(), rootMargin,
                                m_shouldReportRootBounds);
  geometry.computeGeometry();

  // Some corner cases for threshold index:
  //   - If target rect is zero area, because it has zero width and/or zero
  //     height,
  //     only two states are recognized:
  //     - 0 means not intersecting.
  //     - 1 means intersecting.
  //     No other threshold crossings are possible.
  //   - Otherwise:
  //     - If root and target do not intersect, the threshold index is 0.
  //     - If root and target intersect but the intersection has zero-area
  //       (i.e., they have a coincident edge or corner), we consider the
  //       intersection to have "crossed" a zero threshold, but not crossed
  //       any non-zero threshold.
  unsigned newThresholdIndex;
  float newVisibleRatio;
  if (geometry.doesIntersect()) {
    if (geometry.targetRect().isEmpty()) {
      newVisibleRatio = 1;
    } else {
      float intersectionArea =
          geometry.intersectionRect().size().width().toFloat() *
          geometry.intersectionRect().size().height().toFloat();
      float targetArea = geometry.targetRect().size().width().toFloat() *
                         geometry.targetRect().size().height().toFloat();
      newVisibleRatio = intersectionArea / targetArea;
    }
    newThresholdIndex = observer()->firstThresholdGreaterThan(newVisibleRatio);
  } else {
    newVisibleRatio = 0;
    newThresholdIndex = 0;
  }

  // TODO(tkent): We can't use CHECK_LT due to a compile error.
  CHECK(newThresholdIndex < kMaxThresholdIndex);

  if (m_lastThresholdIndex != newThresholdIndex) {
    IntRect snappedRootBounds = geometry.rootIntRect();
    IntRect* rootBoundsPointer =
        m_shouldReportRootBounds ? &snappedRootBounds : nullptr;
    IntersectionObserverEntry* newEntry = new IntersectionObserverEntry(
        timestamp, newVisibleRatio, geometry.targetIntRect(), rootBoundsPointer,
        geometry.intersectionIntRect(), geometry.doesIntersect(), target());
    observer()->enqueueIntersectionObserverEntry(*newEntry);
    setLastThresholdIndex(newThresholdIndex);
  }
}

void IntersectionObservation::disconnect() {
  DCHECK(observer());
  if (m_target)
    target()->ensureIntersectionObserverData().removeObservation(*observer());
  m_observer.clear();
}

DEFINE_TRACE(IntersectionObservation) {
  visitor->trace(m_observer);
  visitor->trace(m_target);
}

}  // namespace blink

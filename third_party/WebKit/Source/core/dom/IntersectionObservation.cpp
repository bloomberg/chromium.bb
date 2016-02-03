// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObservation.h"

#include "core/dom/ElementRareData.h"
#include "core/dom/IntersectionObserver.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer, Element& target, bool shouldReportRootBounds)
    : m_observer(observer)
    , m_target(target.ensureIntersectionObserverData().createWeakPtr(&target))
    , m_active(true)
    , m_shouldReportRootBounds(shouldReportRootBounds)
    , m_lastThresholdIndex(0)
{
}

Element* IntersectionObservation::target() const
{
    return toElement(m_target.get());
}

void IntersectionObservation::initializeGeometry(IntersectionGeometry& geometry)
{
    ASSERT(m_target);
    LayoutObject* targetLayoutObject = target()->layoutObject();
    if (targetLayoutObject->isBoxModelObject())
        geometry.targetRect = toLayoutBoxModelObject(targetLayoutObject)->visualOverflowRect();
    else
        geometry.targetRect = toLayoutText(targetLayoutObject)->visualOverflowRect();
    if (!geometry.targetRect.size().width())
        geometry.targetRect.setWidth(LayoutUnit(1));
    if (!geometry.targetRect.size().height())
        geometry.targetRect.setHeight(LayoutUnit(1));
    geometry.intersectionRect = geometry.targetRect;
}

void IntersectionObservation::clipToRoot(LayoutRect& rect)
{
    // Map and clip rect into root element coordinates.
    // TODO(szager): the writing mode flipping needs a test.
    ASSERT(m_target);
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    LayoutObject* targetLayoutObject = target()->layoutObject();
    targetLayoutObject->mapToVisibleRectInAncestorSpace(toLayoutBoxModelObject(rootLayoutObject), rect, nullptr);
    if (rootLayoutObject->hasOverflowClip()) {
        LayoutBox* rootLayoutBox = toLayoutBox(rootLayoutObject);
        LayoutRect clipRect(LayoutPoint(), LayoutSize(rootLayoutBox->layer()->size()));
        m_observer->applyRootMargin(clipRect);
        rootLayoutBox->flipForWritingMode(rect);
        rect.intersect(clipRect);
        rootLayoutBox->flipForWritingMode(rect);
    }
}

void IntersectionObservation::clipToFrameView(IntersectionGeometry& geometry)
{
    Node* rootNode = m_observer->root();
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    if (rootLayoutObject->isLayoutView()) {
        geometry.rootRect = LayoutRect(toLayoutView(rootLayoutObject)->frameView()->visibleContentRect());
        m_observer->applyRootMargin(geometry.rootRect);
        geometry.intersectionRect.intersect(geometry.rootRect);
    } else {
        if (rootLayoutObject->isBox())
            geometry.rootRect = LayoutRect(toLayoutBox(rootLayoutObject)->absoluteContentBox());
        else
            geometry.rootRect = LayoutRect(rootLayoutObject->absoluteBoundingBoxRect());
        m_observer->applyRootMargin(geometry.rootRect);
    }

    LayoutPoint scrollPosition(rootNode->document().view()->scrollPosition());
    geometry.targetRect.moveBy(-scrollPosition);
    geometry.intersectionRect.moveBy(-scrollPosition);
    geometry.rootRect.moveBy(-scrollPosition);
}

static void mapRectToDocumentCoordinates(LayoutObject& layoutObject, LayoutRect& rect)
{
    rect = LayoutRect(layoutObject.localToAbsoluteQuad(FloatQuad(FloatRect(rect)), UseTransforms | ApplyContainerFlip | TraverseDocumentBoundaries).boundingBox());
}

bool IntersectionObservation::computeGeometry(IntersectionGeometry& geometry)
{
    ASSERT(m_target);
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    LayoutObject* targetLayoutObject = target()->layoutObject();
    if (!rootLayoutObject->isBoxModelObject())
        return false;
    if (!targetLayoutObject->isBoxModelObject() && !targetLayoutObject->isText())
        return false;

    // Initialize targetRect and intersectionRect to bounds of target, in target's coordinate space.
    initializeGeometry(geometry);

    // TODO(szager): Support intersection observations for zero-area targets.  For now, we just
    // punt on the observation.
    if (!geometry.targetRect.size().width() || !geometry.targetRect.size().height())
        return false;

    // Clip intersectionRect to the root, and map it to root coordinates.
    clipToRoot(geometry.intersectionRect);

    // Map targetRect into document coordinates.
    mapRectToDocumentCoordinates(*targetLayoutObject, geometry.targetRect);

    // Map intersectionRect into document coordinates.
    mapRectToDocumentCoordinates(*rootLayoutObject, geometry.intersectionRect);

    // Clip intersectionRect to FrameView visible area if necessary, and map all geometry to frame coordinates.
    clipToFrameView(geometry);

    if (geometry.intersectionRect.size().isZero())
        geometry.intersectionRect = LayoutRect();

    return true;
}

void IntersectionObservation::computeIntersectionObservations(DOMHighResTimeStamp timestamp)
{
    // Pre-oilpan, there will be a delay between the time when the target Element gets deleted
    // (because its ref count dropped to zero) and when this IntersectionObservation gets
    // deleted (during the next gc run, because the target Element is the only thing keeping
    // the IntersectionObservation alive).  During that interval, we need to check that m_target
    // hasn't been cleared.
    Element* targetElement = target();
    if (!targetElement || !isActive())
        return;
    LayoutObject* targetLayoutObject = targetElement->layoutObject();
    // TODO(szager): Support SVG
    if (!targetLayoutObject || (!targetLayoutObject->isBox() && !targetLayoutObject->isInline()))
        return;

    IntersectionGeometry geometry;
    if (!computeGeometry(geometry))
        return;

    float intersectionArea = geometry.intersectionRect.size().width().toFloat() * geometry.intersectionRect.size().height().toFloat();
    float targetArea = geometry.targetRect.size().width().toFloat() * geometry.targetRect.size().height().toFloat();
    if (!targetArea)
        return;
    float newVisibleRatio = intersectionArea / targetArea;
    unsigned newThresholdIndex = observer().firstThresholdGreaterThan(newVisibleRatio);
    IntRect snappedRootBounds = pixelSnappedIntRect(geometry.rootRect);
    IntRect* rootBoundsPointer = m_shouldReportRootBounds ? &snappedRootBounds : nullptr;
    if (m_lastThresholdIndex != newThresholdIndex) {
        IntersectionObserverEntry* newEntry = new IntersectionObserverEntry(
            timestamp,
            pixelSnappedIntRect(geometry.targetRect),
            rootBoundsPointer,
            pixelSnappedIntRect(geometry.intersectionRect),
            targetElement);
        observer().enqueueIntersectionObserverEntry(*newEntry);
    }
    setLastThresholdIndex(newThresholdIndex);
}

void IntersectionObservation::disconnect()
{
    IntersectionObserver* observer = m_observer;
    clearRootAndRemoveFromTarget();
    observer->removeObservation(*this);
}

void IntersectionObservation::clearRootAndRemoveFromTarget()
{
    if (m_target)
        target()->ensureIntersectionObserverData().removeObservation(observer());
    m_observer.clear();
}

DEFINE_TRACE(IntersectionObservation)
{
    visitor->trace(m_observer);
    visitor->trace(m_target);
}

} // namespace blink

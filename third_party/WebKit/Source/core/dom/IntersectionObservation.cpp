// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObservation.h"

#include "core/dom/ElementRareData.h"
#include "core/dom/IntersectionObserver.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer, Element& target, bool shouldReportRootBounds)
    : m_observer(observer)
    , m_target(target.ensureIntersectionObserverData().createWeakPtr(&target))
    , m_shouldReportRootBounds(shouldReportRootBounds)
    , m_lastThresholdIndex(0)
{
}

Element* IntersectionObservation::target() const
{
    return toElement(m_target.get());
}

void IntersectionObservation::applyRootMargin(LayoutRect& rect) const
{
    if (m_shouldReportRootBounds)
        m_observer->applyRootMargin(rect);
}

void IntersectionObservation::initializeTargetRect(LayoutRect& rect) const
{
    ASSERT(m_target);
    LayoutObject* targetLayoutObject = target()->layoutObject();
    ASSERT(targetLayoutObject && targetLayoutObject->isBoxModelObject());
    rect = toLayoutBoxModelObject(targetLayoutObject)->visualOverflowRect();

    // TODO(szager): Properly support intersection observations for zero-area targets
    //   by using edge-inclusive geometry.
    if (!rect.size().width())
        rect.setWidth(LayoutUnit(1));
    if (!rect.size().height())
        rect.setHeight(LayoutUnit(1));
}

void IntersectionObservation::initializeRootRect(LayoutRect& rect) const
{
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    if (rootLayoutObject->isLayoutView())
        rect = LayoutRect(toLayoutView(rootLayoutObject)->frameView()->visibleContentRect());
    // TODO(szager): Obey the spec -- use content box for a scrolling element, border box otherwise.
    else if (rootLayoutObject->isBox())
        rect = LayoutRect(toLayoutBox(rootLayoutObject)->contentBoxRect());
    else
        rect = LayoutRect(toLayoutBoxModelObject(rootLayoutObject)->borderBoundingBox());
    applyRootMargin(rect);
}

void IntersectionObservation::clipToRoot(LayoutRect& rect, const LayoutRect& rootRect) const
{
    // Map and clip rect into root element coordinates.
    // TODO(szager): the writing mode flipping needs a test.
    ASSERT(m_target);
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    LayoutObject* targetLayoutObject = target()->layoutObject();

    targetLayoutObject->mapToVisibleRectInAncestorSpace(toLayoutBoxModelObject(rootLayoutObject), rect, nullptr);
    LayoutRect rootClipRect(rootRect);
    toLayoutBox(rootLayoutObject)->flipForWritingMode(rootClipRect);
    rect.intersect(rootClipRect);
}

static void mapRectUpToDocument(LayoutRect& rect, const LayoutObject& layoutObject, const Document& document)
{
    FloatQuad mappedQuad = layoutObject.localToAbsoluteQuad(
        FloatQuad(FloatRect(rect)),
        UseTransforms | ApplyContainerFlip);
    rect = LayoutRect(mappedQuad.boundingBox());
}

static void mapRectDownToDocument(LayoutRect& rect, LayoutBoxModelObject& layoutObject, const Document& document)
{
    FloatQuad mappedQuad = document.layoutView()->ancestorToLocalQuad(
        &layoutObject,
        FloatQuad(FloatRect(rect)),
        UseTransforms | ApplyContainerFlip | TraverseDocumentBoundaries);
    rect = LayoutRect(mappedQuad.boundingBox());
}

void IntersectionObservation::mapTargetRectToTargetFrameCoordinates(LayoutRect& rect) const
{
    LayoutObject& targetLayoutObject = *target()->layoutObject();
    Document& targetDocument = target()->document();
    LayoutSize scrollPosition = LayoutSize(toIntSize(targetDocument.view()->scrollPosition()));
    mapRectUpToDocument(rect, targetLayoutObject, targetDocument);
    rect.move(-scrollPosition);
}

void IntersectionObservation::mapRootRectToRootFrameCoordinates(LayoutRect& rect) const
{
    LayoutObject& rootLayoutObject = *m_observer->rootLayoutObject();
    Document& rootDocument = rootLayoutObject.document();
    LayoutSize scrollPosition = LayoutSize(toIntSize(rootDocument.view()->scrollPosition()));
    mapRectUpToDocument(rect, rootLayoutObject, rootLayoutObject.document());
    rect.move(-scrollPosition);
}

void IntersectionObservation::mapRootRectToTargetFrameCoordinates(LayoutRect& rect) const
{
    LayoutObject& rootLayoutObject = *m_observer->rootLayoutObject();
    Document& targetDocument = target()->document();
    LayoutSize scrollPosition = LayoutSize(toIntSize(targetDocument.view()->scrollPosition()));

    if (&targetDocument == &rootLayoutObject.document())
        mapRectUpToDocument(rect, rootLayoutObject, targetDocument);
    else
        mapRectDownToDocument(rect, toLayoutBoxModelObject(rootLayoutObject), targetDocument);

    rect.move(-scrollPosition);
}

static bool isContainingBlockChainDescendant(LayoutObject* descendant, LayoutObject* ancestor)
{
    LocalFrame* ancestorFrame = ancestor->document().frame();
    LocalFrame* descendantFrame = descendant->document().frame();

    if (ancestor->isLayoutView())
        return descendantFrame && descendantFrame->tree().top() == ancestorFrame;

    if (ancestorFrame != descendantFrame)
        return false;

    while (descendant && descendant != ancestor)
        descendant = descendant->containingBlock();
    return descendant;
}

bool IntersectionObservation::computeGeometry(IntersectionGeometry& geometry) const
{
    // Pre-oilpan, there will be a delay between the time when the target Element gets deleted
    // (because its ref count dropped to zero) and when this IntersectionObservation gets
    // deleted (during the next gc run, because the target Element is the only thing keeping
    // the IntersectionObservation alive).  During that interval, we need to check that m_target
    // hasn't been cleared.
    Element* targetElement = target();
    if (!targetElement || !targetElement->inDocument())
        return false;
    LayoutObject* targetLayoutObject = targetElement->layoutObject();
    ASSERT(m_observer);
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    // TODO(szager): Support SVG
    if (!targetLayoutObject)
        return false;
    if (!targetLayoutObject->isBoxModelObject() && !targetLayoutObject->isText())
        return false;
    if (!rootLayoutObject || !rootLayoutObject->isBoxModelObject())
        return false;
    if (!isContainingBlockChainDescendant(targetLayoutObject, rootLayoutObject))
        return false;

    initializeTargetRect(geometry.targetRect);
    geometry.intersectionRect = geometry.targetRect;
    initializeRootRect(geometry.rootRect);

    clipToRoot(geometry.intersectionRect, geometry.rootRect);

    // TODO(szager): there are some simple optimizations that can be done here:
    //   - Don't transform rootRect if it's not going to be reported
    //   - Don't transform intersectionRect if it's empty
    mapTargetRectToTargetFrameCoordinates(geometry.targetRect);
    mapRootRectToTargetFrameCoordinates(geometry.intersectionRect);
    mapRootRectToRootFrameCoordinates(geometry.rootRect);

    if (geometry.intersectionRect.size().isZero())
        geometry.intersectionRect = LayoutRect();

    return true;
}

void IntersectionObservation::computeIntersectionObservations(DOMHighResTimeStamp timestamp)
{
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
            target());
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

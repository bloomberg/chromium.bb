// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/dom/ElementIntersectionObserverData.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/IntersectionObserverCallback.h"
#include "core/dom/IntersectionObserverController.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "core/dom/IntersectionObserverInit.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutView.h"
#include "platform/Timer.h"
#include "wtf/MainThread.h"
#include <algorithm>

namespace blink {

static void parseThresholds(const DoubleOrDoubleArray& thresholdParameter, Vector<float>& thresholds, ExceptionState& exceptionState)
{
    if (thresholdParameter.isDouble()) {
        thresholds.append(static_cast<float>(thresholdParameter.getAsDouble()));
    } else {
        for (auto thresholdValue : thresholdParameter.getAsDoubleArray())
            thresholds.append(static_cast<float>(thresholdValue));
    }

    for (auto thresholdValue : thresholds) {
        if (thresholdValue < 0.0 || thresholdValue > 1.0) {
            exceptionState.throwTypeError("Threshold values must be between 0 and 1");
            break;
        }
    }

    std::sort(thresholds.begin(), thresholds.end());
}

IntersectionObserver* IntersectionObserver::create(const IntersectionObserverInit& observerInit, IntersectionObserverCallback& callback, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<Element> root = observerInit.root();
    if (!root) {
        // TODO(szager): Use Document instead of document element for implicit root. (crbug.com/570538)
        ExecutionContext* context = callback.executionContext();
        ASSERT(context->isDocument());
        Frame* mainFrame = toDocument(context)->frame()->tree().top();
        if (mainFrame && mainFrame->isLocalFrame())
            root = toLocalFrame(mainFrame)->document()->documentElement();
    }
    if (!root) {
        exceptionState.throwDOMException(HierarchyRequestError, "Unable to get root element in main frame to track.");
        return nullptr;
    }

    Vector<float> thresholds;
    if (observerInit.hasThreshold())
        parseThresholds(observerInit.threshold(), thresholds, exceptionState);
    else
        thresholds.append(0);
    if (exceptionState.hadException())
        return nullptr;

    return new IntersectionObserver(callback, *root, thresholds);
}

IntersectionObserver::IntersectionObserver(IntersectionObserverCallback& callback, Element& root, const Vector<float>& thresholds)
    : m_callback(&callback)
    , m_root(root.ensureIntersectionObserverData().createWeakPtr(&root))
    , m_thresholds(thresholds)
{
    root.document().ensureIntersectionObserverController().addTrackedObserver(*this);
}

LayoutObject* IntersectionObserver::rootLayoutObject()
{
    Element* rootElement = root();
    if (rootElement == rootElement->document().documentElement())
        return rootElement->document().layoutView();
    return rootElement->layoutObject();
}

bool IntersectionObserver::isDescendantOfRoot(const Element* target) const
{
    // Is m_root an ancestor, through the DOM and frame trees, of target?
    Element* rootElement = m_root.get();
    if (!rootElement || !target || target == rootElement)
        return false;
    if (!target->inDocument() || !rootElement->inDocument())
        return false;

    Document* rootDocument = &rootElement->document();
    Document* targetDocument = &target->document();
    while (targetDocument != rootDocument) {
        target = targetDocument->ownerElement();
        if (!target)
            return false;
        targetDocument = &target->document();
    }
    return target->isDescendantOf(rootElement);
}

void IntersectionObserver::observe(Element* target, ExceptionState& exceptionState)
{
    checkRootAndDetachIfNeeded();
    if (!m_root) {
        exceptionState.throwDOMException(HierarchyRequestError, "Invalid observer: root element or containing document has been deleted.");
        return;
    }
    if (!target) {
        exceptionState.throwTypeError("Observation target must be an element.");
        return;
    }
    if (m_root.get() == target) {
        exceptionState.throwDOMException(HierarchyRequestError, "Cannot use the same element for root and target.");
        return;
    }
    if (!isDescendantOfRoot(target)) {
        exceptionState.throwDOMException(HierarchyRequestError, "Observed element must be a descendant of the observer's root element.");
        return;
    }

    bool shouldReportRootBounds = target->document().frame()->securityContext()->securityOrigin()->canAccess(root()->document().frame()->securityContext()->securityOrigin());

    if (target->ensureIntersectionObserverData().getObservationFor(*this))
        return;

    IntersectionObservation* observation = new IntersectionObservation(*this, *target, shouldReportRootBounds);
    target->ensureIntersectionObserverData().addObservation(*observation);
    m_observations.add(observation);
}

void IntersectionObserver::unobserve(Element* target, ExceptionState&)
{
    checkRootAndDetachIfNeeded();
    if (!target || !target->intersectionObserverData())
        return;
    // TODO(szager): unobserve callback
    if (IntersectionObservation* observation = target->intersectionObserverData()->getObservationFor(*this))
        observation->disconnect();
}

void IntersectionObserver::computeIntersectionObservations(double timestamp)
{
    checkRootAndDetachIfNeeded();
    if (!m_root)
        return;
    for (auto& observation : m_observations)
        observation->computeIntersectionObservations(timestamp);
}

void IntersectionObserver::disconnect()
{
    HeapVector<Member<IntersectionObservation>> observationsToDisconnect;
    copyToVector(m_observations, observationsToDisconnect);
    for (auto& observation : observationsToDisconnect)
        observation->disconnect();
    ASSERT(m_observations.isEmpty());
}

void IntersectionObserver::removeObservation(IntersectionObservation& observation)
{
    m_observations.remove(&observation);
}

HeapVector<Member<IntersectionObserverEntry>> IntersectionObserver::takeRecords()
{
    checkRootAndDetachIfNeeded();
    HeapVector<Member<IntersectionObserverEntry>> entries;
    entries.swap(m_entries);
    return entries;
}

void IntersectionObserver::enqueueIntersectionObserverEntry(IntersectionObserverEntry& entry)
{
    m_entries.append(&entry);
    toDocument(m_callback->executionContext())->ensureIntersectionObserverController().scheduleIntersectionObserverForDelivery(*this);
}

unsigned IntersectionObserver::firstThresholdGreaterThan(float ratio) const
{
    unsigned result = 0;
    while (result < m_thresholds.size() && m_thresholds[result] < ratio)
        ++result;
    return result;
}

void IntersectionObserver::deliver()
{
    checkRootAndDetachIfNeeded();

    if (m_entries.isEmpty())
        return;

    HeapVector<Member<IntersectionObserverEntry>> entries;
    entries.swap(m_entries);
    m_callback->handleEvent(entries, *this);
}

void IntersectionObserver::setActive(bool active)
{
    checkRootAndDetachIfNeeded();
    for (auto& observation : m_observations)
        observation->setActive(m_root && active && isDescendantOfRoot(observation->target()));
}

void IntersectionObserver::checkRootAndDetachIfNeeded()
{
#if ENABLE(OILPAN)
    // TODO(szager): Pre-oilpan, ElementIntersectionObserverData::dispose() will take
    // care of this cleanup.  When oilpan ships, there will be a potential leak of the
    // callback's execution context when the root goes away.  For a detailed explanation:
    //
    //   https://goo.gl/PC2Baj
    //
    // When that happens, this method should catch most potential leaks, but a complete
    // solution will still be needed, along the lines described in the above link.
    if (m_root)
        return;
    disconnect();
#endif
}

DEFINE_TRACE(IntersectionObserver)
{
    visitor->trace(m_callback);
    visitor->trace(m_root);
    visitor->trace(m_observations);
    visitor->trace(m_entries);
}

} // namespace blink

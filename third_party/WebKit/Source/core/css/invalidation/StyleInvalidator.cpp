
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/css/invalidation/StyleInvalidator.h"

#include "core/css/invalidation/InvalidationSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutObject.h"

namespace blink {

// StyleInvalidator methods are super sensitive to performance benchmarks.
// We easily get 1% regression per additional if statement on recursive
// invalidate methods.
// To minimize performance impact, we wrap trace events with a lookup of
// cached flag. The cached flag is made "static const" and is not shared
// with InvalidationSet to avoid additional GOT lookup cost.
static const unsigned char* s_tracingEnabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element, reason) \
    if (UNLIKELY(*s_tracingEnabled)) \
        TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason);

void StyleInvalidator::invalidate(Document& document)
{
    RecursionData recursionData;
    if (Element* documentElement = document.documentElement())
        invalidate(*documentElement, recursionData);
    document.clearChildNeedsStyleInvalidation();
    document.clearNeedsStyleInvalidation();
    clearPendingInvalidations();
}

void StyleInvalidator::scheduleInvalidation(PassRefPtr<InvalidationSet> invalidationSet, Element& element)
{
    ASSERT(element.inActiveDocument());
    if (element.styleChangeType() >= SubtreeStyleChange)
        return;
    if (invalidationSet->wholeSubtreeInvalid()) {
        element.setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleInvalidator));
        clearInvalidation(element);
        return;
    }
    if (invalidationSet->invalidatesSelf())
        element.setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleInvalidator));

    if (invalidationSet->isEmpty())
        return;

    InvalidationList& list = ensurePendingInvalidationList(element);
    list.append(invalidationSet);
    element.setNeedsStyleInvalidation();
}

StyleInvalidator::InvalidationList& StyleInvalidator::ensurePendingInvalidationList(Element& element)
{
    PendingInvalidationMap::AddResult addResult = m_pendingInvalidationMap.add(&element, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtr(new InvalidationList);
    return *addResult.storedValue->value;
}

void StyleInvalidator::clearInvalidation(Element& element)
{
    if (!element.needsStyleInvalidation())
        return;
    m_pendingInvalidationMap.remove(&element);
    element.clearNeedsStyleInvalidation();
}

void StyleInvalidator::clearPendingInvalidations()
{
    m_pendingInvalidationMap.clear();
}

StyleInvalidator::StyleInvalidator()
{
    s_tracingEnabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
    InvalidationSet::cacheTracingFlag();
}

StyleInvalidator::~StyleInvalidator()
{
}

void StyleInvalidator::RecursionData::pushInvalidationSet(const InvalidationSet& invalidationSet)
{
    ASSERT(!m_wholeSubtreeInvalid);
    ASSERT(!invalidationSet.wholeSubtreeInvalid());
    ASSERT(!invalidationSet.isEmpty());
    if (invalidationSet.treeBoundaryCrossing())
        m_treeBoundaryCrossing = true;
    if (invalidationSet.insertionPointCrossing())
        m_insertionPointCrossing = true;
    m_invalidationSets.append(&invalidationSet);
    m_invalidateCustomPseudo = invalidationSet.customPseudoInvalid();
}

ALWAYS_INLINE bool StyleInvalidator::RecursionData::matchesCurrentInvalidationSets(Element& element)
{
    if (m_invalidateCustomPseudo && element.shadowPseudoId() != nullAtom) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element, InvalidateCustomPseudo);
        return true;
    }

    if (m_insertionPointCrossing && element.isInsertionPoint())
        return true;

    for (const auto& invalidationSet : m_invalidationSets) {
        if (invalidationSet->invalidatesElement(element))
            return true;
    }

    return false;
}

ALWAYS_INLINE bool StyleInvalidator::checkInvalidationSetsAgainstElement(Element& element, StyleInvalidator::RecursionData& recursionData)
{
    if (element.styleChangeType() >= SubtreeStyleChange || recursionData.wholeSubtreeInvalid()) {
        recursionData.setWholeSubtreeInvalid();
        return false;
    }
    bool thisElementNeedsStyleRecalc = recursionData.matchesCurrentInvalidationSets(element);
    if (element.needsStyleInvalidation()) {
        if (InvalidationList* invalidationList = m_pendingInvalidationMap.get(&element)) {
            for (const auto& invalidationSet : *invalidationList)
                recursionData.pushInvalidationSet(*invalidationSet);
            if (UNLIKELY(*s_tracingEnabled)) {
                TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
                    "StyleInvalidatorInvalidationTracking",
                    TRACE_EVENT_SCOPE_THREAD,
                    "data", InspectorStyleInvalidatorInvalidateEvent::invalidationList(element, *invalidationList));
            }
        }
    }
    return thisElementNeedsStyleRecalc;
}

bool StyleInvalidator::invalidateChildren(Element& element, StyleInvalidator::RecursionData& recursionData)
{
    bool someChildrenNeedStyleRecalc = false;
    for (ShadowRoot* root = element.youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!recursionData.treeBoundaryCrossing() && !root->childNeedsStyleInvalidation() && !root->needsStyleInvalidation())
            continue;
        for (Element* child = ElementTraversal::firstChild(*root); child; child = ElementTraversal::nextSibling(*child)) {
            bool childRecalced = invalidate(*child, recursionData);
            someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
        }
        root->clearChildNeedsStyleInvalidation();
        root->clearNeedsStyleInvalidation();
    }
    for (Element* child = ElementTraversal::firstChild(element); child; child = ElementTraversal::nextSibling(*child)) {
        bool childRecalced = invalidate(*child, recursionData);
        someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
    }
    return someChildrenNeedStyleRecalc;
}

bool StyleInvalidator::invalidate(Element& element, StyleInvalidator::RecursionData& recursionData)
{
    RecursionCheckpoint checkpoint(&recursionData);

    bool thisElementNeedsStyleRecalc = checkInvalidationSetsAgainstElement(element, recursionData);

    bool someChildrenNeedStyleRecalc = false;
    if (recursionData.hasInvalidationSets() || element.childNeedsStyleInvalidation())
        someChildrenNeedStyleRecalc = invalidateChildren(element, recursionData);

    if (thisElementNeedsStyleRecalc) {
        ASSERT(!recursionData.wholeSubtreeInvalid());
        element.setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleInvalidator));
    } else if (recursionData.hasInvalidationSets() && someChildrenNeedStyleRecalc) {
        // Clone the ComputedStyle in order to preserve correct style sharing, if possible. Otherwise recalc style.
        if (LayoutObject* layoutObject = element.layoutObject()) {
            layoutObject->setStyleInternal(ComputedStyle::clone(layoutObject->styleRef()));
        } else {
            TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element, PreventStyleSharingForParent);
            element.setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleInvalidator));
        }
    }

    if (recursionData.insertionPointCrossing() && element.isInsertionPoint())
        element.setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleInvalidator));

    element.clearChildNeedsStyleInvalidation();
    element.clearNeedsStyleInvalidation();

    return thisElementNeedsStyleRecalc;
}

DEFINE_TRACE(StyleInvalidator)
{
#if ENABLE(OILPAN)
    visitor->trace(m_pendingInvalidationMap);
#endif
}

} // namespace blink

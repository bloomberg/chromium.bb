/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/events/EventPath.h"

#include "EventNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/FocusEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/TouchEventContext.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGUseElement.h"

namespace WebCore {

Node* EventPath::parent(Node* node)
{
    EventPath eventPath(node);
    return eventPath.size() > 1 ? eventPath[1].node() : 0;
}

EventTarget* EventPath::eventTargetRespectingTargetRules(Node* referenceNode)
{
    ASSERT(referenceNode);

    if (referenceNode->isPseudoElement())
        return referenceNode->parentNode();

    if (!referenceNode->isSVGElement() || !referenceNode->isInShadowTree())
        return referenceNode;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects.
    Node& rootNode = referenceNode->treeScope().rootNode();
    Element* shadowHostElement = rootNode.isShadowRoot() ? toShadowRoot(rootNode).host() : 0;
    // At this time, SVG nodes are not supported in non-<use> shadow trees.
    if (!shadowHostElement || !shadowHostElement->hasTagName(SVGNames::useTag))
        return referenceNode;
    SVGUseElement* useElement = toSVGUseElement(shadowHostElement);
    if (SVGElementInstance* instance = useElement->instanceForShadowTreeElement(referenceNode))
        return instance;

    return referenceNode;
}

static inline bool inTheSameScope(ShadowRoot* shadowRoot, EventTarget* target)
{
    return target->toNode() && target->toNode()->treeScope().rootNode() == shadowRoot;
}

static inline EventDispatchBehavior determineDispatchBehavior(Event* event, ShadowRoot* shadowRoot, EventTarget* target)
{
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (Element* element = FullscreenElementStack::currentFullScreenElementFrom(&target->toNode()->document())) {
        // FIXME: We assume that if the full screen element is a media element that it's
        // the video-only full screen. Both here and elsewhere. But that is probably wrong.
        if (element->isMediaElement() && shadowRoot && shadowRoot->host() == element)
            return StayInsideShadowDOM;
    }

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    const AtomicString eventType = event->type();
    if (inTheSameScope(shadowRoot, target)
        && (eventType == EventTypeNames::abort
            || eventType == EventTypeNames::change
            || eventType == EventTypeNames::error
            || eventType == EventTypeNames::load
            || eventType == EventTypeNames::reset
            || eventType == EventTypeNames::resize
            || eventType == EventTypeNames::scroll
            || eventType == EventTypeNames::select
            || eventType == EventTypeNames::selectstart))
        return StayInsideShadowDOM;

    return RetargetEvent;
}

EventPath::EventPath(Event* event)
    : m_node(0)
    , m_event(event)
{
}

EventPath::EventPath(Node* node)
    : m_node(node)
    , m_event(0)
{
    resetWith(node);
}

void EventPath::resetWith(Node* node)
{
    ASSERT(node);
    m_node = node;
    m_nodeEventContexts.clear();
    m_treeScopeEventContexts.clear();
    calculatePath();
    calculateAdjustedTargets();
    calculateAdjustedEventPath();
}

void EventPath::addNodeEventContext(Node* node)
{
    m_nodeEventContexts.append(NodeEventContext(node, eventTargetRespectingTargetRules(node)));
}

void EventPath::calculatePath()
{
    ASSERT(m_node);
    ASSERT(m_nodeEventContexts.isEmpty());
    m_node->document().updateDistributionForNodeIfNeeded(const_cast<Node*>(m_node));

    Node* current = m_node;
    addNodeEventContext(current);
    if (!m_node->inDocument())
        return;
    while (current) {
        if (current->isShadowRoot() && m_event && determineDispatchBehavior(m_event, toShadowRoot(current), m_node) == StayInsideShadowDOM)
            break;
        Vector<InsertionPoint*, 8> insertionPoints;
        collectDestinationInsertionPoints(*current, insertionPoints);
        if (!insertionPoints.isEmpty()) {
            for (size_t i = 0; i < insertionPoints.size(); ++i) {
                InsertionPoint* insertionPoint = insertionPoints[i];
                if (insertionPoint->isShadowInsertionPoint()) {
                    ShadowRoot* containingShadowRoot = insertionPoint->containingShadowRoot();
                    ASSERT(containingShadowRoot);
                    if (!containingShadowRoot->isOldest())
                        addNodeEventContext(containingShadowRoot->olderShadowRoot());
                }
                addNodeEventContext(insertionPoint);
            }
            current = insertionPoints.last();
            continue;
        }
        if (current->isShadowRoot()) {
            current = current->shadowHost();
            addNodeEventContext(current);
        } else {
            current = current->parentNode();
            if (current)
                addNodeEventContext(current);
        }
    }
}

void EventPath::calculateAdjustedEventPath()
{
    if (!RuntimeEnabledFeatures::shadowDOMEnabled())
        return;
    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TreeScopeEventContext* treeScopeEventContext = m_treeScopeEventContexts[i].get();
        Vector<RefPtr<Node> > nodes;
        nodes.reserveInitialCapacity(size());
        for (size_t i = 0; i < size(); ++i) {
            if (at(i).node()->treeScope().isInclusiveAncestorOf(treeScopeEventContext->treeScope()))
                nodes.append(at(i).node());
        }
        treeScopeEventContext->adoptEventPath(nodes);
    }
}

#ifndef NDEBUG
static inline bool movedFromOlderToYounger(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    Node& rootNode = lastTreeScope.rootNode();
    return rootNode.isShadowRoot() && toShadowRoot(rootNode).youngerShadowRoot() == currentTreeScope.rootNode();
}

static inline bool movedFromYoungerToOlder(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    Node& rootNode = lastTreeScope.rootNode();
    return rootNode.isShadowRoot() && toShadowRoot(rootNode).olderShadowRoot() == currentTreeScope.rootNode();
}
#endif

static inline bool movedFromChildToParent(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    return lastTreeScope.parentTreeScope() == &currentTreeScope;
}

static inline bool movedFromParentToChild(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    return currentTreeScope.parentTreeScope() == &lastTreeScope;
}

void EventPath::calculateAdjustedTargets()
{
    Vector<Node*, 32> targetStack;
    const TreeScope* lastTreeScope = 0;
    bool isSVGElement = at(0).node()->isSVGElement();

    typedef HashMap<const TreeScope*, RefPtr<TreeScopeEventContext> > TreeScopeEventContextMap;
    TreeScopeEventContextMap treeScopeEventContextMap;
    TreeScopeEventContext* lastSharedEventContext = 0;

    for (size_t i = 0; i < size(); ++i) {
        Node* current = at(i).node();
        TreeScope& currentTreeScope = current->treeScope();
        if (targetStack.isEmpty()) {
            targetStack.append(current);
        } else if (lastTreeScope != &currentTreeScope && !isSVGElement) {
            if (movedFromParentToChild(*lastTreeScope, currentTreeScope)) {
                targetStack.append(targetStack.last());
            } else if (movedFromChildToParent(*lastTreeScope, currentTreeScope)) {
                ASSERT(!targetStack.isEmpty());
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(current);
            } else {
                ASSERT(movedFromYoungerToOlder(*lastTreeScope, currentTreeScope) || movedFromOlderToYounger(*lastTreeScope, currentTreeScope));
                ASSERT(!targetStack.isEmpty());
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(current);
                else
                    targetStack.append(targetStack.last());
            }
        }
        if (lastTreeScope != &currentTreeScope) {
            TreeScopeEventContextMap::AddResult addResult = treeScopeEventContextMap.add(&currentTreeScope, TreeScopeEventContext::create(currentTreeScope));
            lastSharedEventContext = addResult.iterator->value.get();
            if (addResult.isNewEntry)
                lastSharedEventContext->setTarget(eventTargetRespectingTargetRules(targetStack.last()));
        }
        at(i).setTreeScopeEventContext(lastSharedEventContext);
        lastTreeScope = &currentTreeScope;
    }
    m_treeScopeEventContexts.appendRange(treeScopeEventContextMap.values().begin(), treeScopeEventContextMap.values().end());
}

void EventPath::buildRelatedNodeMap(const Node* relatedNode, RelatedTargetMap& relatedTargetMap)
{
    TreeScope* lastTreeScope = 0;
    EventPath eventPath(const_cast<Node*>(relatedNode));
    for (size_t i = 0; i < eventPath.size(); ++i) {
        TreeScope* treeScope = &eventPath[i].node()->treeScope();
        if (treeScope != lastTreeScope)
            relatedTargetMap.add(treeScope, eventPath[i].target());
        lastTreeScope = treeScope;
    }
}

EventTarget* EventPath::findRelatedNode(TreeScope* scope, RelatedTargetMap& relatedTargetMap)
{
    Vector<TreeScope*, 32> parentTreeScopes;
    EventTarget* relatedNode = 0;
    while (scope) {
        parentTreeScopes.append(scope);
        RelatedTargetMap::const_iterator found = relatedTargetMap.find(scope);
        if (found != relatedTargetMap.end()) {
            relatedNode = found->value;
            break;
        }
        scope = scope->parentTreeScope();
    }
    for (Vector<TreeScope*, 32>::iterator iter = parentTreeScopes.begin(); iter < parentTreeScopes.end(); ++iter)
        relatedTargetMap.add(*iter, relatedNode);
    return relatedNode;
}

void EventPath::adjustForRelatedTarget(Node* target, EventTarget* relatedTarget)
{
    if (!target)
        return;
    if (!relatedTarget)
        return;
    Node* relatedNode = relatedTarget->toNode();
    if (!relatedNode)
        return;
    RelatedTargetMap relatedNodeMap;
    buildRelatedNodeMap(relatedNode, relatedNodeMap);

    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TreeScopeEventContext* treeScopeEventContext = m_treeScopeEventContexts[i].get();
        treeScopeEventContext->setRelatedTarget(findRelatedNode(&treeScopeEventContext->treeScope(), relatedNodeMap));
    }

    shrinkIfNeeded(target, relatedTarget);
}

void EventPath::shrinkIfNeeded(const Node* target, const EventTarget* relatedTarget)
{
    // Synthetic mouse events can have a relatedTarget which is identical to the target.
    bool targetIsIdenticalToToRelatedTarget = (target == relatedTarget);

    for (size_t i = 0; i < size(); ++i) {
        if (targetIsIdenticalToToRelatedTarget) {
            if (target->treeScope().rootNode() == at(i).node()) {
                shrink(i + 1);
                break;
            }
        } else if (at(i).target() == at(i).relatedTarget()) {
            // Event dispatching should be stopped here.
            shrink(i);
            break;
        }
    }
}

void EventPath::adjustForTouchEvent(Node* node, TouchEvent& touchEvent)
{
    Vector<TouchList*> adjustedTouches;
    Vector<TouchList*> adjustedTargetTouches;
    Vector<TouchList*> adjustedChangedTouches;
    Vector<TreeScope*> treeScopes;

    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TouchEventContext* touchEventContext = m_treeScopeEventContexts[i]->ensureTouchEventContext();
        adjustedTouches.append(&touchEventContext->touches());
        adjustedTargetTouches.append(&touchEventContext->targetTouches());
        adjustedChangedTouches.append(&touchEventContext->changedTouches());
        treeScopes.append(&m_treeScopeEventContexts[i]->treeScope());
    }

    adjustTouchList(node, touchEvent.touches(), adjustedTouches, treeScopes);
    adjustTouchList(node, touchEvent.targetTouches(), adjustedTargetTouches, treeScopes);
    adjustTouchList(node, touchEvent.changedTouches(), adjustedChangedTouches, treeScopes);

#ifndef NDEBUG
    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TreeScope& treeScope = m_treeScopeEventContexts[i]->treeScope();
        TouchEventContext* touchEventContext = m_treeScopeEventContexts[i]->touchEventContext();
        checkReachability(treeScope, touchEventContext->touches());
        checkReachability(treeScope, touchEventContext->targetTouches());
        checkReachability(treeScope, touchEventContext->changedTouches());
    }
#endif
}

void EventPath::adjustTouchList(const Node* node, const TouchList* touchList, Vector<TouchList*> adjustedTouchList, const Vector<TreeScope*>& treeScopes)
{
    if (!touchList)
        return;
    for (size_t i = 0; i < touchList->length(); ++i) {
        const Touch& touch = *touchList->item(i);
        RelatedTargetMap relatedNodeMap;
        buildRelatedNodeMap(touch.target()->toNode(), relatedNodeMap);
        for (size_t j = 0; j < treeScopes.size(); ++j) {
            adjustedTouchList[j]->append(touch.cloneWithNewTarget(findRelatedNode(treeScopes[j], relatedNodeMap)));
        }
    }
}

#ifndef NDEBUG
void EventPath::checkReachability(TreeScope& treeScope, TouchList& touchList)
{
    for (size_t i = 0; i < touchList.length(); ++i)
        ASSERT(touchList.item(i)->target()->toNode()->treeScope().isInclusiveAncestorOf(treeScope));
}
#endif

} // namespace

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/events/EventRetargeter.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/ContainerNode.h"
#include "core/events/EventContext.h"
#include "core/events/EventPath.h"
#include "core/events/FocusEvent.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/events/MouseEvent.h"
#include "core/dom/Touch.h"
#include "core/events/TouchEvent.h"
#include "core/dom/TouchList.h"
#include "core/dom/TreeScope.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

void EventRetargeter::adjustForMouseEvent(Node* node, MouseEvent& mouseEvent)
{
    adjustForRelatedTarget(node, mouseEvent.relatedTarget(), mouseEvent.eventPath());
}

void EventRetargeter::adjustForFocusEvent(Node* node, FocusEvent& focusEvent)
{
    adjustForRelatedTarget(node, focusEvent.relatedTarget(), focusEvent.eventPath());
}

void EventRetargeter::adjustForTouchEvent(Node* node, TouchEvent& touchEvent)
{
    EventPath& eventPath = touchEvent.eventPath();
    size_t eventPathSize = eventPath.size();

    EventPathTouchLists eventPathTouches(eventPathSize);
    EventPathTouchLists eventPathTargetTouches(eventPathSize);
    EventPathTouchLists eventPathChangedTouches(eventPathSize);

    for (size_t i = 0; i < eventPathSize; ++i) {
        ASSERT(eventPath[i].isTouchEventContext());
        TouchEventContext& touchEventContext = toTouchEventContext(eventPath[i]);
        eventPathTouches[i] = touchEventContext.touches();
        eventPathTargetTouches[i] = touchEventContext.targetTouches();
        eventPathChangedTouches[i] = touchEventContext.changedTouches();
    }

    adjustTouchList(node, touchEvent.touches(), eventPath, eventPathTouches);
    adjustTouchList(node, touchEvent.targetTouches(), eventPath, eventPathTargetTouches);
    adjustTouchList(node, touchEvent.changedTouches(), eventPath, eventPathChangedTouches);
}

void EventRetargeter::adjustTouchList(const Node* node, const TouchList* touchList, const EventPath& eventPath, EventPathTouchLists& eventPathTouchLists)
{
    if (!touchList)
        return;
    size_t eventPathSize = eventPath.size();
    ASSERT(eventPathTouchLists.size() == eventPathSize);
    for (size_t i = 0; i < touchList->length(); ++i) {
        const Touch& touch = *touchList->item(i);
        AdjustedTargets adjustedNodes;
        calculateAdjustedNodes(node, touch.target()->toNode(), DoesNotStopAtBoundary, const_cast<EventPath&>(eventPath), adjustedNodes);
        ASSERT(adjustedNodes.size() == eventPathSize);
        for (size_t j = 0; j < eventPathSize; ++j)
            eventPathTouchLists[j]->append(touch.cloneWithNewTarget(adjustedNodes[j].get()));
    }
}

void EventRetargeter::adjustForRelatedTarget(const Node* node, EventTarget* relatedTarget, EventPath& eventPath)
{
    if (!node)
        return;
    if (!relatedTarget)
        return;
    Node* relatedNode = relatedTarget->toNode();
    if (!relatedNode)
        return;
    AdjustedTargets adjustedNodes;
    calculateAdjustedNodes(node, relatedNode, StopAtBoundaryIfNeeded, eventPath, adjustedNodes);
    ASSERT(adjustedNodes.size() <= eventPath.size());
    for (size_t i = 0; i < adjustedNodes.size(); ++i) {
        ASSERT(eventPath[i].isMouseOrFocusEventContext());
        MouseOrFocusEventContext& mouseOrFocusEventContext = static_cast<MouseOrFocusEventContext&>(eventPath[i]);
        mouseOrFocusEventContext.setRelatedTarget(adjustedNodes[i]);
    }
}

void EventRetargeter::calculateAdjustedNodes(const Node* node, const Node* relatedNode, EventWithRelatedTargetDispatchBehavior eventWithRelatedTargetDispatchBehavior, EventPath& eventPath, AdjustedTargets& adjustedTargets)
{
    RelatedTargetMap relatedNodeMap;
    buildRelatedNodeMap(relatedNode, relatedNodeMap);

    // Synthetic mouse events can have a relatedTarget which is identical to the target.
    bool targetIsIdenticalToToRelatedTarget = (node == relatedNode);

    TreeScope* lastTreeScope = 0;
    EventTarget* adjustedTarget = 0;

    for (size_t i = 0; i < eventPath.size(); ++i) {
        TreeScope* scope = &eventPath[i].node()->treeScope();
        if (scope == lastTreeScope) {
            // Re-use the previous adjustedRelatedTarget if treeScope does not change. Just for the performance optimization.
            adjustedTargets.append(adjustedTarget);
        } else {
            adjustedTarget = findRelatedNode(scope, relatedNodeMap);
            adjustedTargets.append(adjustedTarget);
        }
        lastTreeScope = scope;
        if (eventWithRelatedTargetDispatchBehavior == DoesNotStopAtBoundary)
            continue;
        if (targetIsIdenticalToToRelatedTarget) {
            if (node->treeScope().rootNode() == eventPath[i].node()) {
                eventPath.shrink(i + 1);
                break;
            }
        } else if (eventPath[i].target() == adjustedTarget) {
            // Event dispatching should be stopped here.
            eventPath.shrink(i);
            adjustedTargets.shrink(adjustedTargets.size() - 1);
            break;
        }
    }
}

void EventRetargeter::buildRelatedNodeMap(const Node* relatedNode, RelatedTargetMap& relatedTargetMap)
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

EventTarget* EventRetargeter::findRelatedNode(TreeScope* scope, RelatedTargetMap& relatedTargetMap)
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

}

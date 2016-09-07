// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/MouseEventManager.h"

#include "core/dom/ElementTraversal.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandlingUtil.h"

namespace blink {

namespace {

PlatformMouseEvent mouseEventWithRegion(Node* node, const PlatformMouseEvent& mouseEvent)
{
    if (!node->isElementNode())
        return mouseEvent;

    Element* element = toElement(node);
    if (!element->isInCanvasSubtree())
        return mouseEvent;

    HTMLCanvasElement* canvas = Traversal<HTMLCanvasElement>::firstAncestorOrSelf(*element);
    // In this case, the event target is canvas and mouse rerouting doesn't happen.
    if (canvas == element)
        return mouseEvent;
    String region = canvas->getIdFromControl(element);
    PlatformMouseEvent newMouseEvent = mouseEvent;
    newMouseEvent.setRegion(region);
    return newMouseEvent;
}

} // namespace

MouseEventManager::MouseEventBoundaryEventDispatcher::MouseEventBoundaryEventDispatcher(
    MouseEventManager* mouseEventManager,
    const PlatformMouseEvent* platformMouseEvent,
    EventTarget* exitedTarget)
    : m_mouseEventManager(mouseEventManager)
    , m_platformMouseEvent(platformMouseEvent)
    , m_exitedTarget(exitedTarget)
{
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchOut(
    EventTarget* target, EventTarget* relatedTarget)
{
    dispatch(target, relatedTarget, EventTypeNames::mouseout, mouseEventWithRegion(m_exitedTarget->toNode(), *m_platformMouseEvent), false);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchOver(
    EventTarget* target, EventTarget* relatedTarget)
{
    dispatch(target, relatedTarget, EventTypeNames::mouseover, *m_platformMouseEvent, false);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchLeave(
    EventTarget* target, EventTarget* relatedTarget, bool checkForListener)
{
    dispatch(target, relatedTarget, EventTypeNames::mouseleave, mouseEventWithRegion(m_exitedTarget->toNode(), *m_platformMouseEvent), checkForListener);
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatchEnter(
    EventTarget* target, EventTarget* relatedTarget, bool checkForListener)
{
    dispatch(target, relatedTarget, EventTypeNames::mouseenter, *m_platformMouseEvent, checkForListener);
}

AtomicString MouseEventManager::MouseEventBoundaryEventDispatcher::getLeaveEvent()
{
    return EventTypeNames::mouseleave;
}

AtomicString MouseEventManager::MouseEventBoundaryEventDispatcher::getEnterEvent()
{
    return EventTypeNames::mouseenter;
}

void MouseEventManager::MouseEventBoundaryEventDispatcher::dispatch(
    EventTarget* target, EventTarget* relatedTarget, const AtomicString& type,
    const PlatformMouseEvent& platformMouseEvent, bool checkForListener)
{
    m_mouseEventManager->dispatchMouseEvent(target, type, platformMouseEvent,
        relatedTarget, 0, checkForListener);
}

void MouseEventManager::sendBoundaryEvents(
    EventTarget* exitedTarget,
    EventTarget* enteredTarget,
    const PlatformMouseEvent& mousePlatformEvent)
{
    MouseEventBoundaryEventDispatcher boundaryEventDispatcher(this, &mousePlatformEvent, exitedTarget);
    boundaryEventDispatcher.sendBoundaryEvents(exitedTarget, enteredTarget);
}

WebInputEventResult MouseEventManager::dispatchMouseEvent(
    EventTarget* target,
    const AtomicString& mouseEventType,
    const PlatformMouseEvent& mouseEvent,
    EventTarget* relatedTarget,
    int detail,
    bool checkForListener)
{
    if (target && target->toNode()
        && (!checkForListener || target->hasEventListeners(mouseEventType))) {
        Node* targetNode = target->toNode();
        MouseEvent* event = MouseEvent::create(mouseEventType,
            targetNode->document().domWindow(), mouseEvent, detail,
            relatedTarget ? relatedTarget->toNode() : nullptr);
        DispatchEventResult dispatchResult = target->dispatchEvent(event);
        return EventHandlingUtil::toWebInputEventResult(dispatchResult);
    }
    return WebInputEventResult::NotHandled;
}

MouseEventManager::MouseEventManager(LocalFrame* frame)
    : m_frame(frame)
{
    clear();
}

void MouseEventManager::clear()
{
}

DEFINE_TRACE(MouseEventManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_nodeUnderMouse);
}

} // namespace blink

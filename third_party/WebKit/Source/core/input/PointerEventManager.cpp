// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/PointerEventManager.h"

#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/events/MouseEvent.h"
#include "core/input/EventHandler.h"

namespace blink {

namespace {

const AtomicString& pointerEventNameForTouchPointState(PlatformTouchPoint::TouchState state)
{
    switch (state) {
    case PlatformTouchPoint::TouchReleased:
        return EventTypeNames::pointerup;
    case PlatformTouchPoint::TouchCancelled:
        return EventTypeNames::pointercancel;
    case PlatformTouchPoint::TouchPressed:
        return EventTypeNames::pointerdown;
    case PlatformTouchPoint::TouchMoved:
        return EventTypeNames::pointermove;
    case PlatformTouchPoint::TouchStationary:
        // Fall through to default
    default:
        ASSERT_NOT_REACHED();
        return emptyAtom;
    }
}

bool isInDocument(PassRefPtrWillBeRawPtr<EventTarget> n)
{
    return n && n->toNode() && n->toNode()->inDocument();
}

WebInputEventResult dispatchMouseEvent(
    PassRefPtrWillBeRawPtr<EventTarget> prpTarget,
    const AtomicString& mouseEventType,
    const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<EventTarget> prpRelatedTarget,
    int detail = 0,
    bool checkForListener = false)
{
    RefPtrWillBeRawPtr<EventTarget> target = prpTarget;
    RefPtrWillBeRawPtr<EventTarget> relatedTarget = prpRelatedTarget;
    if (target->toNode()
        && (!checkForListener || target->hasEventListeners(mouseEventType))) {
        RefPtrWillBeRawPtr<Node> targetNode = target->toNode();
        RefPtrWillBeRawPtr<MouseEvent> event = MouseEvent::create(mouseEventType,
            targetNode->document().domWindow(), mouseEvent, detail,
            relatedTarget ? relatedTarget->toNode() : nullptr);
        DispatchEventResult dispatchResult = target->dispatchEvent(event);
        return EventHandler::toWebInputEventResult(dispatchResult);
    }
    return WebInputEventResult::NotHandled;
}

} // namespace

WebInputEventResult PointerEventManager::dispatchPointerEvent(
    PassRefPtrWillBeRawPtr<EventTarget> prpTarget,
    PassRefPtrWillBeRawPtr<PointerEvent> prpPointerEvent,
    bool checkForListener)
{
    RefPtrWillBeRawPtr<EventTarget> target = prpTarget;
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = prpPointerEvent;

    // Set whether node under pointer has received pointerover or not.
    int pointerId = pointerEvent->pointerId();
    if ((pointerEvent->type() == EventTypeNames::pointerout
        || pointerEvent->type() == EventTypeNames::pointerover)
        && m_nodeUnderPointer.contains(pointerId)) {
        RefPtrWillBeRawPtr<EventTarget> targetUnderPointer =
            m_nodeUnderPointer.get(pointerId).target;
        if (targetUnderPointer == target) {
            m_nodeUnderPointer.set(pointerId, EventTargetAttributes
                (targetUnderPointer,
                pointerEvent->type() == EventTypeNames::pointerover));
        }
    }
    if (!RuntimeEnabledFeatures::pointerEventEnabled())
        return WebInputEventResult::NotHandled;
    if (!checkForListener || target->hasEventListeners(pointerEvent->type())) {
        DispatchEventResult dispatchResult = target->dispatchEvent(pointerEvent);
        return EventHandler::toWebInputEventResult(dispatchResult);
    }
    return WebInputEventResult::NotHandled;
}

PassRefPtrWillBeRawPtr<EventTarget> PointerEventManager::getEffectiveTargetForPointerEvent(
    PassRefPtrWillBeRawPtr<EventTarget> target, int pointerId)
{
    if (EventTarget* capturingTarget = getCapturingNode(pointerId))
        return capturingTarget;
    return target;
}

// Sends node transition events (pointer|mouse)(out|leave|over|enter) to the corresponding targets
void PointerEventManager::sendNodeTransitionEvents(
    PassRefPtrWillBeRawPtr<Node> exitedNode,
    PassRefPtrWillBeRawPtr<Node> enteredNode,
    const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<AbstractView> view)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.create(EventTypeNames::mouseout, mouseEvent,
        nullptr, view);
    processPendingPointerCapture(pointerEvent, enteredNode, mouseEvent, true);

    // Pointer event type does not matter as it will be overridden in the sendNodeTransitionEvents
    sendNodeTransitionEvents(exitedNode, enteredNode, pointerEvent, mouseEvent,
        true);
}

void PointerEventManager::sendNodeTransitionEvents(
    PassRefPtrWillBeRawPtr<EventTarget> prpExitedTarget,
    PassRefPtrWillBeRawPtr<EventTarget> prpEnteredTarget,
    PassRefPtrWillBeRawPtr<PointerEvent> prpPointerEvent,
    const PlatformMouseEvent& mouseEvent, bool sendMouseEvent)
{
    RefPtrWillBeRawPtr<EventTarget> exitedTarget = prpExitedTarget;
    RefPtrWillBeRawPtr<EventTarget> enteredTarget = prpEnteredTarget;
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = prpPointerEvent;
    if (exitedTarget == enteredTarget)
        return;

    if (EventTarget* capturingTarget = getCapturingNode(pointerEvent->pointerId())) {
        if (capturingTarget == exitedTarget)
            enteredTarget = nullptr;
        else if (capturingTarget == enteredTarget)
            exitedTarget = nullptr;
        else
            return;
    }

    // Dispatch pointerout/mouseout events
    if (isInDocument(exitedTarget)) {
        dispatchPointerEvent(exitedTarget, m_pointerEventFactory.create(
            pointerEvent, EventTypeNames::pointerout, enteredTarget));
        if (sendMouseEvent) {
            dispatchMouseEvent(exitedTarget,
                EventTypeNames::mouseout, mouseEvent, enteredTarget);
        }
    }

    // Create lists of all exited/entered ancestors, locate the common ancestor
    WillBeHeapVector<RefPtrWillBeMember<Node>, 32> exitedAncestors;
    WillBeHeapVector<RefPtrWillBeMember<Node>, 32> enteredAncestors;
    if (isInDocument(exitedTarget)) {
        RefPtrWillBeRawPtr<Node> exitedNode = exitedTarget->toNode();
        exitedNode->updateDistribution();
        for (RefPtrWillBeRawPtr<Node> node = exitedNode; node; node = FlatTreeTraversal::parent(*node))
            exitedAncestors.append(node);
    }

    if (isInDocument(enteredTarget)) {
        RefPtrWillBeRawPtr<Node> enteredNode = enteredTarget->toNode();
        enteredNode->updateDistribution();
        for (RefPtrWillBeRawPtr<Node> node = enteredNode; node; node = FlatTreeTraversal::parent(*node))
            enteredAncestors.append(node);
    }

    // A note on mouseenter and mouseleave: These are non-bubbling events, and they are dispatched if there
    // is a capturing event handler on an ancestor or a normal event handler on the element itself. This special
    // handling is necessary to avoid O(n^2) capturing event handler checks.
    //
    //   Note, however, that this optimization can possibly cause some unanswered/missing/redundant mouseenter or
    // mouseleave events in certain contrived eventhandling scenarios, e.g., when:
    // - the mouseleave handler for a node sets the only capturing-mouseleave-listener in its ancestor, or
    // - DOM mods in any mouseenter/mouseleave handler changes the common ancestor of exited & entered nodes, etc.
    // We think the spec specifies a "frozen" state to avoid such corner cases (check the discussion on "candidate event
    // listeners" at http://www.w3.org/TR/uievents), but our code below preserves one such behavior from past only to
    // match Firefox and IE behavior.
    //
    // TODO(mustaq): Confirm spec conformance, double-check with other browsers.

    size_t numExitedAncestors = exitedAncestors.size();
    size_t numEnteredAncestors = enteredAncestors.size();

    size_t exitedAncestorIndex = numExitedAncestors;
    size_t enteredAncestorIndex = numEnteredAncestors;
    for (size_t i = 0; i < numExitedAncestors; i++) {
        for (size_t j = 0; j < numEnteredAncestors; j++) {
            if (exitedAncestors[i] == enteredAncestors[j]) {
                exitedAncestorIndex = i;
                enteredAncestorIndex = j;
                break;
            }
        }
        if (exitedAncestorIndex <  exitedAncestors.size())
            break;
    }

    bool exitedNodeHasCapturingAncestor = false;
    for (size_t j = 0; j < exitedAncestors.size(); j++) {
        if (exitedAncestors[j]->hasCapturingEventListeners(EventTypeNames::mouseleave)
            || (RuntimeEnabledFeatures::pointerEventEnabled()
            && exitedAncestors[j]->hasCapturingEventListeners(EventTypeNames::pointerleave)))
            exitedNodeHasCapturingAncestor = true;
    }

    // Dispatch pointerleave/mouseleave events, in child-to-parent order.
    for (size_t j = 0; j < exitedAncestorIndex; j++) {
        dispatchPointerEvent(exitedAncestors[j].get(),
            m_pointerEventFactory.create(
                pointerEvent, EventTypeNames::pointerleave, enteredTarget),
            !exitedNodeHasCapturingAncestor);
        if (sendMouseEvent) {
            dispatchMouseEvent(exitedAncestors[j].get(),
                EventTypeNames::mouseleave, mouseEvent, enteredTarget,
                0, !exitedNodeHasCapturingAncestor);
        }
    }

    // Dispatch pointerover/mouseover.
    if (isInDocument(enteredTarget)) {
        dispatchPointerEvent(enteredTarget, m_pointerEventFactory.create(
            pointerEvent, EventTypeNames::pointerover, exitedTarget));
        if (sendMouseEvent) {
            dispatchMouseEvent(enteredTarget,
                EventTypeNames::mouseover, mouseEvent, exitedTarget);
        }
    }

    // Defer locating capturing pointeenter/mouseenter listener until /after/ dispatching the leave events because
    // the leave handlers might set a capturing enter handler.
    bool enteredNodeHasCapturingAncestor = false;
    for (size_t i = 0; i < enteredAncestors.size(); i++) {
        if (enteredAncestors[i]->hasCapturingEventListeners(EventTypeNames::mouseenter)
            || (RuntimeEnabledFeatures::pointerEventEnabled()
            && enteredAncestors[i]->hasCapturingEventListeners(EventTypeNames::pointerenter)))
            enteredNodeHasCapturingAncestor = true;
    }

    // Dispatch pointerenter/mouseenter events, in parent-to-child order.
    for (size_t i = enteredAncestorIndex; i > 0; i--) {
        dispatchPointerEvent(enteredAncestors[i-1].get(),
            m_pointerEventFactory.create(
                pointerEvent, EventTypeNames::pointerenter, exitedTarget),
            !enteredNodeHasCapturingAncestor);
        if (sendMouseEvent) {
            dispatchMouseEvent(enteredAncestors[i-1].get(),
                EventTypeNames::mouseenter, mouseEvent, exitedTarget,
                0, !enteredNodeHasCapturingAncestor);
        }
    }
}

void PointerEventManager::setNodeUnderPointer(
    PassRefPtrWillBeRawPtr<PointerEvent> prpPointerEvent,
    PassRefPtrWillBeRawPtr<EventTarget> prpTarget)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = prpPointerEvent;
    RefPtrWillBeRawPtr<EventTarget> target = prpTarget;
    if (m_nodeUnderPointer.contains(pointerEvent->pointerId())) {
        EventTargetAttributes node = m_nodeUnderPointer.get(
            pointerEvent->pointerId());
        if (!target) {
            m_nodeUnderPointer.remove(pointerEvent->pointerId());
        } else {
            m_nodeUnderPointer.set(pointerEvent->pointerId(),
                EventTargetAttributes(target, false));
        }
        sendNodeTransitionEvents(node.target, target, pointerEvent);
    } else if (target) {
        m_nodeUnderPointer.add(pointerEvent->pointerId(),
            EventTargetAttributes(target, false));
        sendNodeTransitionEvents(nullptr, target, pointerEvent);
    }
}

void PointerEventManager::sendTouchCancelPointerEvent(PassRefPtrWillBeRawPtr<EventTarget> target,
    const PlatformTouchPoint& point)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.createPointerCancel(point);

    processPendingPointerCapture(pointerEvent, target);

    // TODO(nzolghadr): crbug.com/579553 dealing with implicit touch capturing vs pointer event capturing
    dispatchPointerEvent(
        getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId()),
        pointerEvent.get());

    setNodeUnderPointer(pointerEvent, nullptr);

    removePointer(pointerEvent);
}

WebInputEventResult PointerEventManager::sendTouchPointerEvent(
    PassRefPtrWillBeRawPtr<EventTarget> prpTarget,
    const PlatformTouchPoint& touchPoint, PlatformEvent::Modifiers modifiers,
    const double width, const double height,
    const double clientX, const double clientY)
{
    RefPtrWillBeRawPtr<EventTarget> target = prpTarget;

    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.create(
        pointerEventNameForTouchPointState(touchPoint.state()),
        touchPoint, modifiers, width, height, clientX, clientY);

    processPendingPointerCapture(pointerEvent, target);

    setNodeUnderPointer(pointerEvent, target);

    // TODO(nzolghadr): crbug.com/579553 dealing with implicit touch capturing vs pointer event capturing
    WebInputEventResult result = dispatchPointerEvent(
        getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId()),
        pointerEvent.get());

    if (touchPoint.state() == PlatformTouchPoint::TouchReleased
        || touchPoint.state() == PlatformTouchPoint::TouchCancelled) {
        setNodeUnderPointer(pointerEvent, nullptr);
        removePointer(pointerEvent);
    }

    return result;
}

WebInputEventResult PointerEventManager::sendMousePointerEvent(
    PassRefPtrWillBeRawPtr<Node> target, const AtomicString& mouseEventType,
    int clickCount, const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<Node> relatedTarget,
    PassRefPtrWillBeRawPtr<AbstractView> view)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.create(mouseEventType, mouseEvent,
        relatedTarget, view);

    processPendingPointerCapture(pointerEvent, target, mouseEvent, true);

    // TODO(crbug.com/587955): We should call setNodeUnderPointer here but it causes sending
    // transition events that should be first removed from EventHandler.

    RefPtrWillBeRawPtr<EventTarget> effectiveTarget =
        getEffectiveTargetForPointerEvent(target, pointerEvent->pointerId());

    WebInputEventResult result =
        dispatchPointerEvent(effectiveTarget, pointerEvent);

    if (result != WebInputEventResult::NotHandled
        && pointerEvent->type() == EventTypeNames::pointerdown)
        m_preventMouseEventForPointerTypeMouse = true;

    if (!m_preventMouseEventForPointerTypeMouse) {
        result = EventHandler::mergeEventResult(result,
            dispatchMouseEvent(effectiveTarget, mouseEventType, mouseEvent,
            nullptr, clickCount));
    }

    if (pointerEvent->buttons() == 0)
        releasePointerCapture(pointerEvent->pointerId());

    return result;
}

PointerEventManager::PointerEventManager()
{
    clear();
}

PointerEventManager::~PointerEventManager()
{
}

void PointerEventManager::clear()
{
    m_preventMouseEventForPointerTypeMouse = false;
    m_pointerEventFactory.clear();
    m_nodeUnderPointer.clear();
}

void PointerEventManager::conditionallyEnableMouseEventForPointerTypeMouse(
    unsigned modifiers)
{

    if (MouseEvent::platformModifiersToButtons(modifiers) ==
        static_cast<unsigned short>(MouseEvent::Buttons::None))
        m_preventMouseEventForPointerTypeMouse = false;
}

void PointerEventManager::processPendingPointerCapture(
    const PassRefPtrWillBeRawPtr<PointerEvent> prpPointerEvent,
    const PassRefPtrWillBeRawPtr<EventTarget> prpHitTestTarget,
    const PlatformMouseEvent& mouseEvent, bool sendMouseEvent)
{
    // TODO(nzolghadr): Process sending got/lostpointercapture
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = prpPointerEvent;
    RefPtrWillBeRawPtr<EventTarget> hitTestTarget = prpHitTestTarget;
    int pointerId = pointerEvent->pointerId();
    bool hasPointerCaptureTarget = m_pointerCaptureTarget.contains(pointerId);
    bool hasPendingPointerCaptureTarget = m_pendingPointerCaptureTarget.contains(pointerId);
    const RefPtrWillBeRawPtr<EventTarget> pointerCaptureTarget =
        hasPointerCaptureTarget ? m_pointerCaptureTarget.get(pointerId) : nullptr;
    const RefPtrWillBeRawPtr<EventTarget> pendingPointerCaptureTarget =
        hasPendingPointerCaptureTarget ? m_pendingPointerCaptureTarget.get(pointerId) : nullptr;

    if (hasPendingPointerCaptureTarget
        && (pointerCaptureTarget != pendingPointerCaptureTarget)) {
        if (!hasPointerCaptureTarget
            && pendingPointerCaptureTarget != hitTestTarget
            && m_nodeUnderPointer.contains(pointerId)
            && m_nodeUnderPointer.get(pointerId).hasRecievedOverEvent) {
            sendNodeTransitionEvents(hitTestTarget, nullptr, pointerEvent,
                mouseEvent, sendMouseEvent);
        }
    }

    // Set pointerCaptureTarget from pendingPointerCaptureTarget. This does
    // affect the behavior of sendNodeTransitionEvents function. So the
    // ordering of the surrounding blocks of code is important.
    if (!hasPendingPointerCaptureTarget)
        m_pointerCaptureTarget.remove(pointerId);
    else
        m_pointerCaptureTarget.set(pointerId, m_pendingPointerCaptureTarget.get(pointerId));

    if (hasPointerCaptureTarget && (pointerCaptureTarget != pendingPointerCaptureTarget)) {
        if (!hasPendingPointerCaptureTarget && pointerCaptureTarget != hitTestTarget) {
            sendNodeTransitionEvents(nullptr, hitTestTarget, pointerEvent,
                mouseEvent, sendMouseEvent);
        }
    }

}

void PointerEventManager::removeTargetFromPointerCapturingMapping(
    PointerCapturingMap& map, const EventTarget* target)
{
    // We could have kept a reverse mapping to make this deletion possibly
    // faster but it adds some code complication which might not be worth of
    // the performance improvement considering there might not be a lot of
    // active pointer or pointer captures at the same time.
    PointerCapturingMap tmp = map;
    for (PointerCapturingMap::iterator it = tmp.begin(); it != tmp.end(); ++it) {
        if (it->value == target)
            map.remove(it->key);
    }
}

EventTarget* PointerEventManager::getCapturingNode(int pointerId)
{
    if (m_pointerCaptureTarget.contains(pointerId))
        return m_pointerCaptureTarget.get(pointerId);
    return nullptr;
}

void PointerEventManager::removePointer(
    const PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent)
{
    if (m_pointerEventFactory.remove(pointerEvent)) {
        int pointerId = pointerEvent->pointerId();
        m_pendingPointerCaptureTarget.remove(pointerId);
        m_pointerCaptureTarget.remove(pointerId);
        m_nodeUnderPointer.remove(pointerId);
    }
}

void PointerEventManager::elementRemoved(EventTarget* target)
{
    removeTargetFromPointerCapturingMapping(m_pointerCaptureTarget, target);
    removeTargetFromPointerCapturingMapping(m_pendingPointerCaptureTarget, target);
    // TODO(nzolghadr): Process sending lostpointercapture to the document
}

void PointerEventManager::setPointerCapture(int pointerId, EventTarget* target)
{
    if (m_pointerEventFactory.isActiveButtonsState(pointerId))
        m_pendingPointerCaptureTarget.set(pointerId, target);
}

void PointerEventManager::releasePointerCapture(int pointerId, EventTarget* target)
{
    if (m_pointerCaptureTarget.get(pointerId) == target)
        releasePointerCapture(pointerId);
}

void PointerEventManager::releasePointerCapture(int pointerId)
{
    m_pendingPointerCaptureTarget.remove(pointerId);
}

bool PointerEventManager::isActive(const int pointerId)
{
    return m_pointerEventFactory.isActive(pointerId);
}

DEFINE_TRACE(PointerEventManager)
{
#if ENABLE(OILPAN)
    visitor->trace(m_nodeUnderPointer);
    visitor->trace(m_pointerCaptureTarget);
    visitor->trace(m_pendingPointerCaptureTarget);
#endif
}


} // namespace blink

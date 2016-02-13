// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/PointerEventManager.h"

#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/events/MouseEvent.h"
#include "core/input/EventHandler.h"

namespace blink {

namespace {

const AtomicString& pointerEventNameForTouchPointState(PlatformTouchPoint::State state)
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

const AtomicString& pointerEventNameForMouseEventName(const AtomicString& mouseEventName)
{
#define RETURN_CORRESPONDING_PE_NAME(eventSuffix) \
    if (mouseEventName == EventTypeNames::mouse##eventSuffix) {\
        return EventTypeNames::pointer##eventSuffix;\
    }

    RETURN_CORRESPONDING_PE_NAME(down);
    RETURN_CORRESPONDING_PE_NAME(enter);
    RETURN_CORRESPONDING_PE_NAME(leave);
    RETURN_CORRESPONDING_PE_NAME(move);
    RETURN_CORRESPONDING_PE_NAME(out);
    RETURN_CORRESPONDING_PE_NAME(over);
    RETURN_CORRESPONDING_PE_NAME(up);

#undef RETURN_CORRESPONDING_PE_NAME

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

bool isInDocument(PassRefPtrWillBeRawPtr<EventTarget> n)
{
    return n && n->toNode() && n->toNode()->inDocument();
}

WebInputEventResult dispatchPointerEvent(
    PassRefPtrWillBeRawPtr<EventTarget> target,
    PassRefPtrWillBeRawPtr<PointerEvent> pointerevent,
    bool checkForListener = false)
{
    if (!RuntimeEnabledFeatures::pointerEventEnabled())
        return WebInputEventResult::NotHandled;
    if (!checkForListener || target->hasEventListeners(pointerevent->type())) {
        bool dispatchResult = target->dispatchEvent(pointerevent);
        return EventHandler::eventToEventResult(pointerevent, dispatchResult);
    }
    return WebInputEventResult::NotHandled;
}

WebInputEventResult dispatchMouseEvent(
    PassRefPtrWillBeRawPtr<EventTarget> target,
    const AtomicString& mouseEventType,
    const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
    int detail = 0,
    bool checkForListener = false)
{
    if (target->toNode()
        && (!checkForListener || target->hasEventListeners(mouseEventType))) {
        RefPtrWillBeRawPtr<Node> targetNode = target->toNode();
        RefPtrWillBeRawPtr<MouseEvent> event = MouseEvent::create(mouseEventType,
            targetNode->document().domWindow(), mouseEvent, detail,
            relatedTarget ? relatedTarget->toNode() : nullptr);
        bool res = target->dispatchEvent(event);
        return EventHandler::eventToEventResult(event, res);
    }
    return WebInputEventResult::NotHandled;
}

} // namespace

PassRefPtrWillBeRawPtr<Node> PointerEventManager::getEffectiveTargetForPointerEvent(
    PassRefPtrWillBeRawPtr<Node> target,
    PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent)
{
    // TODO(nzolghadr): Add APIs to set the capturing nodes and return the correct node here
    (void) pointerEvent;
    return target;
}

// Sends node transition events (pointer|mouse)(out|leave|over|enter) to the corresponding targets
void PointerEventManager::sendNodeTransitionEvents(
    PassRefPtrWillBeRawPtr<Node> exitedNode,
    PassRefPtrWillBeRawPtr<Node> enteredNode,
    const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<AbstractView> view)
{
    // Pointer event type does not matter as it will be overridden in the sendNodeTransitionEvents
    sendNodeTransitionEvents(exitedNode, enteredNode,
        m_pointerEventFactory.create(EventTypeNames::pointerout, mouseEvent, nullptr, view),
        mouseEvent, true);
}

void PointerEventManager::sendNodeTransitionEvents(
    PassRefPtrWillBeRawPtr<EventTarget> exitedTarget,
    PassRefPtrWillBeRawPtr<EventTarget> enteredTarget,
    PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent,
    const PlatformMouseEvent& mouseEvent, bool sendMouseEvent)
{
    if (exitedTarget == enteredTarget)
        return;

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
    PassRefPtrWillBeRawPtr<PointerEvent> pointerevent,
    PassRefPtrWillBeRawPtr<EventTarget> target)
{
    if (m_nodeUnderPointer.contains(pointerevent->pointerId())) {
        sendNodeTransitionEvents(m_nodeUnderPointer.get(
            pointerevent->pointerId()), target, pointerevent);
        if (!target)
            m_nodeUnderPointer.remove(pointerevent->pointerId());
        else
            m_nodeUnderPointer.set(pointerevent->pointerId(), target);
    } else if (target) {
        sendNodeTransitionEvents(nullptr, target, pointerevent);
        m_nodeUnderPointer.add(pointerevent->pointerId(), target);
    }
}

void PointerEventManager::sendTouchCancelPointerEvent(PassRefPtrWillBeRawPtr<EventTarget> target,
    const PlatformTouchPoint& point)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.createPointerCancel(point);

    // TODO(nzolghadr): crbug.com/579553 dealing with implicit touch capturing vs pointer event capturing
    target->dispatchEvent(pointerEvent.get());

    m_pointerEventFactory.remove(pointerEvent);
    setNodeUnderPointer(pointerEvent, nullptr);
}

WebInputEventResult PointerEventManager::sendTouchPointerEvent(
    PassRefPtrWillBeRawPtr<EventTarget> target,
    const PlatformTouchPoint& touchPoint, PlatformEvent::Modifiers modifiers,
    const double width, const double height,
    const double clientX, const double clientY)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.create(
        pointerEventNameForTouchPointState(touchPoint.state()),
        touchPoint, modifiers, width, height, clientX, clientY);

    setNodeUnderPointer(pointerEvent, target);

    // TODO(nzolghadr): crbug.com/579553 dealing with implicit touch capturing vs pointer event capturing
    WebInputEventResult result = dispatchPointerEvent(target, pointerEvent.get());

    if (touchPoint.state() == PlatformTouchPoint::TouchReleased
        || touchPoint.state() == PlatformTouchPoint::TouchCancelled) {
        m_pointerEventFactory.remove(pointerEvent);
        setNodeUnderPointer(pointerEvent, nullptr);
    }

    return result;
}

WebInputEventResult PointerEventManager::sendMousePointerEvent(
    PassRefPtrWillBeRawPtr<Node> target, const AtomicString& mouseEventType,
    int clickCount, const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<Node> relatedTarget,
    PassRefPtrWillBeRawPtr<AbstractView> view)
{
    AtomicString pointerEventType =
        pointerEventNameForMouseEventName(mouseEventType);
    unsigned short pointerButtonsPressed =
        MouseEvent::platformModifiersToButtons(mouseEvent.modifiers());

    // Make sure chorded buttons fire pointermove instead of pointerup/pointerdown.
    if ((pointerEventType == EventTypeNames::pointerdown && (pointerButtonsPressed & ~MouseEvent::buttonToButtons(mouseEvent.button())) != 0)
        || (pointerEventType == EventTypeNames::pointerup && pointerButtonsPressed != 0))
        pointerEventType = EventTypeNames::pointermove;

    RefPtrWillBeRawPtr<PointerEvent> pointerEvent =
        m_pointerEventFactory.create(pointerEventType, mouseEvent,
        relatedTarget, view);

    RefPtrWillBeRawPtr<Node> effectiveTarget =
        getEffectiveTargetForPointerEvent(target, pointerEvent);

    WebInputEventResult result =
        dispatchPointerEvent(effectiveTarget, pointerEvent);

    if (result != WebInputEventResult::NotHandled
        && pointerEventType == EventTypeNames::pointerdown)
        m_preventMouseEventForPointerTypeMouse = true;

    if (!m_preventMouseEventForPointerTypeMouse) {
        result = EventHandler::mergeEventResult(result,
            dispatchMouseEvent(effectiveTarget, mouseEventType, mouseEvent,
            nullptr, clickCount));
    }

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

DEFINE_TRACE(PointerEventManager)
{
#if ENABLE(OILPAN)
    visitor->trace(m_nodeUnderPointer);
#endif
}


} // namespace blink

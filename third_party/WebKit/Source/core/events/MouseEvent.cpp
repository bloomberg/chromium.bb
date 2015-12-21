/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/events/MouseEvent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Element.h"
#include "core/events/EventDispatcher.h"
#include "platform/PlatformMouseEvent.h"

namespace blink {

PassRefPtrWillBeRawPtr<MouseEvent> MouseEvent::create(ScriptState* scriptState, const AtomicString& type, const MouseEventInit& initializer)
{
    if (scriptState && scriptState->world().isIsolatedWorld())
        UIEventWithKeyState::didCreateEventInIsolatedWorld(initializer.ctrlKey(), initializer.altKey(), initializer.shiftKey(), initializer.metaKey());
    return adoptRefWillBeNoop(new MouseEvent(type, initializer));
}

PassRefPtrWillBeRawPtr<MouseEvent> MouseEvent::create(const AtomicString& eventType, PassRefPtrWillBeRawPtr<AbstractView> view, const PlatformMouseEvent& event, int detail, PassRefPtrWillBeRawPtr<Node> relatedTarget)
{
    ASSERT(event.type() == PlatformEvent::MouseMoved || event.button() != NoButton);

    bool isMouseEnterOrLeave = eventType == EventTypeNames::mouseenter || eventType == EventTypeNames::mouseleave;
    bool isCancelable = !isMouseEnterOrLeave;
    bool isBubbling = !isMouseEnterOrLeave;

    return MouseEvent::create(
        eventType, isBubbling, isCancelable, view,
        detail, event.globalPosition().x(), event.globalPosition().y(), event.position().x(), event.position().y(),
        event.movementDelta().x(), event.movementDelta().y(),
        event.modifiers(), event.button(),
        platformModifiersToButtons(event.modifiers()),
        relatedTarget, event.timestamp(), event.syntheticEventType());
}

PassRefPtrWillBeRawPtr<MouseEvent> MouseEvent::create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view,
    int detail, int screenX, int screenY, int windowX, int windowY,
    int movementX, int movementY,
    PlatformEvent::Modifiers modifiers,
    short button, unsigned short buttons,
    PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
    double platformTimeStamp,
    PlatformMouseEvent::SyntheticEventType syntheticEventType)
{
    return adoptRefWillBeNoop(new MouseEvent(type, canBubble, cancelable, view,
        detail, screenX, screenY, windowX, windowY,
        movementX, movementY,
        modifiers, button, buttons, relatedTarget, platformTimeStamp, syntheticEventType));
}

PassRefPtrWillBeRawPtr<MouseEvent> MouseEvent::create(const AtomicString& eventType, PassRefPtrWillBeRawPtr<AbstractView> view, PassRefPtrWillBeRawPtr<Event> underlyingEvent, SimulatedClickCreationScope creationScope)
{
    PlatformEvent::Modifiers modifiers = PlatformEvent::NoModifiers;
    if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
        modifiers = keyStateEvent->modifiers();
    }

    PlatformMouseEvent::SyntheticEventType syntheticType = PlatformMouseEvent::Positionless;
    int screenX = 0;
    int screenY = 0;
    if (underlyingEvent && underlyingEvent->isMouseEvent()) {
        syntheticType = PlatformMouseEvent::RealOrIndistinguishable;
        MouseEvent* mouseEvent = toMouseEvent(underlyingEvent.get());
        screenX = mouseEvent->screenLocation().x();
        screenY = mouseEvent->screenLocation().y();
    }

    double timestamp = underlyingEvent ? underlyingEvent->platformTimeStamp() : monotonicallyIncreasingTime();
    RefPtrWillBeRawPtr<MouseEvent> createdEvent = MouseEvent::create(eventType, true, true, view,
        0, screenX, screenY, 0, 0, 0, 0, modifiers, 0, 0, nullptr,
        timestamp, syntheticType);

    createdEvent->setTrusted(creationScope == SimulatedClickCreationScope::FromUserAgent);
    createdEvent->setUnderlyingEvent(underlyingEvent);
    if (syntheticType == PlatformMouseEvent::RealOrIndistinguishable) {
        MouseEvent* mouseEvent = toMouseEvent(createdEvent->underlyingEvent());
        createdEvent->initCoordinates(mouseEvent->clientLocation());
    }

    return createdEvent.release();
}

MouseEvent::MouseEvent()
    : m_button(0)
    , m_buttons(0)
    , m_relatedTarget(nullptr)
    , m_syntheticEventType(PlatformMouseEvent::RealOrIndistinguishable)
{
}

MouseEvent::MouseEvent(const AtomicString& eventType, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view,
    int detail, int screenX, int screenY, int windowX, int windowY,
    int movementX, int movementY,
    PlatformEvent::Modifiers modifiers,
    short button, unsigned short buttons,
    PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
    double platformTimeStamp,
    PlatformMouseEvent::SyntheticEventType syntheticEventType)
    : MouseRelatedEvent(eventType, canBubble, cancelable, view, detail, IntPoint(screenX, screenY),
        IntPoint(windowX, windowY), IntPoint(movementX, movementY), modifiers,
        platformTimeStamp,
        syntheticEventType == PlatformMouseEvent::Positionless ? PositionType::Positionless : PositionType::Position,
        syntheticEventType == PlatformMouseEvent::FromTouch ? InputDeviceCapabilities::firesTouchEventsSourceCapabilities() : InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities())
    , m_button(button)
    , m_buttons(buttons)
    , m_relatedTarget(relatedTarget)
    , m_syntheticEventType(syntheticEventType)
{
}

MouseEvent::MouseEvent(const AtomicString& eventType, const MouseEventInit& initializer)
    : MouseRelatedEvent(eventType, initializer)
    , m_button(initializer.button())
    , m_buttons(initializer.buttons())
    , m_relatedTarget(initializer.relatedTarget())
    , m_syntheticEventType(PlatformMouseEvent::RealOrIndistinguishable)
{
}

MouseEvent::~MouseEvent()
{
}

unsigned short MouseEvent::platformModifiersToButtons(unsigned modifiers)
{
    unsigned short buttons = 0;

    if (modifiers & PlatformEvent::LeftButtonDown)
        buttons |= static_cast<unsigned short>(Buttons::Left);
    if (modifiers & PlatformEvent::RightButtonDown)
        buttons |= static_cast<unsigned short>(Buttons::Right);
    if (modifiers & PlatformEvent::MiddleButtonDown)
        buttons |= static_cast<unsigned short>(Buttons::Middle);

    return buttons;
}

unsigned short MouseEvent::buttonToButtons(short button)
{
    switch (button) {
    case NoButton:
        return static_cast<unsigned short>(Buttons::None);
    case LeftButton:
        return static_cast<unsigned short>(Buttons::Left);
    case RightButton:
        return static_cast<unsigned short>(Buttons::Right);
    case MiddleButton:
        return static_cast<unsigned short>(Buttons::Middle);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void MouseEvent::initMouseEvent(ScriptState* scriptState, const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view,
                                int detail, int screenX, int screenY, int clientX, int clientY,
                                bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
                                short button, PassRefPtrWillBeRawPtr<EventTarget> relatedTarget, unsigned short buttons)
{
    if (dispatched())
        return;

    if (scriptState && scriptState->world().isIsolatedWorld())
        UIEventWithKeyState::didCreateEventInIsolatedWorld(ctrlKey, altKey, shiftKey, metaKey);

    initModifiers(ctrlKey, altKey, shiftKey, metaKey);
    initMouseEventInternal(type, canBubble, cancelable, view, detail, screenX, screenY, clientX, clientY, modifiers(), button, relatedTarget, nullptr, buttons);
}

void MouseEvent::initMouseEventInternal(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view,
    int detail, int screenX, int screenY, int clientX, int clientY, PlatformEvent::Modifiers modifiers,
    short button, PassRefPtrWillBeRawPtr<EventTarget> relatedTarget, InputDeviceCapabilities* sourceCapabilities, unsigned short buttons)
{
    initUIEventInternal(type, canBubble, cancelable, view, detail, sourceCapabilities);

    m_screenLocation = IntPoint(screenX, screenY);
    m_button = button;
    m_buttons = buttons;
    m_relatedTarget = relatedTarget;
    m_modifiers = modifiers;

    initCoordinates(IntPoint(clientX, clientY));

    // FIXME: SyntheticEventType is not set to RealOrIndistinguishable here.
}

const AtomicString& MouseEvent::interfaceName() const
{
    return EventNames::MouseEvent;
}

bool MouseEvent::isMouseEvent() const
{
    return true;
}

int MouseEvent::which() const
{
    // For the DOM, the return values for left, middle and right mouse buttons are 0, 1, 2, respectively.
    // For the Netscape "which" property, the return values for left, middle and right mouse buttons are 1, 2, 3, respectively.
    // So we must add 1.
    return m_button + 1;
}

Node* MouseEvent::toElement() const
{
    // MSIE extension - "the object toward which the user is moving the mouse pointer"
    if (type() == EventTypeNames::mouseout || type() == EventTypeNames::mouseleave)
        return relatedTarget() ? relatedTarget()->toNode() : nullptr;

    return target() ? target()->toNode() : nullptr;
}

Node* MouseEvent::fromElement() const
{
    // MSIE extension - "object from which activation or the mouse pointer is exiting during the event" (huh?)
    if (type() != EventTypeNames::mouseout && type() != EventTypeNames::mouseleave)
        return relatedTarget() ? relatedTarget()->toNode() : nullptr;

    return target() ? target()->toNode() : nullptr;
}

DEFINE_TRACE(MouseEvent)
{
    visitor->trace(m_relatedTarget);
    MouseRelatedEvent::trace(visitor);
}

PassRefPtrWillBeRawPtr<EventDispatchMediator> MouseEvent::createMediator()
{
    return MouseEventDispatchMediator::create(this);
}

PassRefPtrWillBeRawPtr<MouseEventDispatchMediator> MouseEventDispatchMediator::create(PassRefPtrWillBeRawPtr<MouseEvent> mouseEvent)
{
    return adoptRefWillBeNoop(new MouseEventDispatchMediator(mouseEvent));
}

MouseEventDispatchMediator::MouseEventDispatchMediator(PassRefPtrWillBeRawPtr<MouseEvent> mouseEvent)
    : EventDispatchMediator(mouseEvent)
{
}

MouseEvent& MouseEventDispatchMediator::event() const
{
    return toMouseEvent(EventDispatchMediator::event());
}

bool MouseEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    MouseEvent& mouseEvent = event();
    if (!mouseEvent.isTrusted()) {
        mouseEvent.eventPath().adjustForRelatedTarget(dispatcher.node(), mouseEvent.relatedTarget());
        return dispatcher.dispatch();
    }

    if (isDisabledFormControl(&dispatcher.node()))
        return false;

    if (mouseEvent.type().isEmpty())
        return true; // Shouldn't happen.

    ASSERT(!mouseEvent.target() || mouseEvent.target() != mouseEvent.relatedTarget());

    EventTarget* relatedTarget = mouseEvent.relatedTarget();
    mouseEvent.eventPath().adjustForRelatedTarget(dispatcher.node(), relatedTarget);

    dispatcher.dispatch();
    bool swallowEvent = mouseEvent.defaultHandled() || mouseEvent.defaultPrevented();

    if (mouseEvent.type() != EventTypeNames::click || mouseEvent.detail() != 2)
        return !swallowEvent;

    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute. This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    RefPtrWillBeRawPtr<MouseEvent> doubleClickEvent = MouseEvent::create();
    doubleClickEvent->initMouseEventInternal(EventTypeNames::dblclick, mouseEvent.bubbles(), mouseEvent.cancelable(), mouseEvent.view(),
        mouseEvent.detail(), mouseEvent.screenX(), mouseEvent.screenY(), mouseEvent.clientX(), mouseEvent.clientY(),
        mouseEvent.modifiers(), mouseEvent.button(), relatedTarget, mouseEvent.sourceCapabilities(), mouseEvent.buttons());

    // Inherit the trusted status from the original event.
    doubleClickEvent->setTrusted(mouseEvent.isTrusted());
    if (mouseEvent.defaultHandled())
        doubleClickEvent->setDefaultHandled();
    EventDispatcher::dispatchEvent(dispatcher.node(), MouseEventDispatchMediator::create(doubleClickEvent));
    if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
        return false;
    return !swallowEvent;
}

} // namespace blink

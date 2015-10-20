// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/events/PointerEvent.h"

#include "core/dom/Element.h"
#include "core/events/EventDispatcher.h"

namespace blink {

namespace {

const char* pointerTypeNameForWebPointPointerType(WebPointerProperties::PointerType type)
{
    switch (type) {
    case WebPointerProperties::PointerType::Unknown:
        return "";
    case WebPointerProperties::PointerType::Touch:
        return "touch";
    case WebPointerProperties::PointerType::Pen:
        return "pen";
    case WebPointerProperties::PointerType::Mouse:
        return "mouse";
    }
    ASSERT_NOT_REACHED();
    return "";
}

}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEvent::create(const AtomicString& type,
    const bool isPrimary, const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<Node> relatedTarget,
    PassRefPtrWillBeRawPtr<AbstractView> view)
{
    PointerEventInit pointerEventInit;

    // TODO(crbug.com/537319): Define a constant somewhere for mouse id.
    pointerEventInit.setPointerId(0);

    pointerEventInit.setPointerType(pointerTypeNameForWebPointPointerType(WebPointerProperties::PointerType::Mouse));
    pointerEventInit.setIsPrimary(true);

    pointerEventInit.setScreenX(mouseEvent.globalPosition().x());
    pointerEventInit.setScreenY(mouseEvent.globalPosition().y());
    pointerEventInit.setClientX(mouseEvent.position().x());
    pointerEventInit.setClientY(mouseEvent.position().y());

    pointerEventInit.setButton(mouseEvent.button());
    pointerEventInit.setButtons(MouseEvent::platformModifiersToButtons(mouseEvent.modifiers()));

    UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit, mouseEvent.modifiers());

    pointerEventInit.setBubbles(type != EventTypeNames::pointerenter
        && type != EventTypeNames::pointerleave);
    pointerEventInit.setCancelable(type != EventTypeNames::pointerenter
        && type != EventTypeNames::pointerleave && type != EventTypeNames::pointercancel);

    pointerEventInit.setView(view);
    if (relatedTarget)
        pointerEventInit.setRelatedTarget(relatedTarget);

    return PointerEvent::create(type, pointerEventInit);
}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEvent::create(const AtomicString& type,
    const bool isPrimary, const PlatformTouchPoint& touchPoint,
    PlatformEvent::Modifiers modifiers,
    const double width, const double height,
    const double clientX, const double clientY)
{
    const unsigned& pointerId = touchPoint.id();
    const PlatformTouchPoint::State pointState = touchPoint.state();

    bool pointerReleasedOrCancelled = pointState == PlatformTouchPoint::TouchReleased
        || pointState == PlatformTouchPoint::TouchCancelled;
    const WebPointerProperties::PointerType pointerType = touchPoint.pointerProperties().pointerType;
    const String& pointerTypeStr = pointerTypeNameForWebPointPointerType(pointerType);

    bool isEnterOrLeave = false;

    PointerEventInit pointerEventInit;
    pointerEventInit.setPointerId(pointerId);
    pointerEventInit.setWidth(width);
    pointerEventInit.setHeight(height);
    pointerEventInit.setPressure(touchPoint.force());
    pointerEventInit.setTiltX(touchPoint.pointerProperties().tiltX);
    pointerEventInit.setTiltY(touchPoint.pointerProperties().tiltY);
    pointerEventInit.setPointerType(pointerTypeStr);
    pointerEventInit.setIsPrimary(isPrimary);
    pointerEventInit.setScreenX(touchPoint.screenPos().x());
    pointerEventInit.setScreenY(touchPoint.screenPos().y());
    pointerEventInit.setClientX(clientX);
    pointerEventInit.setClientY(clientY);
    pointerEventInit.setButton(0);
    pointerEventInit.setButtons(pointerReleasedOrCancelled ? 0 : 1);

    UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit, modifiers);

    pointerEventInit.setBubbles(!isEnterOrLeave);
    pointerEventInit.setCancelable(!isEnterOrLeave && pointState != PlatformTouchPoint::TouchCancelled);

    return PointerEvent::create(type, pointerEventInit);
}

PointerEvent::PointerEvent()
    : m_pointerId(0)
    , m_width(0)
    , m_height(0)
    , m_pressure(0)
    , m_tiltX(0)
    , m_tiltY(0)
    , m_isPrimary(false)
{
}

PointerEvent::PointerEvent(const AtomicString& type, const PointerEventInit& initializer)
    : MouseEvent(type, initializer)
    , m_pointerId(0)
    , m_width(0)
    , m_height(0)
    , m_pressure(0)
    , m_tiltX(0)
    , m_tiltY(0)
    , m_isPrimary(false)
{
    if (initializer.hasPointerId())
        m_pointerId = initializer.pointerId();
    if (initializer.hasWidth())
        m_width = initializer.width();
    if (initializer.hasHeight())
        m_height = initializer.height();
    if (initializer.hasPressure())
        m_pressure = initializer.pressure();
    if (initializer.hasTiltX())
        m_tiltX = initializer.tiltX();
    if (initializer.hasTiltY())
        m_tiltY = initializer.tiltY();
    if (initializer.hasPointerType())
        m_pointerType = initializer.pointerType();
    if (initializer.hasIsPrimary())
        m_isPrimary = initializer.isPrimary();
}

bool PointerEvent::isMouseEvent() const
{
    return false;
}

bool PointerEvent::isPointerEvent() const
{
    return true;
}

PassRefPtrWillBeRawPtr<EventDispatchMediator> PointerEvent::createMediator()
{
    return PointerEventDispatchMediator::create(this);
}

DEFINE_TRACE(PointerEvent)
{
    MouseEvent::trace(visitor);
}

PassRefPtrWillBeRawPtr<PointerEventDispatchMediator> PointerEventDispatchMediator::create(PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent)
{
    return adoptRefWillBeNoop(new PointerEventDispatchMediator(pointerEvent));
}

PointerEventDispatchMediator::PointerEventDispatchMediator(PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent)
    : EventDispatchMediator(pointerEvent)
{
}

PointerEvent& PointerEventDispatchMediator::event() const
{
    return toPointerEvent(EventDispatchMediator::event());
}

bool PointerEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    if (isDisabledFormControl(&dispatcher.node()))
        return false;

    if (event().type().isEmpty())
        return true; // Shouldn't happen.

    ASSERT(!event().target() || event().target() != event().relatedTarget());

    EventTarget* relatedTarget = event().relatedTarget();
    event().eventPath().adjustForRelatedTarget(dispatcher.node(), relatedTarget);

    dispatcher.dispatch();
    return !event().defaultHandled() && !event().defaultPrevented();
}

} // namespace blink

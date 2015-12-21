/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "core/events/WheelEvent.h"

#include "core/clipboard/DataTransfer.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformWheelEvent.h"

namespace blink {

inline static unsigned convertDeltaMode(const PlatformWheelEvent& event)
{
    return event.granularity() == ScrollByPageWheelEvent ? WheelEvent::DOM_DELTA_PAGE : WheelEvent::DOM_DELTA_PIXEL;
}

PassRefPtrWillBeRawPtr<WheelEvent> WheelEvent::create(const PlatformWheelEvent& event, PassRefPtrWillBeRawPtr<AbstractView> view)
{
    return adoptRefWillBeNoop(new WheelEvent(FloatPoint(event.wheelTicksX(), event.wheelTicksY()), FloatPoint(event.deltaX(), event.deltaY()),
        convertDeltaMode(event), view, event.globalPosition(), event.position(),
        event.modifiers(),
        MouseEvent::platformModifiersToButtons(event.modifiers()), event.timestamp(),
        event.canScroll(), event.resendingPluginId(), event.hasPreciseScrollingDeltas(),
        static_cast<Event::RailsMode>(event.railsMode())));
}

WheelEvent::WheelEvent()
    : m_deltaX(0)
    , m_deltaY(0)
    , m_deltaZ(0)
    , m_deltaMode(DOM_DELTA_PIXEL)
    , m_canScroll(true)
    , m_resendingPluginId(-1)
    , m_hasPreciseScrollingDeltas(false)
    , m_railsMode(RailsModeFree)
{
}

WheelEvent::WheelEvent(const AtomicString& type, const WheelEventInit& initializer)
    : MouseEvent(type, initializer)
    , m_wheelDelta(initializer.wheelDeltaX() ? initializer.wheelDeltaX() : -initializer.deltaX(), initializer.wheelDeltaY() ? initializer.wheelDeltaY() : -initializer.deltaY())
    , m_deltaX(initializer.deltaX() ? initializer.deltaX() : -initializer.wheelDeltaX())
    , m_deltaY(initializer.deltaY() ? initializer.deltaY() : -initializer.wheelDeltaY())
    , m_deltaZ(initializer.deltaZ())
    , m_deltaMode(initializer.deltaMode())
    , m_canScroll(true)
    , m_resendingPluginId(-1)
    , m_hasPreciseScrollingDeltas(false)
    , m_railsMode(RailsModeFree)
{
}

WheelEvent::WheelEvent(const FloatPoint& wheelTicks, const FloatPoint& rawDelta, unsigned deltaMode,
    PassRefPtrWillBeRawPtr<AbstractView> view, const IntPoint& screenLocation, const IntPoint& windowLocation,
    PlatformEvent::Modifiers modifiers, unsigned short buttons, double platformTimeStamp,
    bool canScroll, int resendingPluginId, bool hasPreciseScrollingDeltas, RailsMode railsMode)
    : MouseEvent(EventTypeNames::wheel, true, true, view, 0, screenLocation.x(), screenLocation.y(),
        windowLocation.x(), windowLocation.y(), 0, 0, modifiers, 0, buttons,
        nullptr, platformTimeStamp, PlatformMouseEvent::RealOrIndistinguishable)
    , m_wheelDelta(wheelTicks.x() * TickMultiplier, wheelTicks.y() * TickMultiplier)
    , m_deltaX(-rawDelta.x())
    , m_deltaY(-rawDelta.y())
    , m_deltaZ(0)
    , m_deltaMode(deltaMode)
    , m_canScroll(canScroll)
    , m_resendingPluginId(resendingPluginId)
    , m_hasPreciseScrollingDeltas(hasPreciseScrollingDeltas)
    , m_railsMode(railsMode)
{
}

const AtomicString& WheelEvent::interfaceName() const
{
    return EventNames::WheelEvent;
}

bool WheelEvent::isMouseEvent() const
{
    return false;
}

bool WheelEvent::isWheelEvent() const
{
    return true;
}

PassRefPtrWillBeRawPtr<EventDispatchMediator> WheelEvent::createMediator()
{
    return WheelEventDispatchMediator::create(this);
}

DEFINE_TRACE(WheelEvent)
{
    MouseEvent::trace(visitor);
}

PassRefPtrWillBeRawPtr<WheelEventDispatchMediator> WheelEventDispatchMediator::create(PassRefPtrWillBeRawPtr<WheelEvent> event)
{
    return adoptRefWillBeNoop(new WheelEventDispatchMediator(event));
}

WheelEventDispatchMediator::WheelEventDispatchMediator(PassRefPtrWillBeRawPtr<WheelEvent> event)
    : EventDispatchMediator(event)
{
}

WheelEvent& WheelEventDispatchMediator::event() const
{
    return toWheelEvent(EventDispatchMediator::event());
}

bool WheelEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    return EventDispatchMediator::dispatchEvent(dispatcher) && !event().defaultHandled();
}

} // namespace blink

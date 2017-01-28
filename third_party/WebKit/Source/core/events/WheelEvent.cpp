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

namespace blink {

namespace {

unsigned convertDeltaMode(const WebMouseWheelEvent& event) {
  return event.scrollByPage ? WheelEvent::kDomDeltaPage
                            : WheelEvent::kDomDeltaPixel;
}

// Negate a long value without integer overflow.
long negateIfPossible(long value) {
  if (value == LONG_MIN)
    return value;
  return -value;
}

}  // namespace

WheelEvent* WheelEvent::create(const WebMouseWheelEvent& event,
                               AbstractView* view) {
  return new WheelEvent(event, view);
}

WheelEvent::WheelEvent()
    : m_deltaX(0), m_deltaY(0), m_deltaZ(0), m_deltaMode(kDomDeltaPixel) {}

WheelEvent::WheelEvent(const AtomicString& type,
                       const WheelEventInit& initializer)
    : MouseEvent(type, initializer),
      m_wheelDelta(initializer.wheelDeltaX() ? initializer.wheelDeltaX()
                                             : -initializer.deltaX(),
                   initializer.wheelDeltaY() ? initializer.wheelDeltaY()
                                             : -initializer.deltaY()),
      m_deltaX(initializer.deltaX()
                   ? initializer.deltaX()
                   : negateIfPossible(initializer.wheelDeltaX())),
      m_deltaY(initializer.deltaY()
                   ? initializer.deltaY()
                   : negateIfPossible(initializer.wheelDeltaY())),
      m_deltaZ(initializer.deltaZ()),
      m_deltaMode(initializer.deltaMode()) {}

WheelEvent::WheelEvent(const WebMouseWheelEvent& event, AbstractView* view)
    : MouseEvent(EventTypeNames::wheel,
                 true,
                 event.isCancelable(),
                 view,
                 event,
                 event.clickCount,
                 // TODO(zino): Should support canvas hit region because the
                 // wheel event is a kind of mouse event. Please see
                 // http://crbug.com/594075
                 String(),
                 nullptr),
      m_wheelDelta(event.wheelTicksX * TickMultiplier,
                   event.wheelTicksY * TickMultiplier),
      m_deltaX(-event.deltaXInRootFrame()),
      m_deltaY(-event.deltaYInRootFrame()),
      m_deltaZ(0),
      m_deltaMode(convertDeltaMode(event)),
      m_nativeEvent(event) {}

const AtomicString& WheelEvent::interfaceName() const {
  return EventNames::WheelEvent;
}

bool WheelEvent::isMouseEvent() const {
  return false;
}

bool WheelEvent::isWheelEvent() const {
  return true;
}

EventDispatchMediator* WheelEvent::createMediator() {
  return EventDispatchMediator::create(this);
}

DEFINE_TRACE(WheelEvent) {
  MouseEvent::trace(visitor);
}

}  // namespace blink

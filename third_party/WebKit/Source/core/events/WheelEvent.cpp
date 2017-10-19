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

unsigned ConvertDeltaMode(const WebMouseWheelEvent& event) {
  return event.scroll_by_page ? WheelEvent::kDomDeltaPage
                              : WheelEvent::kDomDeltaPixel;
}

// Negate a long value without integer overflow.
long NegateIfPossible(long value) {
  if (value == LONG_MIN)
    return value;
  return -value;
}

}  // namespace

WheelEvent* WheelEvent::Create(const WebMouseWheelEvent& event,
                               AbstractView* view) {
  return new WheelEvent(event, view);
}

WheelEvent::WheelEvent()
    : delta_x_(0), delta_y_(0), delta_z_(0), delta_mode_(kDomDeltaPixel) {}

WheelEvent::WheelEvent(const AtomicString& type,
                       const WheelEventInit& initializer)
    : MouseEvent(type, initializer),
      wheel_delta_(initializer.wheelDeltaX() ? initializer.wheelDeltaX()
                                             : -initializer.deltaX(),
                   initializer.wheelDeltaY() ? initializer.wheelDeltaY()
                                             : -initializer.deltaY()),
      delta_x_(initializer.deltaX()
                   ? initializer.deltaX()
                   : NegateIfPossible(initializer.wheelDeltaX())),
      delta_y_(initializer.deltaY()
                   ? initializer.deltaY()
                   : NegateIfPossible(initializer.wheelDeltaY())),
      delta_z_(initializer.deltaZ()),
      delta_mode_(initializer.deltaMode()) {}

WheelEvent::WheelEvent(const WebMouseWheelEvent& event, AbstractView* view)
    : MouseEvent(EventTypeNames::wheel,
                 true,
                 event.IsCancelable(),
                 view,
                 event,
                 event.click_count,
                 // TODO(zino): Should support canvas hit region because the
                 // wheel event is a kind of mouse event. Please see
                 // http://crbug.com/594075
                 String(),
                 nullptr),
      wheel_delta_(event.wheel_ticks_x * kTickMultiplier,
                   event.wheel_ticks_y * kTickMultiplier),
      delta_x_(-event.DeltaXInRootFrame()),
      delta_y_(-event.DeltaYInRootFrame()),
      delta_z_(0),
      delta_mode_(ConvertDeltaMode(event)),
      native_event_(event) {}

const AtomicString& WheelEvent::InterfaceName() const {
  return EventNames::WheelEvent;
}

bool WheelEvent::IsMouseEvent() const {
  return false;
}

bool WheelEvent::IsWheelEvent() const {
  return true;
}

EventDispatchMediator* WheelEvent::CreateMediator() {
  return EventDispatchMediator::Create(this);
}

void WheelEvent::Trace(blink::Visitor* visitor) {
  MouseEvent::Trace(visitor);
}

}  // namespace blink

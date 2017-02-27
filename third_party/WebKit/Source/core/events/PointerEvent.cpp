// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEvent.h"

#include "core/dom/Element.h"
#include "core/events/EventDispatcher.h"

namespace blink {

PointerEvent::PointerEvent(const AtomicString& type,
                           const PointerEventInit& initializer)
    : MouseEvent(type, initializer),
      m_pointerId(0),
      m_width(0),
      m_height(0),
      m_pressure(0),
      m_tiltX(0),
      m_tiltY(0),
      m_tangentialPressure(0),
      m_twist(0),
      m_isPrimary(false) {
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
  if (initializer.hasTangentialPressure())
    m_tangentialPressure = initializer.tangentialPressure();
  if (initializer.hasTwist())
    m_twist = initializer.twist();
  if (initializer.hasPointerType())
    m_pointerType = initializer.pointerType();
  if (initializer.hasIsPrimary())
    m_isPrimary = initializer.isPrimary();
  if (initializer.hasCoalescedEvents()) {
    for (auto coalescedEvent : initializer.coalescedEvents())
      m_coalescedEvents.push_back(coalescedEvent);
  }
}

bool PointerEvent::isMouseEvent() const {
  return false;
}

bool PointerEvent::isPointerEvent() const {
  return true;
}

EventDispatchMediator* PointerEvent::createMediator() {
  return PointerEventDispatchMediator::create(this);
}

HeapVector<Member<PointerEvent>> PointerEvent::getCoalescedEvents() const {
  return m_coalescedEvents;
}

DEFINE_TRACE(PointerEvent) {
  visitor->trace(m_coalescedEvents);
  MouseEvent::trace(visitor);
}

PointerEventDispatchMediator* PointerEventDispatchMediator::create(
    PointerEvent* pointerEvent) {
  return new PointerEventDispatchMediator(pointerEvent);
}

PointerEventDispatchMediator::PointerEventDispatchMediator(
    PointerEvent* pointerEvent)
    : EventDispatchMediator(pointerEvent) {}

PointerEvent& PointerEventDispatchMediator::event() const {
  return toPointerEvent(EventDispatchMediator::event());
}

DispatchEventResult PointerEventDispatchMediator::dispatchEvent(
    EventDispatcher& dispatcher) const {
  if (event().type().isEmpty())
    return DispatchEventResult::NotCanceled;  // Shouldn't happen.

  DCHECK(!event().target() || event().target() != event().relatedTarget());

  event().eventPath().adjustForRelatedTarget(dispatcher.node(),
                                             event().relatedTarget());

  return dispatcher.dispatch();
}

}  // namespace blink

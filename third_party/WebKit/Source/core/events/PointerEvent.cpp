// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEvent.h"

#include "core/dom/Element.h"
#include "core/dom/events/EventDispatcher.h"

namespace blink {

PointerEvent::PointerEvent(const AtomicString& type,
                           const PointerEventInit& initializer,
                           TimeTicks platform_time_stamp)
    : MouseEvent(type, initializer, platform_time_stamp),
      pointer_id_(0),
      width_(0),
      height_(0),
      pressure_(0),
      tilt_x_(0),
      tilt_y_(0),
      tangential_pressure_(0),
      twist_(0),
      is_primary_(false),
      coalesced_events_targets_dirty_(false) {
  if (initializer.hasPointerId())
    pointer_id_ = initializer.pointerId();
  if (initializer.hasWidth())
    width_ = initializer.width();
  if (initializer.hasHeight())
    height_ = initializer.height();
  if (initializer.hasPressure())
    pressure_ = initializer.pressure();
  if (initializer.hasTiltX())
    tilt_x_ = initializer.tiltX();
  if (initializer.hasTiltY())
    tilt_y_ = initializer.tiltY();
  if (initializer.hasTangentialPressure())
    tangential_pressure_ = initializer.tangentialPressure();
  if (initializer.hasTwist())
    twist_ = initializer.twist();
  if (initializer.hasPointerType())
    pointer_type_ = initializer.pointerType();
  if (initializer.hasIsPrimary())
    is_primary_ = initializer.isPrimary();
  if (initializer.hasCoalescedEvents()) {
    for (auto coalesced_event : initializer.coalescedEvents())
      coalesced_events_.push_back(coalesced_event);
  }
}

bool PointerEvent::IsMouseEvent() const {
  return false;
}

bool PointerEvent::IsPointerEvent() const {
  return true;
}

EventDispatchMediator* PointerEvent::CreateMediator() {
  return PointerEventDispatchMediator::Create(this);
}

void PointerEvent::ReceivedTarget() {
  coalesced_events_targets_dirty_ = true;
  MouseEvent::ReceivedTarget();
}

HeapVector<Member<PointerEvent>> PointerEvent::getCoalescedEvents() {
  if (coalesced_events_targets_dirty_) {
    for (auto coalesced_event : coalesced_events_)
      coalesced_event->SetTarget(target());
    coalesced_events_targets_dirty_ = false;
  }
  return coalesced_events_;
}

void PointerEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(coalesced_events_);
  MouseEvent::Trace(visitor);
}

PointerEventDispatchMediator* PointerEventDispatchMediator::Create(
    PointerEvent* pointer_event) {
  return new PointerEventDispatchMediator(pointer_event);
}

PointerEventDispatchMediator::PointerEventDispatchMediator(
    PointerEvent* pointer_event)
    : EventDispatchMediator(pointer_event) {}

PointerEvent& PointerEventDispatchMediator::Event() const {
  return ToPointerEvent(EventDispatchMediator::GetEvent());
}

DispatchEventResult PointerEventDispatchMediator::DispatchEvent(
    EventDispatcher& dispatcher) const {
  if (Event().type().IsEmpty())
    return DispatchEventResult::kNotCanceled;  // Shouldn't happen.

  DCHECK(!Event().target() || Event().target() != Event().relatedTarget());

  Event().GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(),
                                                Event().relatedTarget());

  return dispatcher.Dispatch();
}

}  // namespace blink

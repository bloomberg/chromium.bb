// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"

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

double PointerEvent::screenX() const {
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? static_cast<int>(screen_location_.X())
             : screen_location_.X();
}

double PointerEvent::screenY() const {
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? static_cast<int>(screen_location_.Y())
             : screen_location_.Y();
}

double PointerEvent::clientX() const {
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? static_cast<int>(client_location_.X())
             : client_location_.X();
}

double PointerEvent::clientY() const {
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? static_cast<int>(client_location_.Y())
             : client_location_.Y();
}

double PointerEvent::pageX() const {
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? static_cast<int>(page_location_.X())
             : page_location_.X();
}

double PointerEvent::pageY() const {
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? static_cast<int>(page_location_.Y())
             : page_location_.Y();
}

double PointerEvent::offsetX() {
  if (!HasPosition())
    return 0;
  if (!has_cached_relative_position_)
    ComputeRelativePosition();
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? std::round(offset_location_.X())
             : offset_location_.X();
}

double PointerEvent::offsetY() {
  if (!HasPosition())
    return 0;
  if (!has_cached_relative_position_)
    ComputeRelativePosition();
  return (!RuntimeEnabledFeatures::FractionalMouseTypePointerEventEnabled() &&
          pointer_type_ == "mouse")
             ? std::round(offset_location_.Y())
             : offset_location_.Y();
}

void PointerEvent::ReceivedTarget() {
  coalesced_events_targets_dirty_ = true;
  MouseEvent::ReceivedTarget();
}

Node* PointerEvent::toElement() const {
  return nullptr;
}

Node* PointerEvent::fromElement() const {
  return nullptr;
}

HeapVector<Member<PointerEvent>> PointerEvent::getCoalescedEvents() {
  if (coalesced_events_targets_dirty_) {
    for (auto coalesced_event : coalesced_events_)
      coalesced_event->SetTarget(target());
    coalesced_events_targets_dirty_ = false;
  }
  return coalesced_events_;
}

TimeTicks PointerEvent::OldestPlatformTimeStamp() const {
  if (coalesced_events_.size() > 0) {
    // Assume that time stamps of coalesced events are in ascending order.
    return coalesced_events_[0]->PlatformTimeStamp();
  }
  return this->PlatformTimeStamp();
}

void PointerEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(coalesced_events_);
  MouseEvent::Trace(visitor);
}

DispatchEventResult PointerEvent::DispatchEvent(EventDispatcher& dispatcher) {
  if (type().IsEmpty())
    return DispatchEventResult::kNotCanceled;  // Shouldn't happen.

  DCHECK(!target() || target() != relatedTarget());

  GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(), relatedTarget());

  return dispatcher.Dispatch();
}

}  // namespace blink

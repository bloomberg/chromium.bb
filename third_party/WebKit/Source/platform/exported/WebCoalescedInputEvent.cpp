// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_coalesced_input_event.h"

#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"

namespace blink {

namespace {

struct WebInputEventDelete {
  template <class EventType>
  bool Execute(WebInputEvent* event) const {
    if (!event)
      return false;
    DCHECK_EQ(sizeof(EventType), event->size());
    delete static_cast<EventType*>(event);
    return true;
  }
};

template <typename Operator, typename ArgIn>
bool Apply(Operator op, WebInputEvent::Type type, const ArgIn& arg_in) {
  if (WebInputEvent::IsMouseEventType(type))
    return op.template Execute<WebMouseEvent>(arg_in);
  if (type == WebInputEvent::kMouseWheel)
    return op.template Execute<WebMouseWheelEvent>(arg_in);
  if (WebInputEvent::IsKeyboardEventType(type))
    return op.template Execute<WebKeyboardEvent>(arg_in);
  if (WebInputEvent::IsTouchEventType(type))
    return op.template Execute<WebTouchEvent>(arg_in);
  if (WebInputEvent::IsGestureEventType(type))
    return op.template Execute<WebGestureEvent>(arg_in);
  if (WebInputEvent::IsPointerEventType(type))
    return op.template Execute<WebPointerEvent>(arg_in);

  NOTREACHED() << "Unknown webkit event type " << type;
  return false;
}
}

void WebCoalescedInputEvent::WebInputEventDeleter::operator()(
    WebInputEvent* event) const {
  if (!event)
    return;
  Apply(WebInputEventDelete(), event->GetType(), event);
}

WebInputEvent* WebCoalescedInputEvent::EventPointer() {
  return event_.get();
}

void WebCoalescedInputEvent::AddCoalescedEvent(
    const blink::WebInputEvent& event) {
  coalesced_events_.push_back(MakeWebScopedInputEvent(event));
}

const WebInputEvent& WebCoalescedInputEvent::Event() const {
  return *event_.get();
}

size_t WebCoalescedInputEvent::CoalescedEventSize() const {
  return coalesced_events_.size();
}

const WebInputEvent& WebCoalescedInputEvent::CoalescedEvent(
    size_t index) const {
  return *coalesced_events_[index].get();
}

std::vector<const WebInputEvent*>
WebCoalescedInputEvent::GetCoalescedEventsPointers() const {
  std::vector<const WebInputEvent*> events;
  for (const auto& event : coalesced_events_)
    events.push_back(event.get());
  return events;
}

WebCoalescedInputEvent::WebCoalescedInputEvent(const WebInputEvent& event) {
  event_ = MakeWebScopedInputEvent(event);
  coalesced_events_.push_back(MakeWebScopedInputEvent(event));
}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    const WebInputEvent& event,
    const std::vector<const WebInputEvent*>& coalesced_events) {
  event_ = MakeWebScopedInputEvent(event);
  for (const auto& coalesced_event : coalesced_events)
    coalesced_events_.push_back(MakeWebScopedInputEvent(*coalesced_event));
}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    const WebPointerEvent& event,
    const std::vector<WebPointerEvent>& coalesced_events) {
  event_ = MakeWebScopedInputEvent(event);
  for (const auto& coalesced_event : coalesced_events)
    coalesced_events_.push_back(MakeWebScopedInputEvent(coalesced_event));
}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    const WebCoalescedInputEvent& event)
    : WebCoalescedInputEvent(event.Event(),
                             event.GetCoalescedEventsPointers()) {}

WebCoalescedInputEvent::WebScopedInputEvent
WebCoalescedInputEvent::MakeWebScopedInputEvent(
    const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    return WebScopedInputEvent(new blink::WebGestureEvent(
        static_cast<const blink::WebGestureEvent&>(event)));
  }
  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    return WebScopedInputEvent(new blink::WebMouseEvent(
        static_cast<const blink::WebMouseEvent&>(event)));
  }
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    return WebScopedInputEvent(new blink::WebTouchEvent(
        static_cast<const blink::WebTouchEvent&>(event)));
  }
  if (event.GetType() == blink::WebInputEvent::kMouseWheel) {
    return WebScopedInputEvent(new blink::WebMouseWheelEvent(
        static_cast<const blink::WebMouseWheelEvent&>(event)));
  }
  if (blink::WebInputEvent::IsKeyboardEventType(event.GetType())) {
    return WebScopedInputEvent(new blink::WebKeyboardEvent(
        static_cast<const blink::WebKeyboardEvent&>(event)));
  }
  if (blink::WebInputEvent::IsPointerEventType(event.GetType())) {
    return WebScopedInputEvent(new blink::WebPointerEvent(
        static_cast<const blink::WebPointerEvent&>(event)));
  }
  NOTREACHED();
  return WebScopedInputEvent();
}

}  // namespace blink

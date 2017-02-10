// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebCoalescedInputEvent.h"

#include "public/platform/WebGestureEvent.h"
#include "public/platform/WebKeyboardEvent.h"
#include "public/platform/WebMouseWheelEvent.h"
#include "public/platform/WebTouchEvent.h"

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
bool Apply(Operator op, WebInputEvent::Type type, const ArgIn& argIn) {
  if (WebInputEvent::isMouseEventType(type))
    return op.template Execute<WebMouseEvent>(argIn);
  if (type == WebInputEvent::MouseWheel)
    return op.template Execute<WebMouseWheelEvent>(argIn);
  if (WebInputEvent::isKeyboardEventType(type))
    return op.template Execute<WebKeyboardEvent>(argIn);
  if (WebInputEvent::isTouchEventType(type))
    return op.template Execute<WebTouchEvent>(argIn);
  if (WebInputEvent::isGestureEventType(type))
    return op.template Execute<WebGestureEvent>(argIn);

  NOTREACHED() << "Unknown webkit event type " << type;
  return false;
}
}

void WebCoalescedInputEvent::WebInputEventDeleter::operator()(
    WebInputEvent* event) const {
  if (!event)
    return;
  Apply(WebInputEventDelete(), event->type(), event);
}

WebInputEvent* WebCoalescedInputEvent::eventPointer() {
  return m_event.get();
}

void WebCoalescedInputEvent::addCoalescedEvent(
    const blink::WebInputEvent& event) {
  m_coalescedEvents.push_back(makeWebScopedInputEvent(event));
}

const WebInputEvent& WebCoalescedInputEvent::event() const {
  return *m_event.get();
}

size_t WebCoalescedInputEvent::coalescedEventSize() const {
  return m_coalescedEvents.size();
}

const WebInputEvent& WebCoalescedInputEvent::coalescedEvent(
    size_t index) const {
  return *m_coalescedEvents[index].get();
}

std::vector<const WebInputEvent*>
WebCoalescedInputEvent::getCoalescedEventsPointers() const {
  std::vector<const WebInputEvent*> events;
  for (const auto& event : m_coalescedEvents)
    events.push_back(event.get());
  return events;
}

WebCoalescedInputEvent::WebCoalescedInputEvent(const WebInputEvent& event) {
  m_event = makeWebScopedInputEvent(event);
  m_coalescedEvents.push_back(makeWebScopedInputEvent(event));
}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    const WebInputEvent& event,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  m_event = makeWebScopedInputEvent(event);
  for (const auto& coalescedEvent : coalescedEvents)
    m_coalescedEvents.push_back(makeWebScopedInputEvent(*coalescedEvent));
}

WebCoalescedInputEvent::WebScopedInputEvent
WebCoalescedInputEvent::makeWebScopedInputEvent(
    const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::isGestureEventType(event.type())) {
    return WebScopedInputEvent(new blink::WebGestureEvent(
        static_cast<const blink::WebGestureEvent&>(event)));
  }
  if (blink::WebInputEvent::isMouseEventType(event.type())) {
    return WebScopedInputEvent(new blink::WebMouseEvent(
        static_cast<const blink::WebMouseEvent&>(event)));
  }
  if (blink::WebInputEvent::isTouchEventType(event.type())) {
    return WebScopedInputEvent(new blink::WebTouchEvent(
        static_cast<const blink::WebTouchEvent&>(event)));
  }
  if (event.type() == blink::WebInputEvent::MouseWheel) {
    return WebScopedInputEvent(new blink::WebMouseWheelEvent(
        static_cast<const blink::WebMouseWheelEvent&>(event)));
  }
  if (blink::WebInputEvent::isKeyboardEventType(event.type())) {
    return WebScopedInputEvent(new blink::WebKeyboardEvent(
        static_cast<const blink::WebKeyboardEvent&>(event)));
  }
  NOTREACHED();
  return WebScopedInputEvent();
}

}  // namespace blink

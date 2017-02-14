/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/events/GestureEvent.h"

#include "core/dom/Element.h"
#include "wtf/text/AtomicString.h"

namespace blink {

GestureEvent* GestureEvent::create(AbstractView* view,
                                   const WebGestureEvent& event) {
  AtomicString eventType;

  switch (event.type()) {
    case WebInputEvent::GestureScrollBegin:
      eventType = EventTypeNames::gesturescrollstart;
      break;
    case WebInputEvent::GestureScrollEnd:
      eventType = EventTypeNames::gesturescrollend;
      break;
    case WebInputEvent::GestureScrollUpdate:
      eventType = EventTypeNames::gesturescrollupdate;
      break;
    case WebInputEvent::GestureTap:
      eventType = EventTypeNames::gesturetap;
      break;
    case WebInputEvent::GestureTapUnconfirmed:
      eventType = EventTypeNames::gesturetapunconfirmed;
      break;
    case WebInputEvent::GestureTapDown:
      eventType = EventTypeNames::gesturetapdown;
      break;
    case WebInputEvent::GestureShowPress:
      eventType = EventTypeNames::gestureshowpress;
      break;
    case WebInputEvent::GestureLongPress:
      eventType = EventTypeNames::gesturelongpress;
      break;
    case WebInputEvent::GestureFlingStart:
      eventType = EventTypeNames::gestureflingstart;
      break;
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureTapCancel:
    default:
      return nullptr;
  }
  return new GestureEvent(eventType, view, event);
}

GestureEvent::GestureEvent(const AtomicString& eventType,
                           AbstractView* view,
                           const WebGestureEvent& event)
    : UIEventWithKeyState(
          eventType,
          true,
          true,
          view,
          0,
          static_cast<WebInputEvent::Modifiers>(event.modifiers()),
          TimeTicks::FromSeconds(event.timeStampSeconds()),
          nullptr),
      m_nativeEvent(event) {}

const AtomicString& GestureEvent::interfaceName() const {
  // FIXME: when a GestureEvent.idl interface is defined, return the string
  // "GestureEvent".  Until that happens, do not advertise an interface that
  // does not exist, since it will trip up the bindings integrity checks.
  return UIEvent::interfaceName();
}

bool GestureEvent::isGestureEvent() const {
  return true;
}

DEFINE_TRACE(GestureEvent) {
  UIEvent::trace(visitor);
}

}  // namespace blink

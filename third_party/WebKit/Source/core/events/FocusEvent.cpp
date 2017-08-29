/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/events/FocusEvent.h"

#include "core/dom/events/Event.h"
#include "core/dom/events/EventDispatcher.h"

namespace blink {

const AtomicString& FocusEvent::InterfaceName() const {
  return EventNames::FocusEvent;
}

bool FocusEvent::IsFocusEvent() const {
  return true;
}

FocusEvent::FocusEvent() {}

FocusEvent::FocusEvent(const AtomicString& type,
                       bool can_bubble,
                       bool cancelable,
                       AbstractView* view,
                       int detail,
                       EventTarget* related_target,
                       InputDeviceCapabilities* source_capabilities)
    : UIEvent(type,
              can_bubble,
              cancelable,
              ComposedMode::kComposed,
              TimeTicks::Now(),
              view,
              detail,
              source_capabilities),
      related_target_(related_target) {}

FocusEvent::FocusEvent(const AtomicString& type,
                       const FocusEventInit& initializer)
    : UIEvent(type, initializer) {
  if (initializer.hasRelatedTarget())
    related_target_ = initializer.relatedTarget();
}

EventDispatchMediator* FocusEvent::CreateMediator() {
  return FocusEventDispatchMediator::Create(this);
}

DEFINE_TRACE(FocusEvent) {
  visitor->Trace(related_target_);
  UIEvent::Trace(visitor);
}

FocusEventDispatchMediator* FocusEventDispatchMediator::Create(
    FocusEvent* focus_event) {
  return new FocusEventDispatchMediator(focus_event);
}

FocusEventDispatchMediator::FocusEventDispatchMediator(FocusEvent* focus_event)
    : EventDispatchMediator(focus_event) {}

DispatchEventResult FocusEventDispatchMediator::DispatchEvent(
    EventDispatcher& dispatcher) const {
  Event().GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(),
                                                Event().relatedTarget());
  return EventDispatchMediator::DispatchEvent(dispatcher);
}

}  // namespace blink

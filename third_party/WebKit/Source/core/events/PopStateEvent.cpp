/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "core/events/PopStateEvent.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/frame/History.h"

namespace blink {

PopStateEvent::PopStateEvent()
    : serialized_state_(nullptr), history_(nullptr) {}

PopStateEvent::PopStateEvent(const AtomicString& type,
                             const PopStateEventInit& initializer)
    : Event(type, initializer), history_(nullptr) {
  if (initializer.hasState())
    state_ = initializer.state();
}

PopStateEvent::PopStateEvent(PassRefPtr<SerializedScriptValue> serialized_state,
                             History* history)
    : Event(EventTypeNames::popstate, false, true),
      serialized_state_(std::move(serialized_state)),
      history_(history) {}

PopStateEvent::~PopStateEvent() {}

PopStateEvent* PopStateEvent::Create() {
  return new PopStateEvent;
}

PopStateEvent* PopStateEvent::Create(
    PassRefPtr<SerializedScriptValue> serialized_state,
    History* history) {
  return new PopStateEvent(std::move(serialized_state), history);
}

PopStateEvent* PopStateEvent::Create(const AtomicString& type,
                                     const PopStateEventInit& initializer) {
  return new PopStateEvent(type, initializer);
}

const AtomicString& PopStateEvent::InterfaceName() const {
  return EventNames::PopStateEvent;
}

DEFINE_TRACE(PopStateEvent) {
  visitor->Trace(history_);
  Event::Trace(visitor);
}

}  // namespace blink

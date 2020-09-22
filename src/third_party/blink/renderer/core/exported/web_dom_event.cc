/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_dom_event.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/custom_event.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"

namespace blink {

WebDOMEvent WebDOMEvent::CreateCustomEvent(
    v8::Isolate* isolate,
    const char* name,
    bool canBubble,
    bool cancelable,
    v8::Local<v8::Value> value) {
  CustomEvent* customEvent = CustomEvent::Create();
  DCHECK (isolate);
  DCHECK (!(isolate->GetCurrentContext().IsEmpty()));
  ScriptState* state = ScriptState::Current(isolate);
  customEvent->initCustomEvent(state, name, canBubble, cancelable, ScriptValue(isolate, std::move(value)));
  return WebDOMEvent(customEvent);
}

void WebDOMEvent::Reset() {
  Assign(nullptr);
}

void WebDOMEvent::Assign(const WebDOMEvent& other) {
  private_ = other.private_;
}

void WebDOMEvent::Assign(Event* event) {
  private_ = event;
}

WebDOMEvent::WebDOMEvent(Event* event) : private_(event) {}

WebDOMEvent::operator Event*() const {
  return private_.Get();
}

}  // namespace blink

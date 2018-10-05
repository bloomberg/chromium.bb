// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
void JSEventListener::CallListenerFunction(EventTarget&,
                                           Event& event,
                                           v8::Local<v8::Value> js_event) {
  // Step 10: Call a listener with event's currentTarget as receiver and event
  // and handle errors if thrown.
  v8::Maybe<void> maybe_result =
      event_listener_->handleEvent(event.currentTarget(), &event);
  ALLOW_UNUSED_LOCAL(maybe_result);
}

void JSEventListener::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_listener_);
  JSBasedEventListener::Trace(visitor);
}

}  // namespace blink

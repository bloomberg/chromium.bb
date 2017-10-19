// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchEvent.h"

#include "bindings/core/v8/ToV8ForCore.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

ForeignFetchEvent* ForeignFetchEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const ForeignFetchEventInit& initializer) {
  return new ForeignFetchEvent(script_state, type, initializer, nullptr,
                               nullptr);
}

ForeignFetchEvent* ForeignFetchEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const ForeignFetchEventInit& initializer,
    ForeignFetchRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return new ForeignFetchEvent(script_state, type, initializer,
                               respond_with_observer, wait_until_observer);
}

Request* ForeignFetchEvent::request() const {
  return request_;
}

String ForeignFetchEvent::origin() const {
  return origin_;
}

void ForeignFetchEvent::respondWith(ScriptState* script_state,
                                    ScriptPromise script_promise,
                                    ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_)
    observer_->RespondWith(script_state, script_promise, exception_state);
}

const AtomicString& ForeignFetchEvent::InterfaceName() const {
  return EventNames::ForeignFetchEvent;
}

ForeignFetchEvent::ForeignFetchEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const ForeignFetchEventInit& initializer,
    ForeignFetchRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      observer_(respond_with_observer) {
  if (initializer.hasOrigin())
    origin_ = initializer.origin();
  if (initializer.hasRequest()) {
    request_ = initializer.request();
    ScriptState::Scope scope(script_state);
    request_ = initializer.request();
    v8::Local<v8::Value> request = ToV8(request_, script_state);
    v8::Local<v8::Value> event = ToV8(this, script_state);
    if (event.IsEmpty()) {
      // |toV8| can return an empty handle when the worker is terminating.
      // We don't want the renderer to crash in such cases.
      // TODO(yhirano): Replace this branch with an assertion when the
      // graceful shutdown mechanism is introduced.
      return;
    }
    DCHECK(event->IsObject());
    // Sets a hidden value in order to teach V8 the dependency from
    // the event to the request.
    V8PrivateProperty::GetFetchEventRequest(script_state->GetIsolate())
        .Set(event.As<v8::Object>(), request);
    // From the same reason as above, setHiddenValue can return false.
    // TODO(yhirano): Add an assertion that it returns true once the
    // graceful shutdown mechanism is introduced.
  }
}

void ForeignFetchEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(request_);
  ExtendableEvent::Trace(visitor);
}

}  // namespace blink

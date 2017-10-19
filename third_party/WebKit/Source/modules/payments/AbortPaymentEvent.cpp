// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/AbortPaymentEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/EventModules.h"
#include "modules/serviceworkers/ExtendableEventInit.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

AbortPaymentEvent* AbortPaymentEvent::Create(
    const AtomicString& type,
    const ExtendableEventInit& initializer) {
  return new AbortPaymentEvent(type, initializer, nullptr, nullptr);
}

AbortPaymentEvent* AbortPaymentEvent::Create(
    const AtomicString& type,
    const ExtendableEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return new AbortPaymentEvent(type, initializer, respond_with_observer,
                               wait_until_observer);
}

AbortPaymentEvent::~AbortPaymentEvent() {}

const AtomicString& AbortPaymentEvent::InterfaceName() const {
  return EventNames::AbortPaymentEvent;
}

void AbortPaymentEvent::respondWith(ScriptState* script_state,
                                    ScriptPromise script_promise,
                                    ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_) {
    observer_->RespondWith(script_state, script_promise, exception_state);
  }
}

void AbortPaymentEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

AbortPaymentEvent::AbortPaymentEvent(const AtomicString& type,
                                     const ExtendableEventInit& initializer,
                                     RespondWithObserver* respond_with_observer,
                                     WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      observer_(respond_with_observer) {}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

#include "modules/serviceworkers/RespondWithObserver.h"
#include "wtf/text/AtomicString.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentAppRequest& app_request,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return new PaymentRequestEvent(type, app_request, respond_with_observer,
                                 wait_until_observer);
}

PaymentRequestEvent::~PaymentRequestEvent() {}

const AtomicString& PaymentRequestEvent::InterfaceName() const {
  return EventNames::PaymentRequestEvent;
}

void PaymentRequestEvent::appRequest(PaymentAppRequest& app_request) const {
  app_request = app_request_;
}

void PaymentRequestEvent::respondWith(ScriptState* script_state,
                                      ScriptPromise script_promise,
                                      ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_) {
    observer_->RespondWith(script_state, script_promise, exception_state);
  }
}

DEFINE_TRACE(PaymentRequestEvent) {
  visitor->Trace(app_request_);
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(
    const AtomicString& type,
    const PaymentAppRequest& app_request,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, ExtendableEventInit(), wait_until_observer),
      app_request_(app_request),
      observer_(respond_with_observer) {}

}  // namespace blink

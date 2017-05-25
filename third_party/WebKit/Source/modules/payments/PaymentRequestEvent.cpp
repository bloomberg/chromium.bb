// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

#include "modules/serviceworkers/RespondWithObserver.h"
#include "platform/wtf/text/AtomicString.h"

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

const String& PaymentRequestEvent::topLevelOrigin() const {
  return top_level_origin_;
}

const String& PaymentRequestEvent::paymentRequestOrigin() const {
  return payment_request_origin_;
}

const String& PaymentRequestEvent::paymentRequestId() const {
  return payment_request_id_;
}

const HeapVector<PaymentMethodData>& PaymentRequestEvent::methodData() const {
  return method_data_;
}

void PaymentRequestEvent::total(PaymentItem& value) const {
  value = total_;
}

const HeapVector<PaymentDetailsModifier>& PaymentRequestEvent::modifiers()
    const {
  return modifiers_;
}

const String& PaymentRequestEvent::instrumentKey() const {
  return instrument_key_;
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
  visitor->Trace(method_data_);
  visitor->Trace(modifiers_);
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(
    const AtomicString& type,
    const PaymentAppRequest& app_request,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, ExtendableEventInit(), wait_until_observer),
      top_level_origin_(app_request.topLevelOrigin()),
      payment_request_origin_(app_request.paymentRequestOrigin()),
      payment_request_id_(app_request.paymentRequestId()),
      method_data_(std::move(app_request.methodData())),
      total_(app_request.total()),
      modifiers_(app_request.modifiers()),
      instrument_key_(app_request.instrumentKey()),
      observer_(respond_with_observer) {}

}  // namespace blink

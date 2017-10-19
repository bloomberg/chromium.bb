// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/CanMakePaymentEvent.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLocation.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClientCallback.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

CanMakePaymentEvent* CanMakePaymentEvent::Create(
    const AtomicString& type,
    const CanMakePaymentEventInit& initializer) {
  return new CanMakePaymentEvent(type, initializer, nullptr, nullptr);
}

CanMakePaymentEvent* CanMakePaymentEvent::Create(
    const AtomicString& type,
    const CanMakePaymentEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return new CanMakePaymentEvent(type, initializer, respond_with_observer,
                                 wait_until_observer);
}

CanMakePaymentEvent::~CanMakePaymentEvent() {}

const AtomicString& CanMakePaymentEvent::InterfaceName() const {
  return EventNames::CanMakePaymentEvent;
}

const String& CanMakePaymentEvent::topLevelOrigin() const {
  return top_level_origin_;
}

const String& CanMakePaymentEvent::paymentRequestOrigin() const {
  return payment_request_origin_;
}

const HeapVector<PaymentMethodData>& CanMakePaymentEvent::methodData() const {
  return method_data_;
}

const HeapVector<PaymentDetailsModifier>& CanMakePaymentEvent::modifiers()
    const {
  return modifiers_;
}

void CanMakePaymentEvent::respondWith(ScriptState* script_state,
                                      ScriptPromise script_promise,
                                      ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_) {
    observer_->RespondWith(script_state, script_promise, exception_state);
  }
}

void CanMakePaymentEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(method_data_);
  visitor->Trace(modifiers_);
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

CanMakePaymentEvent::CanMakePaymentEvent(
    const AtomicString& type,
    const CanMakePaymentEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_level_origin_(initializer.topLevelOrigin()),
      payment_request_origin_(initializer.paymentRequestOrigin()),
      method_data_(std::move(initializer.methodData())),
      modifiers_(initializer.modifiers()),
      observer_(respond_with_observer) {}

}  // namespace blink

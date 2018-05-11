// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/can_make_payment_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/serviceworkers/respond_with_observer.h"
#include "third_party/blink/renderer/modules/serviceworkers/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/modules/serviceworkers/service_worker_window_client_callback.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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

CanMakePaymentEvent::~CanMakePaymentEvent() = default;

const AtomicString& CanMakePaymentEvent::InterfaceName() const {
  return EventNames::CanMakePaymentEvent;
}

const String& CanMakePaymentEvent::topOrigin() const {
  return top_origin_;
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
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot respond with data when the event is not trusted");
    return;
  }

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
      top_origin_(initializer.topOrigin()),
      payment_request_origin_(initializer.paymentRequestOrigin()),
      method_data_(std::move(initializer.methodData())),
      modifiers_(initializer.modifiers()),
      observer_(respond_with_observer) {}

}  // namespace blink

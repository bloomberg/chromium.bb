// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestEvent.h"

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

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit& initializer) {
  return new PaymentRequestEvent(type, initializer, nullptr, nullptr);
}

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return new PaymentRequestEvent(type, initializer, respond_with_observer,
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

const ScriptValue PaymentRequestEvent::total(ScriptState* script_state) const {
  return ScriptValue::From(script_state, total_);
}

const HeapVector<PaymentDetailsModifier>& PaymentRequestEvent::modifiers()
    const {
  return modifiers_;
}

const String& PaymentRequestEvent::instrumentKey() const {
  return instrument_key_;
}

ScriptPromise PaymentRequestEvent::openWindow(ScriptState* script_state,
                                              const String& url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  KURL parsed_url_to_open = context->CompleteURL(url);
  if (!parsed_url_to_open.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }

  // TODO(gogerald): Once the issue of the spec is resolved, we should apply the
  // changes. Refer https://github.com/w3c/payment-handler/issues/168.
  if (!context->GetSecurityOrigin()->IsSameSchemeHostPortAndSuborigin(
          SecurityOrigin::Create(parsed_url_to_open).Get())) {
    resolver->Reject(DOMException::Create(
        kSecurityError,
        "'" + parsed_url_to_open.ElidedString() + "' is not allowed."));
    return promise;
  }

  // TODO(gogerald): Once the issue of the spec is resolved, we should apply the
  // changes. Refer https://github.com/w3c/payment-handler/issues/169.
  if (!context->IsWindowInteractionAllowed()) {
    resolver->Reject(DOMException::Create(kInvalidAccessError,
                                          "Not allowed to open a window."));
    return promise;
  }
  context->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(context)->OpenWindowForPaymentHandler(
      parsed_url_to_open, WTF::MakeUnique<NavigateClientCallback>(resolver));
  return promise;
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
    const PaymentRequestEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_level_origin_(initializer.topLevelOrigin()),
      payment_request_origin_(initializer.paymentRequestOrigin()),
      payment_request_id_(initializer.paymentRequestId()),
      method_data_(std::move(initializer.methodData())),
      total_(initializer.total()),
      modifiers_(initializer.modifiers()),
      instrument_key_(initializer.instrumentKey()),
      observer_(respond_with_observer) {}

}  // namespace blink

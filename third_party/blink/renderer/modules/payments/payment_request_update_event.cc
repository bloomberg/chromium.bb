// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_update_event.h"

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/payment_updater.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

// Reject the payment request if the page does not resolve the promise from
// updateWith within 60 seconds.
constexpr TimeDelta kAbortTimeout = TimeDelta::FromSeconds(60);

class UpdatePaymentDetailsFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      PaymentRequestUpdateEvent* update_event) {
    UpdatePaymentDetailsFunction* self =
        MakeGarbageCollected<UpdatePaymentDetailsFunction>(script_state,
                                                           update_event);
    return self->BindToV8Function();
  }

  UpdatePaymentDetailsFunction(ScriptState* script_state,
                               PaymentRequestUpdateEvent* update_event)
      : ScriptFunction(script_state), update_event_(update_event) {
    DCHECK(update_event_);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(update_event_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    update_event_->OnUpdatePaymentDetails(update_event_->type(), value);
    return ScriptValue();
  }

  Member<PaymentRequestUpdateEvent> update_event_;
};

class UpdatePaymentDetailsErrorFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                PaymentUpdater* updater) {
    UpdatePaymentDetailsErrorFunction* self =
        MakeGarbageCollected<UpdatePaymentDetailsErrorFunction>(script_state,
                                                                updater);
    return self->BindToV8Function();
  }

  UpdatePaymentDetailsErrorFunction(ScriptState* script_state,
                                    PaymentUpdater* updater)
      : ScriptFunction(script_state), updater_(updater) {
    DCHECK(updater_);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(updater_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    updater_->OnUpdatePaymentDetailsFailure(
        ToCoreString(value.V8Value()
                         ->ToString(GetScriptState()->GetContext())
                         .ToLocalChecked()));
    return ScriptValue();
  }

  Member<PaymentUpdater> updater_;
};

}  // namespace

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() = default;

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::Create(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit* init) {
  return MakeGarbageCollected<PaymentRequestUpdateEvent>(execution_context,
                                                         type, init);
}

void PaymentRequestUpdateEvent::SetPaymentDetailsUpdater(
    PaymentUpdater* updater) {
  updater_ = updater;
}

void PaymentRequestUpdateEvent::updateWith(ScriptState* script_state,
                                           ScriptPromise promise,
                                           ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot update details when the event is not trusted");
    return;
  }

  if (!updater_)
    return;

  if (wait_for_update_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot update details twice");
    return;
  }

  stopPropagation();
  stopImmediatePropagation();
  wait_for_update_ = true;

  DCHECK(!abort_timer_.IsActive());
  abort_timer_.StartOneShot(kAbortTimeout, FROM_HERE);

  promise.Then(
      UpdatePaymentDetailsFunction::CreateFunction(script_state, this),
      UpdatePaymentDetailsErrorFunction::CreateFunction(script_state, this));
}

void PaymentRequestUpdateEvent::OnUpdatePaymentDetails(
    const AtomicString& event_type,
    const ScriptValue& details_script_value) {
  if (!updater_)
    return;
  abort_timer_.Stop();
  updater_->OnUpdatePaymentDetails(event_type, details_script_value);
  updater_ = nullptr;
}

void PaymentRequestUpdateEvent::OnUpdatePaymentDetailsFailure(
    const String& error) {
  if (!updater_)
    return;
  abort_timer_.Stop();
  updater_->OnUpdatePaymentDetailsFailure(error);
  updater_ = nullptr;
}

void PaymentRequestUpdateEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(updater_);
  Event::Trace(visitor);
}

void PaymentRequestUpdateEvent::OnUpdateEventTimeoutForTesting() {
  OnUpdateEventTimeout(nullptr);
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit* init)
    : Event(type, init),
      wait_for_update_(false),
      abort_timer_(execution_context->GetTaskRunner(TaskType::kUserInteraction),
                   this,
                   &PaymentRequestUpdateEvent::OnUpdateEventTimeout) {}

void PaymentRequestUpdateEvent::OnUpdateEventTimeout(TimerBase*) {
  OnUpdatePaymentDetailsFailure("Timed out waiting for a response to a '" +
                                type() + "' event");
}

}  // namespace blink

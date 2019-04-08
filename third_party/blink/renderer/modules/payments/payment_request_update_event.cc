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
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

// Reject the payment request if the page does not resolve the promise from
// updateWith within 60 seconds.
constexpr TimeDelta kAbortTimeout = TimeDelta::FromSeconds(60);

}  // anonymous namespace

class PaymentRequestUpdateEvent::UpdatePaymentDetailsFunction final
    : public ScriptFunction {
 public:
  enum class ResolveType {
    kFulfilled,
    kRejected,
  };

  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      ResolveType resolve_type,
      PaymentRequestUpdateEvent* event) {
    UpdatePaymentDetailsFunction* self =
        MakeGarbageCollected<UpdatePaymentDetailsFunction>(script_state,
                                                           resolve_type, event);
    return self->BindToV8Function();
  }

  UpdatePaymentDetailsFunction(ScriptState* script_state,
                               ResolveType resolve_type,
                               PaymentRequestUpdateEvent* event)
      : ScriptFunction(script_state),
        resolve_type_(resolve_type),
        event_(event) {
    DCHECK(event_);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(event_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    if (!event_ || !event_->request_)
      return ScriptValue();

    event_->abort_timer_.Stop();
    if (resolve_type_ == ResolveType::kFulfilled) {
      event_->request_->OnUpdatePaymentDetails(value);
    } else if (resolve_type_ == ResolveType::kRejected) {
      event_->request_->OnUpdatePaymentDetailsFailure(
          ToCoreString(value.V8Value()
                           ->ToString(GetScriptState()->GetContext())
                           .ToLocalChecked()));
    }
    event_->SetPaymentRequest(nullptr);
    return ScriptValue();
  }

  ResolveType resolve_type_;
  Member<PaymentRequestUpdateEvent> event_;
};

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() = default;

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::Create(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit* init) {
  return MakeGarbageCollected<PaymentRequestUpdateEvent>(execution_context,
                                                         type, init);
}

void PaymentRequestUpdateEvent::SetPaymentRequest(
    PaymentRequestDelegate* request) {
  request_ = request;
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

  if (wait_for_update_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot update details twice");
    return;
  }

  if (!request_)
    return;

  if (!request_->IsInteractive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "PaymentRequest is no longer interactive");
    return;
  }

  stopPropagation();
  stopImmediatePropagation();
  wait_for_update_ = true;

  DCHECK(!abort_timer_.IsActive());
  abort_timer_.StartOneShot(kAbortTimeout, FROM_HERE);

  promise.Then(UpdatePaymentDetailsFunction::CreateFunction(
                   script_state,
                   UpdatePaymentDetailsFunction::ResolveType::kFulfilled, this),
               UpdatePaymentDetailsFunction::CreateFunction(
                   script_state,
                   UpdatePaymentDetailsFunction::ResolveType::kRejected, this));
}

void PaymentRequestUpdateEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_);
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
  if (!request_)
    return;
  abort_timer_.Stop();
  request_->OnUpdatePaymentDetailsFailure(
      "Timed out waiting for a response to a '" + type() + "' event");
  SetPaymentRequest(nullptr);
}

}  // namespace blink

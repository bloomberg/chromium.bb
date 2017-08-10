// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestUpdateEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/payments/PaymentUpdater.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {
namespace {

// Reject the payment request if the page does not resolve the promise from
// updateWith within 60 seconds.
static const int kAbortTimeout = 60;

class UpdatePaymentDetailsFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                PaymentUpdater* updater) {
    UpdatePaymentDetailsFunction* self =
        new UpdatePaymentDetailsFunction(script_state, updater);
    return self->BindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(updater_);
    ScriptFunction::Trace(visitor);
  }

 private:
  UpdatePaymentDetailsFunction(ScriptState* script_state,
                               PaymentUpdater* updater)
      : ScriptFunction(script_state), updater_(updater) {
    DCHECK(updater_);
  }

  ScriptValue Call(ScriptValue value) override {
    updater_->OnUpdatePaymentDetails(value);
    return ScriptValue();
  }

  Member<PaymentUpdater> updater_;
};

class UpdatePaymentDetailsErrorFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                PaymentUpdater* updater) {
    UpdatePaymentDetailsErrorFunction* self =
        new UpdatePaymentDetailsErrorFunction(script_state, updater);
    return self->BindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(updater_);
    ScriptFunction::Trace(visitor);
  }

 private:
  UpdatePaymentDetailsErrorFunction(ScriptState* script_state,
                                    PaymentUpdater* updater)
      : ScriptFunction(script_state), updater_(updater) {
    DCHECK(updater_);
  }

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

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() {}

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::Create(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit& init) {
  return new PaymentRequestUpdateEvent(execution_context, type, init);
}

void PaymentRequestUpdateEvent::SetPaymentDetailsUpdater(
    PaymentUpdater* updater) {
  DCHECK(!abort_timer_.IsActive());
  abort_timer_.StartOneShot(kAbortTimeout, BLINK_FROM_HERE);
  updater_ = updater;
}

void PaymentRequestUpdateEvent::updateWith(ScriptState* script_state,
                                           ScriptPromise promise,
                                           ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot update details when the event is not trusted");
    return;
  }

  if (!updater_)
    return;

  if (wait_for_update_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Cannot update details twice");
    return;
  }

  stopPropagation();
  stopImmediatePropagation();
  wait_for_update_ = true;

  promise.Then(
      UpdatePaymentDetailsFunction::CreateFunction(script_state, this),
      UpdatePaymentDetailsErrorFunction::CreateFunction(script_state, this));
}

void PaymentRequestUpdateEvent::OnUpdatePaymentDetails(
    const ScriptValue& details_script_value) {
  if (!updater_)
    return;
  abort_timer_.Stop();
  updater_->OnUpdatePaymentDetails(details_script_value);
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

DEFINE_TRACE(PaymentRequestUpdateEvent) {
  visitor->Trace(updater_);
  Event::Trace(visitor);
}

void PaymentRequestUpdateEvent::OnUpdateEventTimeoutForTesting() {
  OnUpdateEventTimeout(nullptr);
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit& init)
    : Event(type, init),
      wait_for_update_(false),
      abort_timer_(
          TaskRunnerHelper::Get(TaskType::kUserInteraction, execution_context),
          this,
          &PaymentRequestUpdateEvent::OnUpdateEventTimeout) {}

void PaymentRequestUpdateEvent::OnUpdateEventTimeout(TimerBase*) {
  OnUpdatePaymentDetailsFailure("Timed out waiting for a response to a '" +
                                type() + "' event");
}

}  // namespace blink

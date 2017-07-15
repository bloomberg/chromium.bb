// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/CanMakePaymentRespondWithObserver.h"

#include <v8.h>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "modules/payments/PaymentHandlerUtils.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/WaitUntilObserver.h"

namespace blink {

CanMakePaymentRespondWithObserver::CanMakePaymentRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

void CanMakePaymentRespondWithObserver::OnResponseRejected(
    WebServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "CanMakePaymentEvent", error);

  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToCanMakePaymentEvent(event_id_, false, event_dispatch_time_);
}

void CanMakePaymentRespondWithObserver::OnResponseFulfilled(
    const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(value.GetIsolate(),
                                 ExceptionState::kUnknownContext,
                                 "PaymentRequestEvent", "respondWith");
  bool response = ToBoolean(ToIsolate(GetExecutionContext()), value.V8Value(),
                            exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    OnResponseRejected(kWebServiceWorkerResponseErrorNoV8Instance);
    return;
  }

  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToCanMakePaymentEvent(event_id_, response, event_dispatch_time_);
}

void CanMakePaymentRespondWithObserver::OnNoResponse() {
  DCHECK(GetExecutionContext());
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToCanMakePaymentEvent(event_id_, false, event_dispatch_time_);
}

DEFINE_TRACE(CanMakePaymentRespondWithObserver) {
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink

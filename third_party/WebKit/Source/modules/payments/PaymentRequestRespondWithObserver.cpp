// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestRespondWithObserver.h"

#include <v8.h>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/V8PaymentAppResponse.h"
#include "core/dom/ExecutionContext.h"
#include "modules/payments/PaymentAppResponse.h"
#include "modules/payments/PaymentHandlerUtils.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "public/platform/modules/payments/WebPaymentAppResponse.h"

namespace blink {

PaymentRequestRespondWithObserver* PaymentRequestRespondWithObserver::Create(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer) {
  return new PaymentRequestRespondWithObserver(context, event_id, observer);
}

void PaymentRequestRespondWithObserver::OnResponseRejected(
    WebServiceWorkerResponseError error) {
  PaymentHandlerUtils::ReportResponseError(GetExecutionContext(),
                                           "PaymentRequestEvent", error);

  WebPaymentAppResponse web_data;
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToPaymentRequestEvent(event_id_, web_data, event_dispatch_time_);
}

void PaymentRequestRespondWithObserver::OnResponseFulfilled(
    const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(value.GetIsolate(),
                                 ExceptionState::kUnknownContext,
                                 "PaymentRequestEvent", "respondWith");
  PaymentAppResponse response = ScriptValue::To<PaymentAppResponse>(
      ToIsolate(GetExecutionContext()), value, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    OnResponseRejected(kWebServiceWorkerResponseErrorNoV8Instance);
    return;
  }

  WebPaymentAppResponse web_data;
  web_data.method_name = response.methodName();

  v8::Local<v8::String> details_value;
  if (!v8::JSON::Stringify(response.details().GetContext(),
                           response.details().V8Value().As<v8::Object>())
           .ToLocal(&details_value)) {
    OnResponseRejected(kWebServiceWorkerResponseErrorUnknown);
    return;
  }
  web_data.stringified_details = ToCoreString(details_value);
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToPaymentRequestEvent(event_id_, web_data, event_dispatch_time_);
}

void PaymentRequestRespondWithObserver::OnNoResponse() {
  DCHECK(GetExecutionContext());
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToPaymentRequestEvent(event_id_, WebPaymentAppResponse(),
                                     event_dispatch_time_);
}

PaymentRequestRespondWithObserver::PaymentRequestRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, event_id, observer) {}

DEFINE_TRACE(PaymentRequestRespondWithObserver) {
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink

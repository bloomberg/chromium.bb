// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestRespondWithObserver.h"

#include <v8.h>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/V8PaymentAppResponse.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/payments/PaymentAppResponse.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "public/platform/modules/payments/WebPaymentAppResponse.h"

namespace blink {
namespace {

// Returns the error message to let the developer know about the reason of the
// unusual failures.
const String GetMessageForResponseError(WebServiceWorkerResponseError error) {
  String error_message =
      "The respondWith() was rejected in PaymentRequestEvent: ";
  switch (error) {
    case kWebServiceWorkerResponseErrorPromiseRejected:
      error_message = error_message + "the promise was rejected.";
      break;
    case kWebServiceWorkerResponseErrorDefaultPrevented:
      error_message =
          error_message +
          "preventDefault() was called without calling respondWith().";
      break;
    case kWebServiceWorkerResponseErrorNoV8Instance:
      error_message = error_message +
                      "an object that was not a PaymentResponse was passed to "
                      "respondWith().";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeError:
      error_message = error_message +
                      "the promise was resolved with an error response object.";
      break;
    case kWebServiceWorkerResponseErrorUnknown:
      error_message = error_message + "an unexpected error occurred.";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeOpaque:
    case kWebServiceWorkerResponseErrorResponseTypeNotBasicOrDefault:
    case kWebServiceWorkerResponseErrorBodyUsed:
    case kWebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest:
    case kWebServiceWorkerResponseErrorResponseTypeOpaqueRedirect:
    case kWebServiceWorkerResponseErrorBodyLocked:
    case kWebServiceWorkerResponseErrorNoForeignFetchResponse:
    case kWebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin:
    case kWebServiceWorkerResponseErrorForeignFetchMismatchedOrigin:
    case kWebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest:
    case kWebServiceWorkerResponseErrorDataPipeCreationFailed:
      NOTREACHED();
      error_message = error_message + "an unexpected error occurred.";
      break;
  }
  return error_message;
}

}  // namespace

PaymentRequestRespondWithObserver::~PaymentRequestRespondWithObserver() {}

PaymentRequestRespondWithObserver* PaymentRequestRespondWithObserver::Create(
    ExecutionContext* context,
    int event_id,
    WaitUntilObserver* observer) {
  return new PaymentRequestRespondWithObserver(context, event_id, observer);
}

void PaymentRequestRespondWithObserver::OnResponseRejected(
    WebServiceWorkerResponseError error) {
  DCHECK(GetExecutionContext());
  GetExecutionContext()->AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                             GetMessageForResponseError(error)));

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

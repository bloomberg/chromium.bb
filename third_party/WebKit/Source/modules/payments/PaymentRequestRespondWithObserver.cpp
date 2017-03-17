// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestRespondWithObserver.h"

#include <v8.h>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
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
const String getMessageForResponseError(WebServiceWorkerResponseError error) {
  String errorMessage =
      "The respondWith() was rejected in PaymentRequestEvent: ";
  switch (error) {
    case WebServiceWorkerResponseErrorPromiseRejected:
      errorMessage = errorMessage + "the promise was rejected.";
      break;
    case WebServiceWorkerResponseErrorDefaultPrevented:
      errorMessage =
          errorMessage +
          "preventDefault() was called without calling respondWith().";
      break;
    case WebServiceWorkerResponseErrorNoV8Instance:
      errorMessage = errorMessage +
                     "an object that was not a PaymentResponse was passed to "
                     "respondWith().";
      break;
    case WebServiceWorkerResponseErrorResponseTypeError:
      errorMessage = errorMessage +
                     "the promise was resolved with an error response object.";
      break;
    case WebServiceWorkerResponseErrorUnknown:
      errorMessage = errorMessage + "an unexpected error occurred.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaque:
    case WebServiceWorkerResponseErrorResponseTypeNotBasicOrDefault:
    case WebServiceWorkerResponseErrorBodyUsed:
    case WebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest:
    case WebServiceWorkerResponseErrorResponseTypeOpaqueRedirect:
    case WebServiceWorkerResponseErrorBodyLocked:
    case WebServiceWorkerResponseErrorNoForeignFetchResponse:
    case WebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin:
    case WebServiceWorkerResponseErrorForeignFetchMismatchedOrigin:
    case WebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest:
      NOTREACHED();
      errorMessage = errorMessage + "an unexpected error occurred.";
      break;
  }
  return errorMessage;
}

}  // namespace

PaymentRequestRespondWithObserver::~PaymentRequestRespondWithObserver() {}

PaymentRequestRespondWithObserver* PaymentRequestRespondWithObserver::create(
    ExecutionContext* context,
    int eventID,
    WaitUntilObserver* observer) {
  return new PaymentRequestRespondWithObserver(context, eventID, observer);
}

void PaymentRequestRespondWithObserver::onResponseRejected(
    WebServiceWorkerResponseError error) {
  DCHECK(getExecutionContext());
  getExecutionContext()->addConsoleMessage(ConsoleMessage::create(
      JSMessageSource, WarningMessageLevel, getMessageForResponseError(error)));

  WebPaymentAppResponse webData;
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToPaymentRequestEvent(m_eventID, webData, m_eventDispatchTime);
}

void PaymentRequestRespondWithObserver::onResponseFulfilled(
    const ScriptValue& value) {
  DCHECK(getExecutionContext());
  ExceptionState exceptionState(value.isolate(), ExceptionState::UnknownContext,
                                "PaymentRequestEvent", "respondWith");
  PaymentAppResponse response = ScriptValue::to<PaymentAppResponse>(
      toIsolate(getExecutionContext()), value, exceptionState);
  if (exceptionState.hadException()) {
    exceptionState.clearException();
    onResponseRejected(WebServiceWorkerResponseErrorNoV8Instance);
    return;
  }

  WebPaymentAppResponse webData;
  webData.methodName = response.methodName();

  v8::Local<v8::String> detailsValue;
  if (!v8::JSON::Stringify(response.details().context(),
                           response.details().v8Value().As<v8::Object>())
           .ToLocal(&detailsValue)) {
    onResponseRejected(WebServiceWorkerResponseErrorUnknown);
    return;
  }
  webData.stringifiedDetails = toCoreString(detailsValue);
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToPaymentRequestEvent(m_eventID, webData, m_eventDispatchTime);
}

void PaymentRequestRespondWithObserver::onNoResponse() {
  DCHECK(getExecutionContext());
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToPaymentRequestEvent(m_eventID, WebPaymentAppResponse(),
                                     m_eventDispatchTime);
}

PaymentRequestRespondWithObserver::PaymentRequestRespondWithObserver(
    ExecutionContext* context,
    int eventID,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, eventID, observer) {}

DEFINE_TRACE(PaymentRequestRespondWithObserver) {
  RespondWithObserver::trace(visitor);
}

}  // namespace blink

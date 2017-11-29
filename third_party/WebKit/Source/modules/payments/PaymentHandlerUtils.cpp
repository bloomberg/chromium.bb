// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentHandlerUtils.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "public/platform/modules/serviceworker/service_worker_error_type.mojom-shared.h"

using blink::mojom::ServiceWorkerResponseError;

namespace blink {

void PaymentHandlerUtils::ReportResponseError(
    ExecutionContext* execution_context,
    const String& event_name_prefix,
    ServiceWorkerResponseError error) {
  String error_message = event_name_prefix + ".respondWith() failed: ";
  switch (error) {
    case ServiceWorkerResponseError::kPromiseRejected:
      error_message =
          error_message + "the promise passed to respondWith() was rejected.";
      break;
    case ServiceWorkerResponseError::kDefaultPrevented:
      error_message =
          error_message +
          "preventDefault() was called without calling respondWith().";
      break;
    case ServiceWorkerResponseError::kNoV8Instance:
      error_message = error_message +
                      "an object that was not a PaymentResponse was passed to "
                      "respondWith().";
      break;
    case ServiceWorkerResponseError::kUnknown:
      error_message = error_message + "an unexpected error occurred.";
      break;
    case ServiceWorkerResponseError::kResponseTypeError:
    case ServiceWorkerResponseError::kResponseTypeOpaque:
    case ServiceWorkerResponseError::kResponseTypeNotBasicOrDefault:
    case ServiceWorkerResponseError::kBodyUsed:
    case ServiceWorkerResponseError::kResponseTypeOpaqueForClientRequest:
    case ServiceWorkerResponseError::kResponseTypeOpaqueRedirect:
    case ServiceWorkerResponseError::kBodyLocked:
    case ServiceWorkerResponseError::kRedirectedResponseForNotFollowRequest:
    case ServiceWorkerResponseError::kDataPipeCreationFailed:
      NOTREACHED();
      error_message = error_message + "an unexpected error occurred.";
      break;
  }

  DCHECK(execution_context);
  execution_context->AddConsoleMessage(ConsoleMessage::Create(
      kJSMessageSource, kWarningMessageLevel, error_message));
}

}  // namespace blink

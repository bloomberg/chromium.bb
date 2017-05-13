/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/serviceworkers/ServiceWorkerError.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "platform/bindings/V8ThrowException.h"

using blink::WebServiceWorkerError;

namespace blink {

namespace {

struct ExceptionParams {
  ExceptionParams(ExceptionCode code,
                  const String& default_message = String(),
                  const String& message = String())
      : code(code), message(message.IsEmpty() ? default_message : message) {}

  ExceptionCode code;
  String message;
};

ExceptionParams GetExceptionParams(const WebServiceWorkerError& web_error) {
  switch (web_error.error_type) {
    case WebServiceWorkerError::kErrorTypeAbort:
      return ExceptionParams(kAbortError,
                             "The Service Worker operation was aborted.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeActivate:
      // Not currently returned as a promise rejection.
      // TODO: Introduce new ActivateError type to ExceptionCodes?
      return ExceptionParams(kAbortError,
                             "The Service Worker activation failed.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeDisabled:
      return ExceptionParams(kNotSupportedError,
                             "Service Worker support is disabled.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeInstall:
      // TODO: Introduce new InstallError type to ExceptionCodes?
      return ExceptionParams(kAbortError,
                             "The Service Worker installation failed.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeScriptEvaluateFailed:
      return ExceptionParams(kAbortError,
                             "The Service Worker script failed to evaluate.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeNavigation:
      // ErrorTypeNavigation should have bailed out before calling this.
      NOTREACHED();
      return ExceptionParams(kUnknownError);
    case WebServiceWorkerError::kErrorTypeNetwork:
      return ExceptionParams(kNetworkError,
                             "The Service Worker failed by network.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeNotFound:
      return ExceptionParams(
          kNotFoundError,
          "The specified Service Worker resource was not found.",
          web_error.message);
    case WebServiceWorkerError::kErrorTypeSecurity:
      return ExceptionParams(
          kSecurityError,
          "The Service Worker security policy prevented an action.",
          web_error.message);
    case WebServiceWorkerError::kErrorTypeState:
      return ExceptionParams(kInvalidStateError,
                             "The Service Worker state was not valid.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeTimeout:
      return ExceptionParams(kAbortError,
                             "The Service Worker operation timed out.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeUnknown:
      return ExceptionParams(kUnknownError,
                             "An unknown error occurred within Service Worker.",
                             web_error.message);
    case WebServiceWorkerError::kErrorTypeType:
      // ErrorTypeType should have been handled before reaching this point.
      NOTREACHED();
      return ExceptionParams(kUnknownError);
  }
  NOTREACHED();
  return ExceptionParams(kUnknownError);
}

}  // namespace

// static
DOMException* ServiceWorkerError::Take(ScriptPromiseResolver*,
                                       const WebServiceWorkerError& web_error) {
  ExceptionParams params = GetExceptionParams(web_error);
  DCHECK_NE(params.code, kUnknownError);
  return DOMException::Create(params.code, params.message);
}

// static
v8::Local<v8::Value> ServiceWorkerErrorForUpdate::Take(
    ScriptPromiseResolver* resolver,
    const WebServiceWorkerError& web_error) {
  ScriptState* script_state = resolver->GetScriptState();
  switch (web_error.error_type) {
    case WebServiceWorkerError::kErrorTypeNetwork:
    case WebServiceWorkerError::kErrorTypeNotFound:
    case WebServiceWorkerError::kErrorTypeScriptEvaluateFailed:
      // According to the spec, these errors during update should result in
      // a TypeError.
      return V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), GetExceptionParams(web_error).message);
    case WebServiceWorkerError::kErrorTypeType:
      return V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                               web_error.message);
    default:
      return ToV8(ServiceWorkerError::Take(resolver, web_error),
                  script_state->GetContext()->Global(),
                  script_state->GetIsolate());
  }
}

}  // namespace blink

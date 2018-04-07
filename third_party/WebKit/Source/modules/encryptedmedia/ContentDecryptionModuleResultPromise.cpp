// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

ExceptionCode WebCdmExceptionToExceptionCode(
    WebContentDecryptionModuleException cdm_exception) {
  switch (cdm_exception) {
    case kWebContentDecryptionModuleExceptionTypeError:
      return kV8TypeError;
    case kWebContentDecryptionModuleExceptionNotSupportedError:
      return kNotSupportedError;
    case kWebContentDecryptionModuleExceptionInvalidStateError:
      return kInvalidStateError;
    case kWebContentDecryptionModuleExceptionQuotaExceededError:
      return kQuotaExceededError;
    case kWebContentDecryptionModuleExceptionUnknownError:
      return kUnknownError;
  }

  NOTREACHED();
  return kUnknownError;
}

ContentDecryptionModuleResultPromise::ContentDecryptionModuleResultPromise(
    ScriptState* script_state)
    : resolver_(ScriptPromiseResolver::Create(script_state)) {}

ContentDecryptionModuleResultPromise::~ContentDecryptionModuleResultPromise() =
    default;

void ContentDecryptionModuleResultPromise::Complete() {
  NOTREACHED();
  if (!IsValidToFulfillPromise())
    return;
  Reject(kInvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::CompleteWithContentDecryptionModule(
    WebContentDecryptionModule* cdm) {
  NOTREACHED();
  if (!IsValidToFulfillPromise())
    return;
  Reject(kInvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::CompleteWithSession(
    WebContentDecryptionModuleResult::SessionStatus status) {
  NOTREACHED();
  if (!IsValidToFulfillPromise())
    return;
  Reject(kInvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::CompleteWithKeyStatus(
    WebEncryptedMediaKeyInformation::KeyStatus) {
  if (!IsValidToFulfillPromise())
    return;
  Reject(kInvalidStateError, "Unexpected completion.");
}

void ContentDecryptionModuleResultPromise::CompleteWithError(
    WebContentDecryptionModuleException exception_code,
    unsigned long system_code,
    const WebString& error_message) {
  if (!IsValidToFulfillPromise())
    return;

  // Non-zero |systemCode| is appended to the |errorMessage|. If the
  // |errorMessage| is empty, we'll report "Rejected with system code
  // (systemCode)".
  StringBuilder result;
  result.Append(error_message);
  if (system_code != 0) {
    if (result.IsEmpty())
      result.Append("Rejected with system code");
    result.Append(" (");
    result.AppendNumber(system_code);
    result.Append(')');
  }
  Reject(WebCdmExceptionToExceptionCode(exception_code), result.ToString());
}

ScriptPromise ContentDecryptionModuleResultPromise::Promise() {
  return resolver_->Promise();
}

void ContentDecryptionModuleResultPromise::Reject(ExceptionCode code,
                                                  const String& error_message) {
  DCHECK(IsValidToFulfillPromise());

  ScriptState::Scope scope(resolver_->GetScriptState());
  v8::Isolate* isolate = resolver_->GetScriptState()->GetIsolate();
  resolver_->Reject(
      V8ThrowDOMException::CreateDOMException(isolate, code, error_message));
  resolver_.Clear();
}

ExecutionContext* ContentDecryptionModuleResultPromise::GetExecutionContext()
    const {
  return resolver_->GetExecutionContext();
}

bool ContentDecryptionModuleResultPromise::IsValidToFulfillPromise() {
  // getExecutionContext() is no longer valid once the context is destroyed.
  // isContextDestroyed() is called to see if the context is in the
  // process of being destroyed. If it is, there is no need to fulfill this
  // promise which is about to go away anyway.
  return GetExecutionContext() && !GetExecutionContext()->IsContextDestroyed();
}

void ContentDecryptionModuleResultPromise::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  ContentDecryptionModuleResult::Trace(visitor);
}

}  // namespace blink

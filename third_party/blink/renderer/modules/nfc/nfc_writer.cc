// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_writer.h"

#include <utility>

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_ndef_message_init.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_push_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
NFCWriter* NFCWriter::Create(ExecutionContext* context) {
  return MakeGarbageCollected<NFCWriter>(context);
}

NFCWriter::NFCWriter(ExecutionContext* context) : ContextClient(context) {}

void NFCWriter::Trace(blink::Visitor* visitor) {
  visitor->Trace(nfc_proxy_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
// https://w3c.github.io/web-nfc/#the-push-method
ScriptPromise NFCWriter::push(ScriptState* script_state,
                              const NDEFMessageSource& push_message,
                              const NFCPushOptions* options,
                              ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!execution_context || !To<Document>(execution_context)->IsInMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           "NFC interfaces are only avaliable "
                                           "in a top-level browsing context"));
  }

  if (options->hasSignal() && options->signal()->aborted()) {
    // If signal’s aborted flag is set, then reject p with an "AbortError"
    // DOMException and return p.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "The NFC operation was cancelled."));
  }

  // 9. If timeout value is NaN or negative, reject promise with "TypeError"
  // and abort these steps.
  if (options->hasTimeout() &&
      (std::isnan(options->timeout()) || options->timeout() < 0)) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(
            script_state->GetIsolate(),
            "Invalid NFCPushOptions.timeout value was provided."));
  }

  // Step 10.8: Run "create Web NFC message", if this throws an exception,
  // reject p with that exception and abort these steps.
  NDEFMessage* ndef_message =
      NDEFMessage::Create(push_message, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  // If NDEFMessage.records is empty, reject promise with TypeError
  if (ndef_message->records().size() == 0) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "Empty NDEFMessage was provided."));
  }

  auto message = device::mojom::blink::NDEFMessage::From(ndef_message);
  DCHECK(message);

  if (!SetNDEFMessageURL(execution_context->GetSecurityOrigin()->ToString(),
                         message.get())) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                           "Cannot set WebNFC Id."));
  }

  if (GetNDEFMessageSize(*message) >
      device::mojom::blink::NDEFMessage::kMaxSize) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotSupportedError,
                          "NDEFMessage exceeds maximum supported size."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);
  InitNfcProxyIfNeeded();

  // If signal is not null, then add the abort steps to signal.
  if (options->hasSignal() && !options->signal()->aborted()) {
    options->signal()->AddAlgorithm(
        WTF::Bind(&NFCWriter::Abort, WrapPersistent(this), options->target(),
                  WrapPersistent(resolver)));
  }

  auto callback = WTF::Bind(&NFCWriter::OnRequestCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));
  nfc_proxy_->Push(std::move(message),
                   device::mojom::blink::NFCPushOptions::From(options),
                   std::move(callback));

  return resolver->Promise();
}

void NFCWriter::OnMojoConnectionError() {
  nfc_proxy_.Clear();

  // If the mojo connection breaks, all push requests will be reject with a
  // default error.
  for (ScriptPromiseResolver* resolver : requests_) {
    resolver->Reject(NFCErrorTypeToDOMException(
        device::mojom::blink::NFCErrorType::NOT_SUPPORTED));
  }
  requests_.clear();
}

void NFCWriter::InitNfcProxyIfNeeded() {
  // Init NfcProxy if needed.
  if (nfc_proxy_)
    return;

  nfc_proxy_ = NFCProxy::From(*To<Document>(GetExecutionContext()));
  DCHECK(nfc_proxy_);

  // Add the writer to proxy's writer list for mojo connection error
  // notification.
  nfc_proxy_->AddWriter(this);
}

void NFCWriter::Abort(const String& target, ScriptPromiseResolver* resolver) {
  // |nfc_proxy_| could be null on Mojo connection failure, simply ignore the
  // abort request in this case.
  if (!nfc_proxy_)
    return;

  // OnRequestCompleted() should always be called whether the push operation is
  // cancelled successfully or not. So do nothing for the cancelled callback.
  nfc_proxy_->CancelPush(target,
                         device::mojom::blink::NFC::CancelPushCallback());
}

void NFCWriter::OnRequestCompleted(ScriptPromiseResolver* resolver,
                                   device::mojom::blink::NFCErrorPtr error) {
  DCHECK(requests_.Contains(resolver));

  requests_.erase(resolver);

  if (error.is_null())
    resolver->Resolve();
  else
    resolver->Reject(NFCErrorTypeToDOMException(error->error_type));
}

}  // namespace blink

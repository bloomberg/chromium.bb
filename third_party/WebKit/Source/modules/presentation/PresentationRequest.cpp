// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/loader/MixedContentChecker.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/ExistingPresentationConnectionCallbacks.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionCallbacks.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationError.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

namespace {

// TODO(mlamouri): refactor in one common place.
PresentationController* GetPresentationController(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);

  Document* document = ToDocument(execution_context);
  if (!document->GetFrame())
    return nullptr;
  return PresentationController::From(*document->GetFrame());
}

WebPresentationClient* PresentationClient(ExecutionContext* execution_context) {
  PresentationController* controller =
      GetPresentationController(execution_context);
  return controller ? controller->Client() : nullptr;
}

Settings* GetSettings(ExecutionContext* execution_context) {
  DCHECK(execution_context);

  Document* document = ToDocument(execution_context);
  return document->GetSettings();
}

}  // anonymous namespace

// static
PresentationRequest* PresentationRequest::Create(
    ExecutionContext* execution_context,
    const String& url,
    ExceptionState& exception_state) {
  Vector<String> urls(1);
  urls[0] = url;
  return Create(execution_context, urls, exception_state);
}

PresentationRequest* PresentationRequest::Create(
    ExecutionContext* execution_context,
    const Vector<String>& urls,
    ExceptionState& exception_state) {
  if (ToDocument(execution_context)->IsSandboxed(kSandboxPresentation)) {
    exception_state.ThrowSecurityError(
        "The document is sandboxed and lacks the 'allow-presentation' flag.");
    return nullptr;
  }

  if (urls.IsEmpty()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "Do not support empty sequence of URLs.");
    return nullptr;
  }

  Vector<KURL> parsed_urls(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    const KURL& parsed_url = KURL(execution_context->Url(), urls[i]);

    if (!parsed_url.IsValid() || !(parsed_url.ProtocolIsInHTTPFamily() ||
                                   parsed_url.ProtocolIs("cast"))) {
      exception_state.ThrowDOMException(
          kSyntaxError, "'" + urls[i] + "' can't be resolved to a valid URL.");
      return nullptr;
    }

    if (MixedContentChecker::IsMixedContent(
            execution_context->GetSecurityOrigin(), parsed_url)) {
      exception_state.ThrowSecurityError(
          "Presentation of an insecure document [" + urls[i] +
          "] is prohibited from a secure context.");
      return nullptr;
    }

    parsed_urls[i] = parsed_url;
  }
  return new PresentationRequest(execution_context, parsed_urls);
}

const AtomicString& PresentationRequest::InterfaceName() const {
  return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void PresentationRequest::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == EventTypeNames::connectionavailable)
    UseCounter::Count(
        GetExecutionContext(),
        UseCounter::kPresentationRequestConnectionAvailableEventListener);
}

bool PresentationRequest::HasPendingActivity() const {
  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  return GetExecutionContext() && HasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* script_state) {
  Settings* context_settings = GetSettings(GetExecutionContext());
  bool is_user_gesture_required =
      !context_settings ||
      context_settings->GetPresentationRequiresUserGesture();

  if (is_user_gesture_required &&
      !UserGestureIndicator::ProcessingUserGesture())
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidAccessError,
            "PresentationRequest::start() requires user gesture."));

  WebPresentationClient* client = PresentationClient(GetExecutionContext());
  if (!client)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  client->StartPresentation(
      urls_, WTF::MakeUnique<PresentationConnectionCallbacks>(resolver, this));
  return resolver->Promise();
}

ScriptPromise PresentationRequest::reconnect(ScriptState* script_state,
                                             const String& id) {
  WebPresentationClient* client = PresentationClient(GetExecutionContext());
  if (!client)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  PresentationController* controller =
      GetPresentationController(GetExecutionContext());
  DCHECK(controller);

  PresentationConnection* existing_connection =
      controller->FindExistingConnection(urls_, id);
  if (existing_connection) {
    client->ReconnectPresentation(
        urls_, id,
        WTF::MakeUnique<ExistingPresentationConnectionCallbacks>(
            resolver, existing_connection));
  } else {
    client->ReconnectPresentation(
        urls_, id,
        WTF::MakeUnique<PresentationConnectionCallbacks>(resolver, this));
  }
  return resolver->Promise();
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* script_state) {
  WebPresentationClient* client = PresentationClient(GetExecutionContext());
  if (!client)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  if (!availability_property_) {
    availability_property_ = new PresentationAvailabilityProperty(
        ExecutionContext::From(script_state), this,
        PresentationAvailabilityProperty::kReady);

    client->GetAvailability(urls_,
                            WTF::MakeUnique<PresentationAvailabilityCallbacks>(
                                availability_property_, urls_));
  }
  return availability_property_->Promise(script_state->World());
}

const Vector<KURL>& PresentationRequest::Urls() const {
  return urls_;
}

DEFINE_TRACE(PresentationRequest) {
  visitor->Trace(availability_property_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* execution_context,
                                         const Vector<KURL>& urls)
    : ContextClient(execution_context), urls_(urls) {
  RecordOriginTypeAccess(execution_context);
}

void PresentationRequest::RecordOriginTypeAccess(
    ExecutionContext* execution_context) const {
  DCHECK(execution_context);
  if (execution_context->IsSecureContext()) {
    UseCounter::Count(execution_context,
                      UseCounter::kPresentationRequestSecureOrigin);
  } else {
    UseCounter::Count(execution_context,
                      UseCounter::kPresentationRequestInsecureOrigin);
  }
}

}  // namespace blink

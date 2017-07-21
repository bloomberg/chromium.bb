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
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/loader/MixedContentChecker.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionCallbacks.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationError.h"

namespace blink {

namespace {

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

    if (!parsed_url.IsValid()) {
      exception_state.ThrowDOMException(
          kSyntaxError, "'" + urls[i] + "' can't be resolved to a valid URL.");
      return nullptr;
    }

    if (parsed_url.ProtocolIsInHTTPFamily() &&
        MixedContentChecker::IsMixedContent(
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
  if (event_type == EventTypeNames::connectionavailable) {
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kPresentationRequestConnectionAvailableEventListener);
  }
}

bool PresentationRequest::HasPendingActivity() const {
  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  if (!GetExecutionContext())
    return false;

  if (HasEventListeners())
    return true;

  return availability_property_ && availability_property_->GetState() ==
                                       ScriptPromisePropertyBase::kPending;
}

// static
void PresentationRequest::RecordStartOriginTypeAccess(
    ExecutionContext& execution_context) {
  if (execution_context.IsSecureContext()) {
    UseCounter::Count(&execution_context,
                      WebFeature::kPresentationRequestStartSecureOrigin);
  } else {
    Deprecation::CountDeprecation(
        &execution_context,
        WebFeature::kPresentationRequestStartInsecureOrigin);
  }
}

ScriptPromise PresentationRequest::start(ScriptState* script_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  Settings* context_settings = GetSettings(execution_context);
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

  WebPresentationClient* client =
      PresentationController::ClientFromContext(execution_context);
  if (!client)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  RecordStartOriginTypeAccess(*execution_context);
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  client->StartPresentation(
      urls_, WTF::MakeUnique<PresentationConnectionCallbacks>(resolver, this));
  return resolver->Promise();
}

ScriptPromise PresentationRequest::reconnect(ScriptState* script_state,
                                             const String& id) {
  WebPresentationClient* client =
      PresentationController::ClientFromContext(GetExecutionContext());
  if (!client)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  DCHECK(controller);

  PresentationConnection* existing_connection =
      controller->FindExistingConnection(urls_, id);
  if (existing_connection) {
    client->ReconnectPresentation(
        urls_, id,
        WTF::MakeUnique<PresentationConnectionCallbacks>(resolver,
                                                         existing_connection));
  } else {
    client->ReconnectPresentation(
        urls_, id,
        WTF::MakeUnique<PresentationConnectionCallbacks>(resolver, this));
  }
  return resolver->Promise();
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* script_state) {
  WebPresentationClient* client =
      PresentationController::ClientFromContext(GetExecutionContext());
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
  RecordConstructorOriginTypeAccess(*execution_context);
}

// static
void PresentationRequest::RecordConstructorOriginTypeAccess(
    ExecutionContext& execution_context) {
  if (execution_context.IsSecureContext()) {
    UseCounter::Count(&execution_context,
                      WebFeature::kPresentationRequestSecureOrigin);
  } else {
    UseCounter::Count(&execution_context,
                      WebFeature::kPresentationRequestInsecureOrigin);
  }
}

}  // namespace blink

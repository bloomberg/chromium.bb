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
PresentationController* presentationController(
    ExecutionContext* executionContext) {
  DCHECK(executionContext);

  Document* document = toDocument(executionContext);
  if (!document->frame())
    return nullptr;
  return PresentationController::from(*document->frame());
}

WebPresentationClient* presentationClient(ExecutionContext* executionContext) {
  PresentationController* controller = presentationController(executionContext);
  return controller ? controller->client() : nullptr;
}

Settings* settings(ExecutionContext* executionContext) {
  DCHECK(executionContext);

  Document* document = toDocument(executionContext);
  return document->settings();
}

ScriptPromise rejectWithSandBoxException(ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState, DOMException::create(SecurityError,
                                        "The document is sandboxed and lacks "
                                        "the 'allow-presentation' flag."));
}

}  // anonymous namespace

// static
PresentationRequest* PresentationRequest::create(
    ExecutionContext* executionContext,
    const String& url,
    ExceptionState& exceptionState) {
  Vector<String> urls(1);
  urls[0] = url;
  return create(executionContext, urls, exceptionState);
}

PresentationRequest* PresentationRequest::create(
    ExecutionContext* executionContext,
    const Vector<String>& urls,
    ExceptionState& exceptionState) {
  if (urls.isEmpty()) {
    exceptionState.throwDOMException(NotSupportedError,
                                     "Do not support empty sequence of URLs.");
    return nullptr;
  }

  Vector<KURL> parsedUrls(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    const KURL& parsedUrl = KURL(executionContext->url(), urls[i]);

    if (!parsedUrl.isValid() ||
        !(parsedUrl.protocolIsInHTTPFamily() || parsedUrl.protocolIs("cast"))) {
      exceptionState.throwDOMException(
          SyntaxError, "'" + urls[i] + "' can't be resolved to a valid URL.");
      return nullptr;
    }

    if (MixedContentChecker::isMixedContent(
            executionContext->getSecurityOrigin(), parsedUrl)) {
      exceptionState.throwSecurityError(
          "Presentation of an insecure document [" + urls[i] +
          "] is prohibited from a secure context.");
      return nullptr;
    }

    parsedUrls[i] = parsedUrl;
  }
  return new PresentationRequest(executionContext, parsedUrls);
}

const AtomicString& PresentationRequest::interfaceName() const {
  return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::getExecutionContext() const {
  return ContextClient::getExecutionContext();
}

void PresentationRequest::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
  if (eventType == EventTypeNames::connectionavailable)
    UseCounter::count(
        getExecutionContext(),
        UseCounter::PresentationRequestConnectionAvailableEventListener);
}

bool PresentationRequest::hasPendingActivity() const {
  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  return getExecutionContext() && hasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* scriptState) {
  Settings* contextSettings = settings(getExecutionContext());
  bool isUserGestureRequired =
      !contextSettings || contextSettings->getPresentationRequiresUserGesture();

  if (isUserGestureRequired && !UserGestureIndicator::utilizeUserGesture())
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidAccessError,
            "PresentationRequest::start() requires user gesture."));

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  client->startSession(
      m_urls, WTF::makeUnique<PresentationConnectionCallbacks>(resolver, this));
  return resolver->promise();
}

ScriptPromise PresentationRequest::reconnect(ScriptState* scriptState,
                                             const String& id) {
  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);

  PresentationController* controller =
      presentationController(getExecutionContext());
  DCHECK(controller);

  PresentationConnection* existingConnection =
      controller->findExistingConnection(m_urls, id);
  if (existingConnection) {
    client->joinSession(
        m_urls, id, WTF::makeUnique<ExistingPresentationConnectionCallbacks>(
                        resolver, existingConnection));
  } else {
    client->joinSession(
        m_urls, id,
        WTF::makeUnique<PresentationConnectionCallbacks>(resolver, this));
  }
  return resolver->promise();
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* scriptState) {
  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  if (!m_availabilityProperty) {
    m_availabilityProperty = new PresentationAvailabilityProperty(
        scriptState->getExecutionContext(), this,
        PresentationAvailabilityProperty::Ready);

    client->getAvailability(m_urls,
                            WTF::makeUnique<PresentationAvailabilityCallbacks>(
                                m_availabilityProperty, m_urls));
  }
  return m_availabilityProperty->promise(scriptState->world());
}

const Vector<KURL>& PresentationRequest::urls() const {
  return m_urls;
}

DEFINE_TRACE(PresentationRequest) {
  visitor->trace(m_availabilityProperty);
  EventTargetWithInlineData::trace(visitor);
  ContextClient::trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* executionContext,
                                         const Vector<KURL>& urls)
    : ContextClient(executionContext), m_urls(urls) {
  recordOriginTypeAccess(executionContext);
}

void PresentationRequest::recordOriginTypeAccess(
    ExecutionContext* executionContext) const {
  DCHECK(executionContext);
  if (executionContext->isSecureContext()) {
    UseCounter::count(executionContext,
                      UseCounter::PresentationRequestSecureOrigin);
  } else {
    UseCounter::count(executionContext,
                      UseCounter::PresentationRequestInsecureOrigin);
  }
}

}  // namespace blink

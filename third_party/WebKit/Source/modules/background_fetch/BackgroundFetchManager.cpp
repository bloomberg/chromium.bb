// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "bindings/modules/v8/RequestOrUSVString.h"
#include "bindings/modules/v8/RequestOrUSVStringOrRequestOrUSVStringSequence.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/BackgroundFetchOptions.h"
#include "modules/background_fetch/BackgroundFetchRegistration.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

namespace {

// Message for the TypeError thrown when an empty request sequence is seen.
const char kEmptyRequestSequenceErrorMessage[] =
    "At least one request must be given.";

// Message for the TypeError thrown when a null request is seen.
const char kNullRequestErrorMessage[] = "Requests must not be null.";

}  // namespace

BackgroundFetchManager::BackgroundFetchManager(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
  m_bridge = BackgroundFetchBridge::from(m_registration);
}

ScriptPromise BackgroundFetchManager::fetch(
    ScriptState* scriptState,
    const String& tag,
    const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
    const BackgroundFetchOptions& options,
    ExceptionState& exceptionState) {
  if (!m_registration->active()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scriptState, requests, exceptionState);
  if (exceptionState.hadException())
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_bridge->fetch(tag, std::move(webRequests), options,
                  WTF::bind(&BackgroundFetchManager::didFetch,
                            wrapPersistent(this), wrapPersistent(resolver)));

  return promise;
}

void BackgroundFetchManager::didFetch(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error,
    BackgroundFetchRegistration* registration) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      DCHECK(registration);
      resolver->resolve(registration);
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_TAG:
      DCHECK(!registration);
      resolver->reject(DOMException::create(
          InvalidStateError,
          "There already is a registration for the given tag."));
      return;
    case mojom::blink::BackgroundFetchError::INVALID_TAG:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

ScriptPromise BackgroundFetchManager::get(ScriptState* scriptState,
                                          const String& tag) {
  if (!m_registration->active()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_bridge->getRegistration(
      tag, WTF::bind(&BackgroundFetchManager::didGetRegistration,
                     wrapPersistent(this), wrapPersistent(resolver)));

  return promise;
}

// static
Vector<WebServiceWorkerRequest> BackgroundFetchManager::createWebRequestVector(
    ScriptState* scriptState,
    const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
    ExceptionState& exceptionState) {
  Vector<WebServiceWorkerRequest> webRequests;

  if (requests.isRequestOrUSVStringSequence()) {
    HeapVector<RequestOrUSVString> requestVector =
        requests.getAsRequestOrUSVStringSequence();

    // Throw a TypeError when the developer has passed an empty sequence.
    if (!requestVector.size()) {
      exceptionState.throwTypeError(kEmptyRequestSequenceErrorMessage);
      return Vector<WebServiceWorkerRequest>();
    }

    webRequests.resize(requestVector.size());

    for (size_t i = 0; i < requestVector.size(); ++i) {
      const RequestOrUSVString& requestOrUrl = requestVector[i];

      Request* request = nullptr;
      if (requestOrUrl.isRequest()) {
        request = requestOrUrl.getAsRequest();
      } else if (requestOrUrl.isUSVString()) {
        request = Request::create(scriptState, requestOrUrl.getAsUSVString(),
                                  exceptionState);
        if (exceptionState.hadException())
          return Vector<WebServiceWorkerRequest>();
      } else {
        exceptionState.throwTypeError(kNullRequestErrorMessage);
        return Vector<WebServiceWorkerRequest>();
      }

      DCHECK(request);
      request->populateWebServiceWorkerRequest(webRequests[i]);
    }
  } else if (requests.isRequest()) {
    DCHECK(requests.getAsRequest());
    webRequests.resize(1);
    requests.getAsRequest()->populateWebServiceWorkerRequest(webRequests[0]);
  } else if (requests.isUSVString()) {
    Request* request =
        Request::create(scriptState, requests.getAsUSVString(), exceptionState);
    if (exceptionState.hadException())
      return Vector<WebServiceWorkerRequest>();

    DCHECK(request);
    webRequests.resize(1);
    request->populateWebServiceWorkerRequest(webRequests[0]);
  } else {
    exceptionState.throwTypeError(kNullRequestErrorMessage);
    return Vector<WebServiceWorkerRequest>();
  }

  return webRequests;
}

void BackgroundFetchManager::didGetRegistration(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error,
    BackgroundFetchRegistration* registration) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      resolver->resolve(registration);
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_TAG:
    case mojom::blink::BackgroundFetchError::INVALID_TAG:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

ScriptPromise BackgroundFetchManager::getTags(ScriptState* scriptState) {
  if (!m_registration->active()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_bridge->getTags(WTF::bind(&BackgroundFetchManager::didGetTags,
                              wrapPersistent(this), wrapPersistent(resolver)));

  return promise;
}

void BackgroundFetchManager::didGetTags(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error,
    const Vector<String>& tags) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      resolver->resolve(tags);
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_TAG:
    case mojom::blink::BackgroundFetchError::INVALID_TAG:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

DEFINE_TRACE(BackgroundFetchManager) {
  visitor->trace(m_registration);
  visitor->trace(m_bridge);
}

}  // namespace blink

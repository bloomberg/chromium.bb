// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/BackgroundFetchOptions.h"
#include "modules/background_fetch/BackgroundFetchRegistration.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

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
    const BackgroundFetchOptions& options) {
  if (!m_registration->active()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // TODO(peter): Register the fetch() with the browser process. The reject
  // cases there are storage errors and duplicated registrations for the `tag`
  // given the `m_registration`.
  BackgroundFetchRegistration* registration = new BackgroundFetchRegistration(
      m_registration.get(), tag, options.icons(), options.totalDownloadSize(),
      options.title());

  resolver->resolve(registration);

  return promise;
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

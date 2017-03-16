// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "modules/background_fetch/BackgroundFetchOptions.h"
#include "modules/background_fetch/BackgroundFetchRegistration.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

BackgroundFetchManager::BackgroundFetchManager(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
}

ScriptPromise BackgroundFetchManager::fetch(
    ScriptState* scriptState,
    String tag,
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
                                          String tag) {
  if (!m_registration->active()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // TODO(peter): Get the background fetch registration for `tag` from the
  // browser process. There may not be one.
  resolver->resolve(v8::Null(scriptState->isolate()));

  return promise;
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

  // TODO(peter): Get a list of tags from the browser process.
  resolver->resolve(Vector<String>());

  return promise;
}

DEFINE_TRACE(BackgroundFetchManager) {
  visitor->trace(m_registration);
}

}  // namespace blink

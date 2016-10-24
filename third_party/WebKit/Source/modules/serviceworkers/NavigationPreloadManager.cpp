// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigationPreloadManager.h"

#include "core/dom/DOMException.h"
#include "modules/serviceworkers/NavigationPreloadCallbacks.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

ScriptPromise NavigationPreloadManager::enable(ScriptState* scriptState) {
  return setEnabled(true, scriptState);
}

ScriptPromise NavigationPreloadManager::disable(ScriptState* scriptState) {
  return setEnabled(false, scriptState);
}

ScriptPromise NavigationPreloadManager::setHeaderValue(ScriptState*,
                                                       const String& value) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise NavigationPreloadManager::getState(ScriptState*) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

NavigationPreloadManager::NavigationPreloadManager(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {}

ScriptPromise NavigationPreloadManager::setEnabled(bool enable,
                                                   ScriptState* scriptState) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::from(m_registration->getExecutionContext());
  if (!client || !client->provider()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError, "No provider."));
  }
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_registration->webRegistration()->enableNavigationPreload(
      enable, client->provider(),
      wrapUnique(new EnableNavigationPreloadCallbacks(resolver)));
  return promise;
}

DEFINE_TRACE(NavigationPreloadManager) {
  visitor->trace(m_registration);
}

}  // namespace blink

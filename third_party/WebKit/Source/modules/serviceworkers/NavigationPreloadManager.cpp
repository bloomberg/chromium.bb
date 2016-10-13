// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigationPreloadManager.h"

#include "modules/serviceworkers/NavigationPreloadCallbacks.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

ScriptPromise NavigationPreloadManager::enable(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_registration->webRegistration()->enableNavigationPreload(
      new EnableNavigationPreloadCallbacks(resolver));
  return promise;
}

ScriptPromise NavigationPreloadManager::disable(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_registration->webRegistration()->disableNavigationPreload(
      new DisableNavigationPreloadCallbacks(resolver));
  return promise;
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

DEFINE_TRACE(NavigationPreloadManager) {
  visitor->trace(m_registration);
}

}  // namespace blink

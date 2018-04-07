// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serviceworkers/navigation_preload_manager.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/serviceworkers/navigation_preload_callbacks.h"
#include "third_party/blink/renderer/modules/serviceworkers/service_worker_container_client.h"
#include "third_party/blink/renderer/modules/serviceworkers/service_worker_registration.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"

namespace blink {

ScriptPromise NavigationPreloadManager::enable(ScriptState* script_state) {
  return SetEnabled(true, script_state);
}

ScriptPromise NavigationPreloadManager::disable(ScriptState* script_state) {
  return SetEnabled(false, script_state);
}

ScriptPromise NavigationPreloadManager::setHeaderValue(
    ScriptState* script_state,
    const String& value) {
  if (!IsValidHTTPHeaderValue(value)) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "The string provided to setHeaderValue ('" + value +
                              "') is not a valid HTTP header field value."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  registration_->WebRegistration()->SetNavigationPreloadHeader(
      value, std::make_unique<SetNavigationPreloadHeaderCallbacks>(resolver));
  return promise;
}

ScriptPromise NavigationPreloadManager::getState(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  registration_->WebRegistration()->GetNavigationPreloadState(
      std::make_unique<GetNavigationPreloadStateCallbacks>(resolver));
  return promise;
}

NavigationPreloadManager::NavigationPreloadManager(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ScriptPromise NavigationPreloadManager::SetEnabled(bool enable,
                                                   ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  registration_->WebRegistration()->EnableNavigationPreload(
      enable, std::make_unique<EnableNavigationPreloadCallbacks>(resolver));
  return promise;
}

void NavigationPreloadManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

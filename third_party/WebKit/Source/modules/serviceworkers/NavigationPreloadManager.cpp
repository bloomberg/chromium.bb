// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigationPreloadManager.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/NavigationPreloadCallbacks.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/network/HTTPParsers.h"

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
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::From(registration_->GetExecutionContext());
  if (!client || !client->Provider()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, "No provider."));
  }

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
      value, client->Provider(),
      WTF::MakeUnique<SetNavigationPreloadHeaderCallbacks>(resolver));
  return promise;
}

ScriptPromise NavigationPreloadManager::getState(ScriptState* script_state) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::From(registration_->GetExecutionContext());
  if (!client || !client->Provider()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, "No provider."));
  }
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  registration_->WebRegistration()->GetNavigationPreloadState(
      client->Provider(),
      WTF::MakeUnique<GetNavigationPreloadStateCallbacks>(resolver));
  return promise;
}

NavigationPreloadManager::NavigationPreloadManager(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ScriptPromise NavigationPreloadManager::SetEnabled(bool enable,
                                                   ScriptState* script_state) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::From(registration_->GetExecutionContext());
  if (!client || !client->Provider()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, "No provider."));
  }
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  registration_->WebRegistration()->EnableNavigationPreload(
      enable, client->Provider(),
      WTF::MakeUnique<EnableNavigationPreloadCallbacks>(resolver));
  return promise;
}

void NavigationPreloadManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerWindowClient.h"

#include <memory>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/page/PageVisibilityState.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLocation.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClientCallback.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebString.h"

namespace blink {

ServiceWorkerWindowClient* ServiceWorkerWindowClient::Take(
    ScriptPromiseResolver*,
    std::unique_ptr<WebServiceWorkerClientInfo> web_client) {
  return web_client ? ServiceWorkerWindowClient::Create(*web_client) : nullptr;
}

ServiceWorkerWindowClient* ServiceWorkerWindowClient::Create(
    const WebServiceWorkerClientInfo& info) {
  return new ServiceWorkerWindowClient(info);
}

ServiceWorkerWindowClient::ServiceWorkerWindowClient(
    const WebServiceWorkerClientInfo& info)
    : ServiceWorkerClient(info),
      page_visibility_state_(info.page_visibility_state),
      is_focused_(info.is_focused) {}

ServiceWorkerWindowClient::~ServiceWorkerWindowClient() {}

String ServiceWorkerWindowClient::visibilityState() const {
  return PageVisibilityStateString(
      static_cast<PageVisibilityState>(page_visibility_state_));
}

ScriptPromise ServiceWorkerWindowClient::focus(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ExecutionContext::From(script_state)->IsWindowInteractionAllowed()) {
    resolver->Reject(DOMException::Create(kInvalidAccessError,
                                          "Not allowed to focus a window."));
    return promise;
  }
  ExecutionContext::From(script_state)->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(ExecutionContext::From(script_state))
      ->Focus(Uuid(),
              std::make_unique<CallbackPromiseAdapter<ServiceWorkerWindowClient,
                                                      ServiceWorkerError>>(
                  resolver));
  return promise;
}

ScriptPromise ServiceWorkerWindowClient::navigate(ScriptState* script_state,
                                                  const String& url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  KURL parsed_url = KURL(ToWorkerGlobalScope(context)->location()->Url(), url);
  if (!parsed_url.IsValid() || parsed_url.ProtocolIsAbout()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }
  if (!context->GetSecurityOrigin()->CanDisplay(parsed_url)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "'" + parsed_url.ElidedString() + "' cannot navigate."));
    return promise;
  }

  ServiceWorkerGlobalScopeClient::From(context)->Navigate(
      Uuid(), parsed_url, std::make_unique<NavigateClientCallback>(resolver));
  return promise;
}

void ServiceWorkerWindowClient::Trace(blink::Visitor* visitor) {
  ServiceWorkerClient::Trace(visitor);
}

}  // namespace blink

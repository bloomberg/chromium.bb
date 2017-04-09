// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerWindowClient.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/PageVisibilityState.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLocation.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClientCallback.h"
#include "public/platform/WebString.h"
#include "wtf/RefPtr.h"
#include <memory>

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

  if (!script_state->GetExecutionContext()->IsWindowInteractionAllowed()) {
    resolver->Reject(DOMException::Create(kInvalidAccessError,
                                          "Not allowed to focus a window."));
    return promise;
  }
  script_state->GetExecutionContext()->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(script_state->GetExecutionContext())
      ->Focus(Uuid(),
              WTF::MakeUnique<CallbackPromiseAdapter<ServiceWorkerWindowClient,
                                                     ServiceWorkerError>>(
                  resolver));
  return promise;
}

ScriptPromise ServiceWorkerWindowClient::navigate(ScriptState* script_state,
                                                  const String& url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = script_state->GetExecutionContext();

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
      Uuid(), parsed_url, WTF::MakeUnique<NavigateClientCallback>(resolver));
  return promise;
}

DEFINE_TRACE(ServiceWorkerWindowClient) {
  ServiceWorkerClient::Trace(visitor);
}

}  // namespace blink

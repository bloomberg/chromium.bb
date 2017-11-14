// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerClients.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLocation.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClientCallback.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientQueryOptions.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"

namespace blink {

namespace {

class ClientArray {
 public:
  using WebType = const WebServiceWorkerClientsInfo&;
  static HeapVector<Member<ServiceWorkerClient>> Take(
      ScriptPromiseResolver*,
      const WebServiceWorkerClientsInfo& web_clients) {
    HeapVector<Member<ServiceWorkerClient>> clients;
    for (size_t i = 0; i < web_clients.clients.size(); ++i) {
      const WebServiceWorkerClientInfo& client = web_clients.clients[i];
      if (client.client_type == kWebServiceWorkerClientTypeWindow)
        clients.push_back(ServiceWorkerWindowClient::Create(client));
      else
        clients.push_back(ServiceWorkerClient::Create(client));
    }
    return clients;
  }

 private:
  WTF_MAKE_NONCOPYABLE(ClientArray);
  ClientArray() = delete;
};

WebServiceWorkerClientType GetClientType(const String& type) {
  if (type == "window")
    return kWebServiceWorkerClientTypeWindow;
  if (type == "worker")
    return kWebServiceWorkerClientTypeWorker;
  if (type == "sharedworker")
    return kWebServiceWorkerClientTypeSharedWorker;
  if (type == "all")
    return kWebServiceWorkerClientTypeAll;
  NOTREACHED();
  return kWebServiceWorkerClientTypeWindow;
}

class GetCallback : public WebServiceWorkerClientCallbacks {
 public:
  explicit GetCallback(ScriptPromiseResolver* resolver) : resolver_(resolver) {}
  ~GetCallback() override {}

  void OnSuccess(
      std::unique_ptr<WebServiceWorkerClientInfo> web_client) override {
    std::unique_ptr<WebServiceWorkerClientInfo> client =
        WTF::WrapUnique(web_client.release());
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (!client) {
      // Resolve the promise with undefined.
      resolver_->Resolve();
      return;
    }
    resolver_->Resolve(ServiceWorkerClient::Take(resolver_, std::move(client)));
  }

  void OnError(const WebServiceWorkerError& error) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Reject(ServiceWorkerError::Take(resolver_.Get(), error));
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  WTF_MAKE_NONCOPYABLE(GetCallback);
};

}  // namespace

ServiceWorkerClients* ServiceWorkerClients::Create() {
  return new ServiceWorkerClients();
}

ServiceWorkerClients::ServiceWorkerClients() {}

ScriptPromise ServiceWorkerClients::get(ScriptState* script_state,
                                        const String& id) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // TODO(jungkees): May be null due to worker termination:
  // http://crbug.com/413518.
  if (!execution_context)
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->GetClient(id, std::make_unique<GetCallback>(resolver));
  return promise;
}

ScriptPromise ServiceWorkerClients::matchAll(
    ScriptState* script_state,
    const ClientQueryOptions& options) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!execution_context)
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  WebServiceWorkerClientQueryOptions web_options;
  web_options.client_type = GetClientType(options.type());
  web_options.include_uncontrolled = options.includeUncontrolled();
  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->GetClients(web_options,
                   std::make_unique<
                       CallbackPromiseAdapter<ClientArray, ServiceWorkerError>>(
                       resolver));
  return promise;
}

ScriptPromise ServiceWorkerClients::claim(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!execution_context)
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto callbacks =
      std::make_unique<CallbackPromiseAdapter<void, ServiceWorkerError>>(
          resolver);
  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->Claim(std::move(callbacks));
  return promise;
}

ScriptPromise ServiceWorkerClients::openWindow(ScriptState* script_state,
                                               const String& url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  KURL parsed_url = KURL(ToWorkerGlobalScope(context)->location()->Url(), url);
  if (!parsed_url.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }

  if (!context->GetSecurityOrigin()->CanDisplay(parsed_url)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "'" + parsed_url.ElidedString() + "' cannot be opened."));
    return promise;
  }

  if (!context->IsWindowInteractionAllowed()) {
    resolver->Reject(DOMException::Create(kInvalidAccessError,
                                          "Not allowed to open a window."));
    return promise;
  }
  context->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(context)->OpenWindowForClients(
      parsed_url, std::make_unique<NavigateClientCallback>(resolver));
  return promise;
}

}  // namespace blink

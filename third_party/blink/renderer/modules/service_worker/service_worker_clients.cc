// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_clients.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_clients_info.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client_callback.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

mojom::ServiceWorkerClientType GetClientType(const String& type) {
  if (type == "window")
    return mojom::ServiceWorkerClientType::kWindow;
  if (type == "sharedworker")
    return mojom::ServiceWorkerClientType::kSharedWorker;
  if (type == "all")
    return mojom::ServiceWorkerClientType::kAll;
  NOTREACHED();
  return mojom::ServiceWorkerClientType::kWindow;
}

class GetCallback : public WebServiceWorkerClientCallbacks {
 public:
  explicit GetCallback(ScriptPromiseResolver* resolver) : resolver_(resolver) {}
  ~GetCallback() override = default;

  void OnSuccess(
      std::unique_ptr<WebServiceWorkerClientInfo> web_client) override {
    std::unique_ptr<WebServiceWorkerClientInfo> client =
        base::WrapUnique(web_client.release());
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
  DISALLOW_COPY_AND_ASSIGN(GetCallback);
};

void DidClaim(ScriptPromiseResolver* resolver,
              mojom::blink::ServiceWorkerErrorType error,
              const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  if (error != mojom::blink::ServiceWorkerErrorType::kNone) {
    DCHECK(!error_msg.IsNull());
    resolver->Reject(
        ServiceWorkerError::GetException(resolver, error, error_msg));
    return;
  }
  DCHECK(error_msg.IsNull());
  resolver->Resolve();
}

void DidGetClients(ScriptPromiseResolver* resolver,
                   Vector<mojom::blink::ServiceWorkerClientInfoPtr> infos) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  HeapVector<Member<ServiceWorkerClient>> clients;
  for (const auto& info : infos) {
    if (info->client_type == mojom::blink::ServiceWorkerClientType::kWindow)
      clients.push_back(ServiceWorkerWindowClient::Create(*info));
    else
      clients.push_back(ServiceWorkerClient::Create(*info));
  }
  resolver->Resolve(std::move(clients));
}

}  // namespace

ServiceWorkerClients* ServiceWorkerClients::Create() {
  return new ServiceWorkerClients();
}

ServiceWorkerClients::ServiceWorkerClients() = default;

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
  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->GetClients(
          mojom::blink::ServiceWorkerClientQueryOptions::New(
              options.includeUncontrolled(), GetClientType(options.type())),
          WTF::Bind(&DidGetClients, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise ServiceWorkerClients::claim(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!execution_context)
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->Claim(WTF::Bind(&DidClaim, WrapPersistent(resolver)));
  return resolver->Promise();
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
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidAccessError,
                                          "Not allowed to open a window."));
    return promise;
  }
  context->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(context)->OpenWindowForClients(
      parsed_url, std::make_unique<NavigateClientCallback>(resolver));
  return promise;
}

}  // namespace blink

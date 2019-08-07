/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

void DidNavigateOrOpenWindow(ScriptPromiseResolver* resolver,
                             bool success,
                             mojom::blink::ServiceWorkerClientInfoPtr info,
                             const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (!success) {
    DCHECK(!info);
    DCHECK(!error_msg.IsNull());
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowException::CreateTypeError(
        resolver->GetScriptState()->GetIsolate(), error_msg));
    return;
  }
  ServiceWorkerWindowClient* window_client = nullptr;
  // Even if the open/navigation succeeded, |info| may be null if information of
  // the opened/navigated window could not be obtained (this can happen for a
  // cross-origin window, or if the browser process could not get the
  // information in time before the window was closed).
  if (info)
    window_client = ServiceWorkerWindowClient::Create(*info);
  resolver->Resolve(window_client);
}

}  // namespace

ServiceWorkerGlobalScopeClient::ServiceWorkerGlobalScopeClient(
    WebServiceWorkerContextClient& client)
    : client_(client) {}

void ServiceWorkerGlobalScopeClient::GetClient(const String& id,
                                               GetClientCallback callback) {
  service_worker_host_->GetClient(id, std::move(callback));
}

void ServiceWorkerGlobalScopeClient::GetClients(
    mojom::blink::ServiceWorkerClientQueryOptionsPtr options,
    GetClientsCallback callback) {
  service_worker_host_->GetClients(std::move(options), std::move(callback));
}

void ServiceWorkerGlobalScopeClient::OpenWindowForClients(
    const KURL& url,
    ScriptPromiseResolver* resolver) {
  service_worker_host_->OpenNewTab(
      url, WTF::Bind(&DidNavigateOrOpenWindow, WrapPersistent(resolver)));
}

void ServiceWorkerGlobalScopeClient::OpenWindowForPaymentHandler(
    const KURL& url,
    ScriptPromiseResolver* resolver) {
  service_worker_host_->OpenPaymentHandlerWindow(
      url, WTF::Bind(&DidNavigateOrOpenWindow, WrapPersistent(resolver)));
}

void ServiceWorkerGlobalScopeClient::SetCachedMetadata(const KURL& url,
                                                       const uint8_t* data,
                                                       size_t size) {
  Vector<uint8_t> meta_data;
  meta_data.Append(data, SafeCast<wtf_size_t>(size));
  service_worker_host_->SetCachedMetadata(url, meta_data);
}

void ServiceWorkerGlobalScopeClient::ClearCachedMetadata(const KURL& url) {
  service_worker_host_->ClearCachedMetadata(url);
}

void ServiceWorkerGlobalScopeClient::PostMessageToClient(
    const String& client_uuid,
    BlinkTransferableMessage message) {
  service_worker_host_->PostMessageToClient(client_uuid, std::move(message));
}

void ServiceWorkerGlobalScopeClient::SkipWaiting(SkipWaitingCallback callback) {
  service_worker_host_->SkipWaiting(std::move(callback));
}

void ServiceWorkerGlobalScopeClient::Claim(ClaimCallback callback) {
  service_worker_host_->ClaimClients(std::move(callback));
}

void ServiceWorkerGlobalScopeClient::Focus(const String& client_uuid,
                                           FocusCallback callback) {
  service_worker_host_->FocusClient(client_uuid, std::move(callback));
}

void ServiceWorkerGlobalScopeClient::Navigate(const String& client_uuid,
                                              const KURL& url,
                                              ScriptPromiseResolver* resolver) {
  service_worker_host_->NavigateClient(
      client_uuid, url,
      WTF::Bind(&DidNavigateOrOpenWindow, WrapPersistent(resolver)));
}

void ServiceWorkerGlobalScopeClient::BindServiceWorkerHost(
    mojom::blink::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(service_worker_host.is_valid());
  DCHECK(!service_worker_host_);
  service_worker_host_.Bind(std::move(service_worker_host),
                            std::move(task_runner));
}

void ServiceWorkerGlobalScopeClient::SetupNavigationPreload(
    int fetch_event_id,
    const KURL& url,
    mojom::blink::FetchEventPreloadHandlePtr preload_handle) {
  auto web_preload_handle = std::make_unique<WebFetchEventPreloadHandle>();
  web_preload_handle->url_loader = preload_handle->url_loader.PassHandle();
  web_preload_handle->url_loader_client_request =
      preload_handle->url_loader_client_request.PassMessagePipe();
  client_.SetupNavigationPreload(fetch_event_id, url,
                                 std::move(web_preload_handle));
}

void ServiceWorkerGlobalScopeClient::RequestTermination(
    base::OnceCallback<void(bool)> callback) {
  client_.RequestTermination(std::move(callback));
}

void ServiceWorkerGlobalScopeClient::WillDestroyWorkerContext() {
  service_worker_host_.reset();
}

const char ServiceWorkerGlobalScopeClient::kSupplementName[] =
    "ServiceWorkerGlobalScopeClient";

ServiceWorkerGlobalScopeClient* ServiceWorkerGlobalScopeClient::From(
    ExecutionContext* context) {
  // TODO(crbug.com/920854): Replace CHECK() with DCHECK() after crashes are
  // gone.
  CHECK(context);
  WorkerClients* worker_clients = To<WorkerGlobalScope>(context)->Clients();
  CHECK(worker_clients);
  ServiceWorkerGlobalScopeClient* client =
      Supplement<WorkerClients>::From<ServiceWorkerGlobalScopeClient>(
          worker_clients);
  CHECK(client);
  return client;
}

void ServiceWorkerGlobalScopeClient::Trace(blink::Visitor* visitor) {
  Supplement<WorkerClients>::Trace(visitor);
}

void ProvideServiceWorkerGlobalScopeClientToWorker(
    WorkerClients* clients,
    ServiceWorkerGlobalScopeClient* client) {
  clients->ProvideSupplement(client);
}

}  // namespace blink

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
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

}  // namespace

ServiceWorkerGlobalScopeClient::ServiceWorkerGlobalScopeClient(
    WebServiceWorkerContextClient& client)
    : client_(client) {}

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

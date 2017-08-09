// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerContainerClient.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"

namespace blink {

ServiceWorkerContainerClient::ServiceWorkerContainerClient(
    Document& document,
    std::unique_ptr<WebServiceWorkerProvider> provider)
    : Supplement<Document>(document), provider_(std::move(provider)) {}

ServiceWorkerContainerClient::ServiceWorkerContainerClient(
    WorkerClients& clients,
    std::unique_ptr<WebServiceWorkerProvider> provider)
    : Supplement<WorkerClients>(clients), provider_(std::move(provider)) {}

ServiceWorkerContainerClient::~ServiceWorkerContainerClient() {}

const char* ServiceWorkerContainerClient::SupplementName() {
  return "ServiceWorkerContainerClient";
}

ServiceWorkerContainerClient* ServiceWorkerContainerClient::From(
    ExecutionContext* context) {
  if (!context)
    return nullptr;
  if (context->IsWorkerGlobalScope()) {
    WorkerClients* worker_clients = ToWorkerGlobalScope(context)->Clients();
    DCHECK(worker_clients);
    ServiceWorkerContainerClient* client =
        static_cast<ServiceWorkerContainerClient*>(
            Supplement<WorkerClients>::From(worker_clients, SupplementName()));
    DCHECK(client);
    return client;
  }
  Document* document = ToDocument(context);
  if (!document->GetFrame() || !document->GetFrame()->Client())
    return nullptr;

  ServiceWorkerContainerClient* client =
      static_cast<ServiceWorkerContainerClient*>(
          Supplement<Document>::From(document, SupplementName()));
  if (!client) {
    client = new ServiceWorkerContainerClient(
        *document,
        document->GetFrame()->Client()->CreateServiceWorkerProvider());
    Supplement<Document>::ProvideTo(*document, SupplementName(), client);
  }
  return client;
}

void ProvideServiceWorkerContainerClientToWorker(
    WorkerClients* clients,
    std::unique_ptr<WebServiceWorkerProvider> provider) {
  clients->ProvideSupplement(
      ServiceWorkerContainerClient::SupplementName(),
      new ServiceWorkerContainerClient(*clients, std::move(provider)));
}

}  // namespace blink

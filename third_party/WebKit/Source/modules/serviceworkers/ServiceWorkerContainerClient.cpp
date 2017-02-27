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
    : Supplement<Document>(document), m_provider(std::move(provider)) {}

ServiceWorkerContainerClient::ServiceWorkerContainerClient(
    WorkerClients& clients,
    std::unique_ptr<WebServiceWorkerProvider> provider)
    : Supplement<WorkerClients>(clients), m_provider(std::move(provider)) {}

ServiceWorkerContainerClient::~ServiceWorkerContainerClient() {}

const char* ServiceWorkerContainerClient::supplementName() {
  return "ServiceWorkerContainerClient";
}

ServiceWorkerContainerClient* ServiceWorkerContainerClient::from(
    ExecutionContext* context) {
  if (!context)
    return nullptr;
  if (context->isWorkerGlobalScope()) {
    WorkerClients* workerClients = toWorkerGlobalScope(context)->clients();
    DCHECK(workerClients);
    ServiceWorkerContainerClient* client =
        static_cast<ServiceWorkerContainerClient*>(
            Supplement<WorkerClients>::from(workerClients, supplementName()));
    DCHECK(client);
    return client;
  }
  Document* document = toDocument(context);
  if (!document->frame())
    return nullptr;

  ServiceWorkerContainerClient* client =
      static_cast<ServiceWorkerContainerClient*>(
          Supplement<Document>::from(document, supplementName()));
  if (!client) {
    client = new ServiceWorkerContainerClient(
        *document,
        document->frame()->loader().client()->createServiceWorkerProvider());
    Supplement<Document>::provideTo(*document, supplementName(), client);
  }
  return client;
}

void provideServiceWorkerContainerClientToWorker(
    WorkerClients* clients,
    std::unique_ptr<WebServiceWorkerProvider> provider) {
  clients->provideSupplement(
      ServiceWorkerContainerClient::supplementName(),
      new ServiceWorkerContainerClient(*clients, std::move(provider)));
}

}  // namespace blink

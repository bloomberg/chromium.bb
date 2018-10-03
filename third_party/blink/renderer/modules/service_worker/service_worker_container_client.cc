// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_container_client.h"

#include <memory>
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace blink {

ServiceWorkerContainerClient::ServiceWorkerContainerClient(
    Document& document,
    std::unique_ptr<WebServiceWorkerProvider> provider)
    : Supplement<Document>(document), provider_(std::move(provider)) {}

ServiceWorkerContainerClient::~ServiceWorkerContainerClient() = default;

const char ServiceWorkerContainerClient::kSupplementName[] =
    "ServiceWorkerContainerClient";

ServiceWorkerContainerClient* ServiceWorkerContainerClient::From(
    ExecutionContext* context) {
  if (!context)
    return nullptr;
  Document* document = To<Document>(context);
  if (!document->GetFrame() || !document->GetFrame()->Client())
    return nullptr;

  ServiceWorkerContainerClient* client =
      Supplement<Document>::From<ServiceWorkerContainerClient>(document);
  if (!client) {
    client = new ServiceWorkerContainerClient(
        *document,
        document->GetFrame()->Client()->CreateServiceWorkerProvider());
    Supplement<Document>::ProvideTo(*document, client);
  }
  return client;
}

}  // namespace blink

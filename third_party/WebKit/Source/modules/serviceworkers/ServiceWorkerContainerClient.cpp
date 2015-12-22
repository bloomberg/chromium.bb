// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerContainerClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"

namespace blink {

PassOwnPtrWillBeRawPtr<ServiceWorkerContainerClient> ServiceWorkerContainerClient::create(PassOwnPtr<WebServiceWorkerProvider> provider)
{
    return adoptPtrWillBeNoop(new ServiceWorkerContainerClient(provider));
}

ServiceWorkerContainerClient::ServiceWorkerContainerClient(PassOwnPtr<WebServiceWorkerProvider> provider)
    : m_provider(provider)
{
}

ServiceWorkerContainerClient::~ServiceWorkerContainerClient()
{
}

const char* ServiceWorkerContainerClient::supplementName()
{
    return "ServiceWorkerContainerClient";
}

ServiceWorkerContainerClient* ServiceWorkerContainerClient::from(ExecutionContext* context)
{
    if (context->isWorkerGlobalScope()) {
        WorkerClients* clients = toWorkerGlobalScope(context)->clients();
        ASSERT(clients);
        return static_cast<ServiceWorkerContainerClient*>(WillBeHeapSupplement<WorkerClients>::from(clients, supplementName()));
    }
    Document* document = toDocument(context);
    if (!document->frame())
        return nullptr;

    ServiceWorkerContainerClient* client = static_cast<ServiceWorkerContainerClient*>(WillBeHeapSupplement<Document>::from(document, supplementName()));
    if (!client) {
        client = new ServiceWorkerContainerClient(document->frame()->loader().client()->createServiceWorkerProvider());
        WillBeHeapSupplement<Document>::provideTo(*document, supplementName(), adoptPtrWillBeNoop(client));
    }
    return client;
}

void provideServiceWorkerContainerClientToWorker(WorkerClients* clients, PassOwnPtr<WebServiceWorkerProvider> provider)
{
    clients->provideSupplement(ServiceWorkerContainerClient::supplementName(), ServiceWorkerContainerClient::create(provider));
}

} // namespace blink

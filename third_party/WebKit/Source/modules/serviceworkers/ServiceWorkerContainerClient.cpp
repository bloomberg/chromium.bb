// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ServiceWorkerContainerClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/WebServiceWorkerProvider.h"

namespace WebCore {

PassOwnPtr<ServiceWorkerContainerClient> ServiceWorkerContainerClient::create(PassOwnPtr<blink::WebServiceWorkerProvider> provider)
{
    return adoptPtr(new ServiceWorkerContainerClient(provider));
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
    if (context->isDocument()) {
        Document* document = toDocument(context);
        ServiceWorkerContainerClient* client = static_cast<ServiceWorkerContainerClient*>(Supplement<Page>::from(document->page(), supplementName()));
        if (client)
            return client;

        // If it's not provided yet create it lazily.
        ASSERT(document->frame());
        document->page()->provideSupplement(ServiceWorkerContainerClient::supplementName(), ServiceWorkerContainerClient::create(document->frame()->loader().client()->createServiceWorkerProvider()));
        return static_cast<ServiceWorkerContainerClient*>(Supplement<Page>::from(document->page(), supplementName()));
    }

    ASSERT(context->isWorkerGlobalScope());
    return static_cast<ServiceWorkerContainerClient*>(Supplement<WorkerClients>::from(toWorkerGlobalScope(context)->clients(), supplementName()));
}

ServiceWorkerContainerClient::ServiceWorkerContainerClient(PassOwnPtr<blink::WebServiceWorkerProvider> provider)
    : m_provider(provider)
{
}

void provideServiceWorkerContainerClientToWorker(WorkerClients* clients, PassOwnPtr<blink::WebServiceWorkerProvider> provider)
{
    clients->provideSupplement(ServiceWorkerContainerClient::supplementName(), ServiceWorkerContainerClient::create(provider));
}

} // namespace WebCore

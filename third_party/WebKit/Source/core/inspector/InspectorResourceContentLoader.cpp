// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorResourceContentLoader.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/page/Page.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class InspectorResourceContentLoader::ResourceClient final : public NoBaseWillBeGarbageCollectedFinalized<InspectorResourceContentLoader::ResourceClient>, private RawResourceClient, private StyleSheetResourceClient {
public:
    explicit ResourceClient(InspectorResourceContentLoader* loader)
        : m_loader(loader)
    {
    }

    void waitForResource(Resource* resource)
    {
        if (resource->type() == Resource::Raw)
            resource->addClient(static_cast<RawResourceClient*>(this));
        else
            resource->addClient(static_cast<StyleSheetResourceClient*>(this));
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_loader);
    }

private:
    RawPtrWillBeMember<InspectorResourceContentLoader> m_loader;

    void setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource*) override;
    void notifyFinished(Resource*) override;
    String debugName() const override { return "InspectorResourceContentLoader::ResourceClient"; }
    void resourceFinished(Resource*);

    friend class InspectorResourceContentLoader;
};

void InspectorResourceContentLoader::ResourceClient::resourceFinished(Resource* resource)
{
    if (m_loader)
        m_loader->resourceFinished(this);

    if (resource->type() == Resource::Raw)
        resource->removeClient(static_cast<RawResourceClient*>(this));
    else
        resource->removeClient(static_cast<StyleSheetResourceClient*>(this));

#if !ENABLE(OILPAN)
    delete this;
#endif
}

void InspectorResourceContentLoader::ResourceClient::setCSSStyleSheet(const String&, const KURL& url, const String&, const CSSStyleSheetResource* resource)
{
    resourceFinished(const_cast<CSSStyleSheetResource*>(resource));
}

void InspectorResourceContentLoader::ResourceClient::notifyFinished(Resource* resource)
{
    if (resource->type() == Resource::CSSStyleSheet)
        return;
    resourceFinished(resource);
}

InspectorResourceContentLoader::InspectorResourceContentLoader(LocalFrame* inspectedFrame)
    : m_allRequestsStarted(false)
    , m_started(false)
    , m_inspectedFrame(inspectedFrame)
{
}

void InspectorResourceContentLoader::start()
{
    m_started = true;
    WillBeHeapVector<RawPtrWillBeMember<Document>> documents;
    OwnPtrWillBeRawPtr<InspectedFrames> inspectedFrames = InspectedFrames::create(m_inspectedFrame);
    for (LocalFrame* frame : *inspectedFrames) {
        documents.append(frame->document());
        documents.appendVector(InspectorPageAgent::importsForFrame(frame));
    }
    for (Document* document : documents) {
        HashSet<String> urlsToFetch;

        ResourceRequest resourceRequest;
        HistoryItem* item = document->frame() ? document->frame()->loader().currentItem() : nullptr;
        if (item) {
            resourceRequest =
                FrameLoader::resourceRequestFromHistoryItem(item, ReturnCacheDataDontLoad);
        } else {
            resourceRequest = document->url();
            resourceRequest.setCachePolicy(ReturnCacheDataDontLoad);
        }
        resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);

        if (!resourceRequest.url().string().isEmpty()) {
            urlsToFetch.add(resourceRequest.url().string());
            FetchRequest request(resourceRequest, FetchInitiatorTypeNames::internal);
            RefPtrWillBeRawPtr<Resource> resource = RawResource::fetch(request, document->fetcher());
            if (resource) {
                // Prevent garbage collection by holding a reference to this resource.
                m_resources.append(resource.get());
                ResourceClient* resourceClient = new ResourceClient(this);
                m_pendingResourceClients.add(resourceClient);
                resourceClient->waitForResource(resource.get());
            }
        }

        WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> > styleSheets;
        InspectorCSSAgent::collectAllDocumentStyleSheets(document, styleSheets);
        for (CSSStyleSheet* styleSheet : styleSheets) {
            if (styleSheet->isInline() || !styleSheet->contents()->loadCompleted())
                continue;
            String url = styleSheet->baseURL().string();
            if (url.isEmpty() || urlsToFetch.contains(url))
                continue;
            urlsToFetch.add(url);
            FetchRequest request(ResourceRequest(url), FetchInitiatorTypeNames::internal);
            request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextInternal);
            RefPtrWillBeRawPtr<Resource> resource = CSSStyleSheetResource::fetch(request, document->fetcher());
            if (!resource)
                continue;
            // Prevent garbage collection by holding a reference to this resource.
            m_resources.append(resource.get());
            ResourceClient* resourceClient = new ResourceClient(this);
            m_pendingResourceClients.add(resourceClient);
            resourceClient->waitForResource(resource.get());
        }
    }

    m_allRequestsStarted = true;
    checkDone();
}

void InspectorResourceContentLoader::ensureResourcesContentLoaded(PassOwnPtr<Closure> callback)
{
    if (!m_started)
        start();
    m_callbacks.append(callback);
    checkDone();
}

InspectorResourceContentLoader::~InspectorResourceContentLoader()
{
    ASSERT(m_resources.isEmpty());
}

DEFINE_TRACE(InspectorResourceContentLoader)
{
#if ENABLE(OILPAN)
    visitor->trace(m_inspectedFrame);
    visitor->trace(m_pendingResourceClients);
    visitor->trace(m_resources);
#endif
}

void InspectorResourceContentLoader::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (frame == m_inspectedFrame)
        stop();
}

void InspectorResourceContentLoader::dispose()
{
    stop();
}

void InspectorResourceContentLoader::stop()
{
    WillBeHeapHashSet<RawPtrWillBeMember<ResourceClient>> pendingResourceClients;
    m_pendingResourceClients.swap(pendingResourceClients);
    for (const auto& client : pendingResourceClients)
        client->m_loader = nullptr;
    m_resources.clear();
    // Make sure all callbacks are called to prevent infinite waiting time.
    checkDone();
    m_allRequestsStarted = false;
    m_started = false;
}

bool InspectorResourceContentLoader::hasFinished()
{
    return m_allRequestsStarted && m_pendingResourceClients.size() == 0;
}

void InspectorResourceContentLoader::checkDone()
{
    if (!hasFinished())
        return;
    Vector<OwnPtr<Closure>> callbacks;
    callbacks.swap(m_callbacks);
    for (const auto& callback : callbacks)
        (*callback)();
}

void InspectorResourceContentLoader::resourceFinished(ResourceClient* client)
{
    m_pendingResourceClients.remove(client);
    checkDone();
}

} // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorResourceContentLoader.h"

#include "core/FetchInitiatorTypeNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourcePtr.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/page/Page.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class InspectorResourceContentLoader::ResourceClient FINAL : private RawResourceClient, private StyleSheetResourceClient {
public:
    ResourceClient(InspectorResourceContentLoader* loader)
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

private:
    InspectorResourceContentLoader* m_loader;

    virtual void setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource*) OVERRIDE;
    virtual void notifyFinished(Resource*) OVERRIDE;
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

    delete this;
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

InspectorResourceContentLoader::InspectorResourceContentLoader(Page* page)
    : m_allRequestsStarted(false)
    , m_started(false)
    , m_page(page)
{
}

void InspectorResourceContentLoader::start()
{
    m_started = true;
    Vector<Document*> documents;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        LocalFrame* localFrame = toLocalFrame(frame);
        documents.append(localFrame->document());
        documents.appendVector(InspectorPageAgent::importsForFrame(localFrame));
    }
    for (Vector<Document*>::const_iterator documentIt = documents.begin(); documentIt != documents.end(); ++documentIt) {
        Document* document = *documentIt;
        HashSet<String> urlsToFetch;

        ResourceRequest resourceRequest;
        HistoryItem* item = document->frame() ? document->frame()->loader().currentItem() : 0;
        if (item) {
            resourceRequest = FrameLoader::requestFromHistoryItem(item, ReturnCacheDataDontLoad);
        } else {
            resourceRequest = document->url();
            resourceRequest.setCachePolicy(ReturnCacheDataDontLoad);
        }
        resourceRequest.setRequestContext(blink::WebURLRequest::RequestContextInternal);

        if (!resourceRequest.url().string().isEmpty()) {
            urlsToFetch.add(resourceRequest.url().string());
            FetchRequest request(resourceRequest, FetchInitiatorTypeNames::internal);
            ResourcePtr<Resource> resource = document->fetcher()->fetchRawResource(request);
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
        for (WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> >::const_iterator stylesheetIt = styleSheets.begin(); stylesheetIt != styleSheets.end(); ++stylesheetIt) {
            CSSStyleSheet* styleSheet = *stylesheetIt;
            if (styleSheet->isInline() || !styleSheet->contents()->loadCompleted())
                continue;
            String url = styleSheet->baseURL().string();
            if (url.isEmpty() || urlsToFetch.contains(url))
                continue;
            urlsToFetch.add(url);
            FetchRequest request(ResourceRequest(url), FetchInitiatorTypeNames::internal);
            request.mutableResourceRequest().setRequestContext(blink::WebURLRequest::RequestContextInternal);
            ResourcePtr<Resource> resource = document->fetcher()->fetchCSSStyleSheet(request);
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

void InspectorResourceContentLoader::ensureResourcesContentLoaded(PassOwnPtr<VoidCallback> callback)
{
    if (!m_started)
        start();
    m_callbacks.append(callback);
    checkDone();
}

InspectorResourceContentLoader::~InspectorResourceContentLoader()
{
    stop();
}

void InspectorResourceContentLoader::stop()
{
    HashSet<ResourceClient*> pendingResourceClients;
    m_pendingResourceClients.swap(pendingResourceClients);
    for (HashSet<ResourceClient*>::const_iterator it = pendingResourceClients.begin(); it != pendingResourceClients.end(); ++it)
        (*it)->m_loader = 0;
    m_resources.clear();
    // Make sure all callbacks are called to prevent infinite waiting time.
    checkDone();
}

bool InspectorResourceContentLoader::hasFinished()
{
    return m_allRequestsStarted && m_pendingResourceClients.size() == 0;
}

void InspectorResourceContentLoader::checkDone()
{
    if (!hasFinished())
        return;
    Vector<OwnPtr<VoidCallback> > callbacks;
    callbacks.swap(m_callbacks);
    for (Vector<OwnPtr<VoidCallback> >::const_iterator it = callbacks.begin(); it != callbacks.end(); ++it)
        (*it)->handleEvent();
}

void InspectorResourceContentLoader::resourceFinished(ResourceClient* client)
{
    m_pendingResourceClients.remove(client);
    checkDone();
}

} // namespace blink

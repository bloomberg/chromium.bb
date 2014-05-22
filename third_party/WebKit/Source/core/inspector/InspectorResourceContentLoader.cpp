// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorResourceContentLoader.h"

#include "FetchInitiatorTypeNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourcePtr.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/page/Page.h"

namespace WebCore {

InspectorResourceContentLoader::InspectorResourceContentLoader(Page* page, PassOwnPtr<VoidCallback> callback)
    : m_callback(callback)
    , m_pendingResourceCount(1)
{
    Vector<Document*> documents;
    for (LocalFrame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        documents.append(frame->document());
        documents.appendVector(InspectorPageAgent::importsForFrame(frame));
    }
    for (Vector<Document*>::const_iterator it = documents.begin(); it != documents.end(); ++it) {
        HashSet<String> urlsToFetch;
        Document* document = *it;
        ResourceRequest resourceRequest;

        if (!document->frame() || !document->frame()->loader().requestFromHistoryForInspector(ReturnCacheDataDontLoad, &resourceRequest)) {
            resourceRequest = document->url();
            resourceRequest.setCachePolicy(ReturnCacheDataDontLoad);
        }

        if (!resourceRequest.url().string().isEmpty()) {
            urlsToFetch.add(resourceRequest.url().string());
            ++m_pendingResourceCount;
            FetchRequest request(resourceRequest, FetchInitiatorTypeNames::internal);
            ResourcePtr<Resource> resource = document->fetcher()->fetchRawResource(request);
            // Prevent garbage collection by holding a reference to this resource.
            m_pendingResources.add(resource.get());
            m_resources.append(resource.get());
            resource->addClient(static_cast<RawResourceClient*>(this));
        }

        Vector<CSSStyleSheet*> styleSheets;
        InspectorCSSAgent::collectAllDocumentStyleSheets(document, styleSheets);
        for (Vector<CSSStyleSheet*>::const_iterator it2 = styleSheets.begin(); it2 != styleSheets.end(); ++it2) {
            CSSStyleSheet* styleSheet = *it2;
            if (styleSheet->isInline() || !styleSheet->contents()->loadCompleted())
                continue;
            String url = styleSheet->contents()->baseURL().string();
            if (url.isEmpty() || urlsToFetch.contains(url))
                continue;
            urlsToFetch.add(url);
            ++m_pendingResourceCount;
            FetchRequest request(ResourceRequest(url), FetchInitiatorTypeNames::internal);
            ResourcePtr<Resource> resource = document->fetcher()->fetchCSSStyleSheet(request);
            // Prevent garbage collection by holding a reference to this resource.
            m_pendingResources.add(resource.get());
            m_resources.append(resource.get());
            resource->addClient(static_cast<StyleSheetResourceClient*>(this));
        }
    }

    // m_pendingResourceCount is set to 1 initially to prevent calling back before all request were sent in case they finish synchronously.
    worked();
}

InspectorResourceContentLoader::~InspectorResourceContentLoader()
{
    for (HashSet<Resource*>::const_iterator it = m_pendingResources.begin(); it != m_pendingResources.end(); ++it)
        removeAsClientFromResource(*it);
    m_pendingResources.clear();
}

void InspectorResourceContentLoader::removeAsClientFromResource(Resource* resource)
{
    if (resource->type() == Resource::Raw)
        resource->removeClient(static_cast<RawResourceClient*>(this));
    else
        resource->removeClient(static_cast<StyleSheetResourceClient*>(this));
}

bool InspectorResourceContentLoader::hasFinished()
{
    return m_pendingResourceCount == 0;
}

void InspectorResourceContentLoader::worked()
{
    --m_pendingResourceCount;
    if (!hasFinished())
        return;
    OwnPtr<VoidCallback> callback;
    callback = m_callback.release();
    return callback->handleEvent();
}

void InspectorResourceContentLoader::notifyFinished(Resource* resource)
{
    if (resource->type() == Resource::CSSStyleSheet)
        return;
    resourceFinished(resource);
}

void InspectorResourceContentLoader::setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource* resource)
{
    resourceFinished(const_cast<CSSStyleSheetResource*>(resource));
}

void InspectorResourceContentLoader::resourceFinished(Resource* resource)
{
    m_pendingResources.remove(resource);
    removeAsClientFromResource(resource);
    worked();
}

} // namespace WebCore

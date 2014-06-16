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

InspectorResourceContentLoader::InspectorResourceContentLoader(Page* page)
    : m_allRequestsStarted(false)
{
    Vector<Document*> documents;
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        LocalFrame* localFrame = toLocalFrame(frame);
        documents.append(localFrame->document());
        documents.appendVector(InspectorPageAgent::importsForFrame(localFrame));
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
            FetchRequest request(ResourceRequest(url), FetchInitiatorTypeNames::internal);
            ResourcePtr<Resource> resource = document->fetcher()->fetchCSSStyleSheet(request);
            // Prevent garbage collection by holding a reference to this resource.
            if (m_pendingResources.contains(resource.get()))
                continue;
            m_pendingResources.add(resource.get());
            m_resources.append(resource.get());
            resource->addClient(static_cast<StyleSheetResourceClient*>(this));
        }
    }

    m_allRequestsStarted = true;
    checkDone();
}

void InspectorResourceContentLoader::addListener(PassOwnPtr<VoidCallback> callback)
{
    m_callbacks.append(callback);
    checkDone();
}

InspectorResourceContentLoader::~InspectorResourceContentLoader()
{
    stop();
}

void InspectorResourceContentLoader::stop()
{
    HashSet<Resource*> pendingResources;
    m_pendingResources.swap(pendingResources);
    for (HashSet<Resource*>::const_iterator it = pendingResources.begin(); it != pendingResources.end(); ++it)
        removeAsClientFromResource(*it);
    // Make sure all callbacks are called to prevent infinite waiting time.
    checkDone();
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
    return m_allRequestsStarted && m_pendingResources.size() == 0;
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
    checkDone();
}

} // namespace WebCore

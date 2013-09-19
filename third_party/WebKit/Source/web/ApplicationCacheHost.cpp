/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/loader/appcache/ApplicationCacheHost.h"

#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"
#include "ApplicationCacheHostInternal.h"
#include "WebFrameImpl.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ProgressEvent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/appcache/ApplicationCache.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/chromium/support/WrappedResourceRequest.h"
#include "core/platform/chromium/support/WrappedResourceResponse.h"
#include "weborigin/SecurityOrigin.h"

using namespace WebKit;

namespace WebCore {

// We provide a custom implementation of this class that calls out to the
// embedding application instead of using WebCore's built in appcache system.
// This file replaces webcore/appcache/ApplicationCacheHost.cpp in our build.

ApplicationCacheHost::ApplicationCacheHost(DocumentLoader* documentLoader)
    : m_domApplicationCache(0)
    , m_documentLoader(documentLoader)
    , m_defersEvents(true)
{
    ASSERT(m_documentLoader);
}

ApplicationCacheHost::~ApplicationCacheHost()
{
}

void ApplicationCacheHost::willStartLoadingMainResource(ResourceRequest& request)
{
    // We defer creating the outer host object to avoid spurious creation/destruction
    // around creating empty documents. At this point, we're initiating a main resource
    // load for the document, so its for real.

    if (!isApplicationCacheEnabled())
        return;

    m_internal = adoptPtr(new ApplicationCacheHostInternal(this));
    if (m_internal->m_outerHost) {
        WrappedResourceRequest wrapped(request);
        m_internal->m_outerHost->willStartMainResourceRequest(wrapped, WebFrameImpl::fromFrame(m_documentLoader->frame()));
    } else
        m_internal.clear();

    // NOTE: The semantics of this method, and others in this interface, are subtly different
    // than the method names would suggest. For example, in this method never returns an appcached
    // response in the SubstituteData out argument, instead we return the appcached response thru
    // the usual resource loading pipeline.
}

void ApplicationCacheHost::selectCacheWithoutManifest()
{
    if (m_internal)
        m_internal->m_outerHost->selectCacheWithoutManifest();
}

void ApplicationCacheHost::selectCacheWithManifest(const KURL& manifestURL)
{
    if (m_internal) {
        if (!m_internal->m_outerHost->selectCacheWithManifest(manifestURL)) {
            // It's a foreign entry, restart the current navigation from the top
            // of the navigation algorithm. The navigation will not result in the
            // same resource being loaded, because "foreign" entries are never picked
            // during navigation.
            // see WebCore::ApplicationCacheGroup::selectCache()
            Frame* frame = m_documentLoader->frame();
            frame->navigationScheduler()->scheduleLocationChange(frame->document()->securityOrigin(),
                frame->document()->url(), frame->document()->referrer());
        }
    }
}

void ApplicationCacheHost::didReceiveResponseForMainResource(const ResourceResponse& response)
{
    if (m_internal) {
        WrappedResourceResponse wrapped(response);
        m_internal->m_outerHost->didReceiveResponseForMainResource(wrapped);
    }
}

void ApplicationCacheHost::mainResourceDataReceived(const char* data, int length)
{
    if (m_internal)
        m_internal->m_outerHost->didReceiveDataForMainResource(data, length);
}

void ApplicationCacheHost::failedLoadingMainResource()
{
    if (m_internal)
        m_internal->m_outerHost->didFinishLoadingMainResource(false);
}

void ApplicationCacheHost::finishedLoadingMainResource()
{
    if (m_internal)
        m_internal->m_outerHost->didFinishLoadingMainResource(true);
}

void ApplicationCacheHost::willStartLoadingResource(ResourceRequest& request)
{
    // FIXME: look into the purpose of the unused KURL& originalURL parameter
    if (m_internal) {
        WrappedResourceRequest wrapped(request);
        m_internal->m_outerHost->willStartSubResourceRequest(wrapped);
    }
}

void ApplicationCacheHost::setApplicationCache(ApplicationCache* domApplicationCache)
{
    ASSERT(!m_domApplicationCache || !domApplicationCache);
    m_domApplicationCache = domApplicationCache;
}

void ApplicationCacheHost::notifyApplicationCache(EventID id, int total, int done)
{
    if (id != PROGRESS_EVENT)
        InspectorInstrumentation::updateApplicationCacheStatus(m_documentLoader->frame());

    if (m_defersEvents) {
        // Event dispatching is deferred until document.onload has fired.
        m_deferredEvents.append(DeferredEvent(id, total, done));
        return;
    }
    dispatchDOMEvent(id, total, done);
}

ApplicationCacheHost::CacheInfo ApplicationCacheHost::applicationCacheInfo()
{
    if (!m_internal)
        return CacheInfo(KURL(), 0, 0, 0);

    WebKit::WebApplicationCacheHost::CacheInfo webInfo;
    m_internal->m_outerHost->getAssociatedCacheInfo(&webInfo);
    return CacheInfo(webInfo.manifestURL, webInfo.creationTime, webInfo.updateTime, webInfo.totalSize);
}

void ApplicationCacheHost::fillResourceList(ResourceInfoList* resources)
{
    if (!m_internal)
        return;

    WebKit::WebVector<WebKit::WebApplicationCacheHost::ResourceInfo> webResources;
    m_internal->m_outerHost->getResourceList(&webResources);
    for (size_t i = 0; i < webResources.size(); ++i) {
        resources->append(ResourceInfo(
            webResources[i].url, webResources[i].isMaster, webResources[i].isManifest, webResources[i].isFallback,
            webResources[i].isForeign, webResources[i].isExplicit, webResources[i].size));
    }
}

void ApplicationCacheHost::stopDeferringEvents()
{
    RefPtr<DocumentLoader> protect(documentLoader());
    for (unsigned i = 0; i < m_deferredEvents.size(); ++i) {
        const DeferredEvent& deferred = m_deferredEvents[i];
        dispatchDOMEvent(deferred.eventID, deferred.progressTotal, deferred.progressDone);
    }
    m_deferredEvents.clear();
    m_defersEvents = false;
}

void ApplicationCacheHost::dispatchDOMEvent(EventID id, int total, int done)
{
    if (m_domApplicationCache) {
        const AtomicString& eventType = ApplicationCache::toEventType(id);
        RefPtr<Event> event;
        if (id == PROGRESS_EVENT)
            event = ProgressEvent::create(eventType, true, done, total);
        else
            event = Event::create(eventType);
        m_domApplicationCache->dispatchEvent(event, ASSERT_NO_EXCEPTION);
    }
}

ApplicationCacheHost::Status ApplicationCacheHost::status() const
{
    return m_internal ? static_cast<Status>(m_internal->m_outerHost->status()) : UNCACHED;
}

bool ApplicationCacheHost::update()
{
    return m_internal ? m_internal->m_outerHost->startUpdate() : false;
}

bool ApplicationCacheHost::swapCache()
{
    bool success = m_internal ? m_internal->m_outerHost->swapCache() : false;
    if (success)
        InspectorInstrumentation::updateApplicationCacheStatus(m_documentLoader->frame());
    return success;
}

void ApplicationCacheHost::abort()
{
    if (m_internal)
        m_internal->m_outerHost->abort();
}

bool ApplicationCacheHost::isApplicationCacheEnabled()
{
    ASSERT(m_documentLoader->frame());
    return m_documentLoader->frame()->settings()
           && m_documentLoader->frame()->settings()->offlineWebApplicationCacheEnabled();
}

}  // namespace WebCore

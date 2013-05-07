/*
 * Copyright (C) 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/ResourceLoader.h"

#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/cache/CachedResourceLoader.h"
#include "core/loader/cache/MemoryCache.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/SecurityOrigin.h"
#include "core/platform/Logging.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceHandle.h"

namespace WebCore {

ResourceLoader::RequestCountTracker::RequestCountTracker(CachedResourceLoader* cachedResourceLoader, CachedResource* resource)
    : m_cachedResourceLoader(cachedResourceLoader)
    , m_resource(resource)
{
    m_cachedResourceLoader->incrementRequestCount(m_resource);
}

ResourceLoader::RequestCountTracker::~RequestCountTracker()
{
    m_cachedResourceLoader->decrementRequestCount(m_resource);
}

PassRefPtr<ResourceLoader> ResourceLoader::create(Frame* frame, CachedResource* resource, const ResourceRequest& request, const ResourceLoaderOptions& options)
{
    RefPtr<ResourceLoader> loader(adoptRef(new ResourceLoader(frame, resource, options)));
    if (!loader->init(request))
        return 0;
    loader->start();
    return loader.release();
}

ResourceLoader::ResourceLoader(Frame* frame, CachedResource* resource, ResourceLoaderOptions options)
    : m_frame(frame)
    , m_documentLoader(frame->loader()->activeDocumentLoader())
    , m_identifier(0)
    , m_loadingMultipartContent(false)
    , m_reachedTerminalState(false)
    , m_cancelled(false)
    , m_notifiedLoadComplete(false)
    , m_defersLoading(frame->page()->defersLoading())
    , m_options(options)
    , m_resource(resource)
    , m_state(Uninitialized)
    , m_requestCountTracker(adoptPtr(new RequestCountTracker(frame->document()->cachedResourceLoader(), resource)))
{
}

ResourceLoader::~ResourceLoader()
{
    ASSERT(m_state != Initialized);
    ASSERT(m_reachedTerminalState);
}

void ResourceLoader::releaseResources()
{
    ASSERT(!reachedTerminalState());
    if (m_state != Uninitialized) {
        m_requestCountTracker.clear();
        m_documentLoader->cachedResourceLoader()->loadDone(m_resource);
        if (reachedTerminalState())
            return;
        m_resource->stopLoading();
        m_documentLoader->removeResourceLoader(this);
    }

    ASSERT(!m_reachedTerminalState);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    RefPtr<ResourceLoader> protector(this);

    m_frame = 0;
    m_documentLoader = 0;
    
    // We need to set reachedTerminalState to true before we release
    // the resources to prevent a double dealloc of WebView <rdar://problem/4372628>
    m_reachedTerminalState = true;

    m_identifier = 0;

    if (m_handle) {
        // Clear out the ResourceHandle's client so that it doesn't try to call
        // us back after we release it, unless it has been replaced by someone else.
        if (m_handle->client() == this)
            m_handle->setClient(0);
        m_handle = 0;
    }

    m_deferredRequest = ResourceRequest();
}

bool ResourceLoader::init(const ResourceRequest& r)
{
    ASSERT(!m_handle);
    ASSERT(m_request.isNull());
    ASSERT(m_deferredRequest.isNull());
    ASSERT(!m_documentLoader->isSubstituteLoadPending(this));
    
    ResourceRequest clientRequest(r);
    
    m_defersLoading = m_frame->page()->defersLoading();
    if (m_options.securityCheck == DoSecurityCheck && !m_frame->document()->securityOrigin()->canDisplay(clientRequest.url())) {
        FrameLoader::reportLocalLoadFailed(m_frame.get(), clientRequest.url().string());
        releaseResources();
        return false;
    }

    m_identifier = createUniqueIdentifier();
    willSendRequest(0, clientRequest, ResourceResponse());
    if (clientRequest.isNull()) {
        cancel();
        return false;
    }
    ASSERT(!reachedTerminalState());

    m_originalRequest = m_request = clientRequest;
    m_state = Initialized;
    m_documentLoader->addResourceLoader(this);
    return true;
}

void ResourceLoader::start()
{
    ASSERT(!m_handle);
    ASSERT(!m_request.isNull());
    ASSERT(m_deferredRequest.isNull());

    if (m_documentLoader->scheduleArchiveLoad(this, m_request))
        return;

    m_documentLoader->applicationCacheHost()->willStartLoadingResource(m_request);

    if (m_defersLoading) {
        m_deferredRequest = m_request;
        return;
    }

    if (!m_reachedTerminalState)
        m_handle = ResourceHandle::create(m_frame->loader()->networkingContext(), m_request, this, m_defersLoading, m_options.sniffContent == SniffContent, m_options.allowCredentials);
}

void ResourceLoader::setDefersLoading(bool defers)
{
    m_defersLoading = defers;
    if (m_handle)
        m_handle->setDefersLoading(defers);
    if (!defers && !m_deferredRequest.isNull()) {
        m_request = m_deferredRequest;
        m_deferredRequest = ResourceRequest();
        start();
    }
}

FrameLoader* ResourceLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

void ResourceLoader::didDownloadData(ResourceHandle*, int length)
{
    if (!m_cancelled && !fastMallocSize(documentLoader()->applicationCacheHost()))
        CRASH();
    if (!m_cancelled && !fastMallocSize(documentLoader()->frame()))
        CRASH();
    RefPtr<ResourceLoader> protect(this);
    m_resource->didDownloadData(length);
}

void ResourceLoader::didFinishLoadingOnePart(double finishTime)
{
    // If load has been cancelled after finishing (which could happen with a
    // JavaScript that changes the window location), do nothing.
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    if (m_notifiedLoadComplete)
        return;
    m_notifiedLoadComplete = true;
    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->didFinishLoad(this, finishTime);
}

void ResourceLoader::didChangePriority(ResourceLoadPriority loadPriority)
{
    if (handle()) {
        frameLoader()->client()->dispatchDidChangeResourcePriority(identifier(), loadPriority);
        handle()->didChangePriority(loadPriority);
    }
}

void ResourceLoader::cancelIfNotFinishing()
{
    if (m_state != Initialized)
        return;
    cancel();
}

void ResourceLoader::cancel()
{
    cancel(ResourceError());
}

void ResourceLoader::cancel(const ResourceError& error)
{
    // If the load has already completed - succeeded, failed, or previously cancelled - do nothing.
    if (m_reachedTerminalState)
        return;
       
    ResourceError nonNullError = error.isNull() ? cancelledError() : error;

    // This function calls out to clients at several points that might do
    // something that causes the last reference to this object to go away.
    RefPtr<ResourceLoader> protector(this);
    
    // If we re-enter cancel() from inside this if() block, we want to pick up from where we left
    // off without re-running it
    if (m_state == Initialized) {
        LOG(ResourceLoading, "Cancelled load of '%s'.\n", m_resource->url().string().latin1().data());
        m_state = Finishing;
        if (m_resource->resourceToRevalidate())
            memoryCache()->revalidationFailed(m_resource);
        m_resource->setResourceError(nonNullError);
        memoryCache()->remove(m_resource);
    }

    // If we re-enter cancel() from inside didFailToLoad(), we want to pick up from where we 
    // left off without redoing any of this work.
    if (!m_cancelled) {
        m_cancelled = true;

        m_documentLoader->cancelPendingSubstituteLoad(this);
        if (m_handle) {
            m_handle->cancel();
            m_handle = 0;
        }

        if (m_options.sendLoadCallbacks == SendCallbacks && m_identifier && !m_notifiedLoadComplete)
            frameLoader()->notifier()->didFailToLoad(this, nonNullError);
    }

    // If cancel() completed from within the call to willCancel() or didFailToLoad(),
    // we don't want to redo didCancel() or releasesResources().
    if (m_reachedTerminalState)
        return;
    releaseResources();
}

ResourceError ResourceLoader::cancelledError()
{
    return frameLoader()->cancelledError(m_request);
}

ResourceError ResourceLoader::cannotShowURLError()
{
    return frameLoader()->client()->cannotShowURLError(m_request);
}

void ResourceLoader::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    // Store the previous URL because we may modify it.
    KURL previousURL = m_request.url();
    RefPtr<ResourceLoader> protect(this);

    ASSERT(!request.isNull());
    if (!redirectResponse.isNull()) {
        if (!m_documentLoader->cachedResourceLoader()->canRequest(m_resource->type(), request.url())) {
            cancel();
            return;
        }
        if (m_resource->type() == CachedResource::ImageResource && m_documentLoader->cachedResourceLoader()->shouldDeferImageLoad(request.url())) {
            cancel();
            return;
        }
        m_resource->willSendRequest(request, redirectResponse);
    }

    if (request.isNull() || reachedTerminalState())
        return;

    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->willSendRequest(this, request, redirectResponse);
    else
        InspectorInstrumentation::willSendRequest(m_frame.get(), m_identifier, m_frame->loader()->documentLoader(), request, redirectResponse);

    m_request = request;

    if (!redirectResponse.isNull() && !m_documentLoader->isCommitted())
        frameLoader()->client()->dispatchDidReceiveServerRedirectForProvisionalLoad();

    if (request.isNull())
        cancel();
}

void ResourceLoader::didReceiveCachedMetadata(ResourceHandle*, const char* data, int length)
{
    ASSERT(m_state == Initialized);
    ASSERT(!m_resource->resourceToRevalidate());
    m_resource->setSerializedCachedMetadata(data, length);
}

void ResourceLoader::didSendData(ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_state == Initialized);
    RefPtr<ResourceLoader> protect(this);
    m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

void ResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    ASSERT(!response.isNull());
    ASSERT(m_state == Initialized);

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object.
    RefPtr<ResourceLoader> protect(this);

    if (m_resource->resourceToRevalidate()) {
        if (response.httpStatusCode() == 304) {
            // 304 Not modified / Use local copy
            // Existing resource is ok, just use it updating the expiration time.
            m_resource->setResponse(response);
            memoryCache()->revalidationSucceeded(m_resource, response);
            if (reachedTerminalState())
                return;

            if (m_options.sendLoadCallbacks == SendCallbacks)
                frameLoader()->notifier()->didReceiveResponse(this, response);
            return;
        }
        // Did not get 304 response, continue as a regular resource load.
        memoryCache()->revalidationFailed(m_resource);
    }

    m_resource->responseReceived(response);
    if (reachedTerminalState())
        return;

    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->didReceiveResponse(this, response);

    // FIXME: Main resources have a different set of rules for multipart than images do.
    // Hopefully we can merge those 2 paths.
    if (response.isMultipart() && m_resource->type() != CachedResource::MainResource) {
        m_loadingMultipartContent = true;

        // We don't count multiParts in a CachedResourceLoader's request count
        m_requestCountTracker.clear();
        if (!m_resource->isImage()) {
            cancel();
            return;
        }
    } else if (m_loadingMultipartContent) {
        // Since a subresource loader does not load multipart sections progressively, data was delivered to the loader all at once.
        // After the first multipart section is complete, signal to delegates that this load is "finished"
        m_documentLoader->subresourceLoaderFinishedLoadingOnePart(this);
        didFinishLoadingOnePart(0);
    }

    if (m_resource->response().httpStatusCode() < 400 || m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return;
    m_state = Finishing;
    m_resource->error(CachedResource::LoadError);
    cancel();
}

void ResourceLoader::didReceiveData(ResourceHandle*, const char* data, int length, int encodedDataLength)
{
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceData(m_frame.get(), identifier(), encodedDataLength);

    if (m_resource->response().httpStatusCode() >= 400 && !m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return;
    ASSERT(!m_resource->resourceToRevalidate());
    ASSERT(!m_resource->errorOccurred());
    ASSERT(m_state == Initialized);

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object.
    RefPtr<ResourceLoader> protect(this);

    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    if (m_options.sendLoadCallbacks == SendCallbacks && m_frame)
        frameLoader()->notifier()->didReceiveData(this, data, length, static_cast<int>(encodedDataLength));

    m_resource->appendData(data, length);

    InspectorInstrumentation::didReceiveResourceData(cookie);
}

void ResourceLoader::didFinishLoading(ResourceHandle*, double finishTime)
{
    if (m_state != Initialized)
        return;
    ASSERT(!reachedTerminalState());
    ASSERT(!m_resource->resourceToRevalidate());
    ASSERT(!m_resource->errorOccurred());
    LOG(ResourceLoading, "Received '%s'.", m_resource->url().string().latin1().data());

    RefPtr<ResourceLoader> protect(this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    m_resource->setLoadFinishTime(finishTime);
    m_resource->finish();
    didFinishLoadingOnePart(finishTime);

    // If the load has been cancelled by a delegate in response to didFinishLoad(), do not release
    // the resources a second time, they have been released by cancel.
    if (m_cancelled)
        return;
    releaseResources();
}

void ResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    ASSERT(!reachedTerminalState());
    LOG(ResourceLoading, "Failed to load '%s'.\n", m_resource->url().string().latin1().data());

    RefPtr<ResourceLoader> protect(this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    if (m_resource->resourceToRevalidate())
        memoryCache()->revalidationFailed(m_resource);
    m_resource->setResourceError(error);
    m_resource->error(CachedResource::LoadError);
    if (!m_resource->isPreloaded())
        memoryCache()->remove(m_resource);

    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    if (!m_notifiedLoadComplete) {
        m_notifiedLoadComplete = true;
        if (m_options.sendLoadCallbacks == SendCallbacks)
            frameLoader()->notifier()->didFailToLoad(this, error);
    }

    releaseResources();
}

void ResourceLoader::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Loader);
    info.addMember(m_handle, "handle");
    info.addMember(m_frame, "frame");
    info.addMember(m_documentLoader, "documentLoader");
    info.addMember(m_request, "request");
    info.addMember(m_originalRequest, "originalRequest");
    info.addMember(m_deferredRequest, "deferredRequest");
    info.addMember(m_options, "options");
    info.addMember(m_resource, "resource");
    info.addMember(m_documentLoader, "documentLoader");
    info.addMember(m_requestCountTracker, "requestCountTracker");
}

}

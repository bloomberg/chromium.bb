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
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/cache/CachedResourceLoader.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
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

PassRefPtr<ResourceLoader> ResourceLoader::create(DocumentLoader* documentLoader, CachedResource* resource, const ResourceRequest& request, const ResourceLoaderOptions& options)
{
    RefPtr<ResourceLoader> loader(adoptRef(new ResourceLoader(documentLoader, resource, options)));
    if (!loader->init(request))
        return 0;
    loader->start();
    return loader.release();
}

ResourceLoader::ResourceLoader(DocumentLoader* documentLoader, CachedResource* resource, ResourceLoaderOptions options)
    : m_frame(documentLoader->frame())
    , m_documentLoader(documentLoader)
    , m_loadingMultipartContent(false)
    , m_notifiedLoadComplete(false)
    , m_defersLoading(m_frame->page()->defersLoading())
    , m_options(options)
    , m_resource(resource)
    , m_state(Uninitialized)
    , m_requestCountTracker(adoptPtr(new RequestCountTracker(documentLoader->cachedResourceLoader(), resource)))
{
}

ResourceLoader::~ResourceLoader()
{
    ASSERT(m_state == Terminated);
}

void ResourceLoader::releaseResources()
{
    ASSERT(m_state != Terminated);
    if (m_state != Uninitialized) {
        m_requestCountTracker.clear();
        m_documentLoader->cachedResourceLoader()->loadDone(m_resource);
        if (m_state == Terminated)
            return;
        m_resource->clearLoader();
        m_documentLoader->removeResourceLoader(this);
    }

    ASSERT(m_state != Terminated);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    RefPtr<ResourceLoader> protector(this);

    m_frame = 0;
    m_documentLoader = 0;

    m_state = Terminated;

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
    
    ResourceRequest clientRequest(r);

    willSendRequest(0, clientRequest, ResourceResponse());
    if (clientRequest.isNull()) {
        cancel();
        return false;
    }
    ASSERT(m_state != Terminated);

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

    m_documentLoader->applicationCacheHost()->willStartLoadingResource(m_request);

    if (m_defersLoading) {
        m_deferredRequest = m_request;
        return;
    }

    if (m_state != Terminated)
        m_handle = ResourceHandle::create(m_request, this, m_defersLoading, m_options.sniffContent == SniffContent, m_options.allowCredentials);
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
    RefPtr<ResourceLoader> protect(this);
    m_resource->didDownloadData(length);
}

void ResourceLoader::didFinishLoadingOnePart(double finishTime)
{
    // If load has been cancelled after finishing (which could happen with a
    // JavaScript that changes the window location), do nothing.
    if (m_state == Terminated)
        return;

    if (m_notifiedLoadComplete)
        return;
    m_notifiedLoadComplete = true;
    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->dispatchDidFinishLoading(m_documentLoader.get(), m_resource->identifier(), finishTime);
}

void ResourceLoader::didChangePriority(ResourceLoadPriority loadPriority)
{
    if (handle()) {
        frameLoader()->client()->dispatchDidChangeResourcePriority(m_resource->identifier(), loadPriority);
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
    if (m_state == Terminated)
        return;
    if (m_state == Finishing) {
        releaseResources();
        return;
    }

    ResourceError nonNullError = error.isNull() ? cancelledError() : error;

    // This function calls out to clients at several points that might do
    // something that causes the last reference to this object to go away.
    RefPtr<ResourceLoader> protector(this);

    LOG(ResourceLoading, "Cancelled load of '%s'.\n", m_resource->url().string().latin1().data());
    if (m_state == Initialized)
        m_state = Finishing;
    m_resource->setResourceError(nonNullError);

    if (m_handle) {
        m_handle->cancel();
        m_handle = 0;
    }

    if (m_options.sendLoadCallbacks == SendCallbacks && !m_notifiedLoadComplete)
        frameLoader()->notifier()->dispatchDidFail(m_documentLoader.get(), m_resource->identifier(), nonNullError);

    if (m_state == Finishing)
        m_resource->error(CachedResource::LoadError);
    if (m_state != Terminated)
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
        if (!m_documentLoader->cachedResourceLoader()->canRequest(m_resource->type(), request.url(), m_options)) {
            cancel();
            return;
        }
        if (m_resource->type() == CachedResource::ImageResource && m_documentLoader->cachedResourceLoader()->shouldDeferImageLoad(request.url())) {
            cancel();
            return;
        }
        m_resource->willSendRequest(request, redirectResponse);
    }

    if (request.isNull() || m_state == Terminated)
        return;

    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->dispatchWillSendRequest(m_documentLoader.get(), m_resource->identifier(), request, redirectResponse, m_options.initiatorInfo);
    else
        InspectorInstrumentation::willSendRequest(m_frame.get(), m_resource->identifier(), m_documentLoader.get(), request, redirectResponse, m_options.initiatorInfo);

    m_request = request;

    if (request.isNull())
        cancel();
}

void ResourceLoader::didReceiveCachedMetadata(ResourceHandle*, const char* data, int length)
{
    ASSERT(m_state == Initialized);
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
    m_resource->responseReceived(response);
    if (m_state == Terminated)
        return;

    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->dispatchDidReceiveResponse(m_documentLoader.get(), m_resource->identifier(), response);

    if (response.isMultipart()) {
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
    // It is possible to receive data on uninitialized resources if it had an error status code, and we are running a nested message
    // loop. When this occurs, ignoring the data is the correct action.
    if (m_resource->response().httpStatusCode() >= 400 && !m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return;
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceData(m_frame.get(), m_resource->identifier(), encodedDataLength);
    ASSERT(m_state == Initialized);

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object.
    RefPtr<ResourceLoader> protect(this);

    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    if (m_options.sendLoadCallbacks == SendCallbacks && m_frame)
        frameLoader()->notifier()->dispatchDidReceiveData(m_documentLoader.get(), m_resource->identifier(), data, length, static_cast<int>(encodedDataLength));

    m_resource->appendData(data, length);

    InspectorInstrumentation::didReceiveResourceData(cookie);
}

void ResourceLoader::didFinishLoading(ResourceHandle*, double finishTime)
{
    if (m_state != Initialized)
        return;
    ASSERT(m_state != Terminated);
    LOG(ResourceLoading, "Received '%s'.", m_resource->url().string().latin1().data());

    RefPtr<ResourceLoader> protect(this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    m_resource->finish(finishTime);
    didFinishLoadingOnePart(finishTime);

    // If the load has been cancelled by a delegate in response to didFinishLoad(), do not release
    // the resources a second time, they have been released by cancel.
    if (m_state == Terminated)
        return;
    releaseResources();
}

void ResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    ASSERT(m_state != Terminated);
    LOG(ResourceLoading, "Failed to load '%s'.\n", m_resource->url().string().latin1().data());

    RefPtr<ResourceLoader> protect(this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    m_resource->setResourceError(error);
    m_resource->error(CachedResource::LoadError);

    if (m_state == Terminated)
        return;

    if (!m_notifiedLoadComplete) {
        m_notifiedLoadComplete = true;
        if (m_options.sendLoadCallbacks == SendCallbacks)
            frameLoader()->notifier()->dispatchDidFail(m_documentLoader.get(), m_resource->identifier(), error);
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

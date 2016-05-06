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

#include "core/fetch/ResourceLoader.h"

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/Logging.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/ResourceError.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"

namespace blink {

ResourceLoader* ResourceLoader::create(ResourceFetcher* fetcher, Resource* resource)
{
    return new ResourceLoader(fetcher, resource);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher, Resource* resource)
    : m_fetcher(fetcher)
    , m_notifiedLoadComplete(false)
    , m_resource(resource)
    , m_state(ConnectionStateNew)
{
    ASSERT(m_resource);
    ASSERT(m_fetcher);
}

ResourceLoader::~ResourceLoader()
{
    ASSERT(m_state == ConnectionStateReleased);
}

DEFINE_TRACE(ResourceLoader)
{
    visitor->trace(m_fetcher);
    visitor->trace(m_resource);
}

void ResourceLoader::releaseResources()
{
    ASSERT(m_state != ConnectionStateReleased);
    ASSERT(m_notifiedLoadComplete);
    m_fetcher->didLoadResource(m_resource.get());
    if (m_state == ConnectionStateReleased)
        return;
    m_resource->clearLoader();
    m_resource = nullptr;

    ASSERT(m_state != ConnectionStateReleased);
    m_state = ConnectionStateReleased;
    if (m_loader) {
        m_loader->cancel();
        m_loader.clear();
    }
    m_fetcher.clear();
}

void ResourceLoader::start(ResourceRequest& request)
{
    ASSERT(!m_loader);

    m_fetcher->willStartLoadingResource(m_resource.get(), this, request);
    RELEASE_ASSERT(m_state == ConnectionStateNew);
    m_state = ConnectionStateStarted;

    m_loader = adoptPtr(Platform::current()->createURLLoader());
    m_loader->setDefersLoading(m_fetcher->defersLoading());
    ASSERT(m_loader);
    m_loader->setLoadingTaskRunner(m_fetcher->loadingTaskRunner());

    if (m_resource->options().synchronousPolicy == RequestSynchronously)
        requestSynchronously(request);
    else
        m_loader->loadAsynchronously(WrappedResourceRequest(request), this);
}

void ResourceLoader::setDefersLoading(bool defers)
{
    ASSERT(m_loader);
    m_loader->setDefersLoading(defers);
}

void ResourceLoader::didDownloadData(WebURLLoader*, int length, int encodedDataLength)
{
    RELEASE_ASSERT(m_state == ConnectionStateReceivedResponse);
    m_fetcher->didDownloadData(m_resource.get(), length, encodedDataLength);
    if (m_state == ConnectionStateReleased)
        return;
    m_resource->didDownloadData(length);
}

void ResourceLoader::didFinishLoadingOnePart(double finishTime, int64_t encodedDataLength)
{
    ASSERT(m_state != ConnectionStateReleased);
    if (isFinishing()) {
        m_fetcher->removeResourceLoader(this);
    } else {
        // When loading a multipart resource, make the loader non-block when
        // finishing loading the first part.
        m_fetcher->moveResourceLoaderToNonBlocking(this);

        m_fetcher->didLoadResource(m_resource.get());
        if (m_state == ConnectionStateReleased)
            return;
    }

    if (m_notifiedLoadComplete)
        return;
    m_notifiedLoadComplete = true;
    m_fetcher->didFinishLoading(m_resource.get(), finishTime, encodedDataLength);
}

void ResourceLoader::didChangePriority(ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    ASSERT(m_state != ConnectionStateReleased);
    if (m_loader)
        m_loader->didChangePriority(static_cast<WebURLRequest::Priority>(loadPriority), intraPriorityValue);
}

void ResourceLoader::cancelIfNotFinishing()
{
    if (isFinishing())
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
    if (m_state == ConnectionStateReleased)
        return;
    if (isFinishing()) {
        releaseResources();
        return;
    }

    ResourceError nonNullError = error.isNull() ? ResourceError::cancelledError(m_resource->lastResourceRequest().url()) : error;

    WTF_LOG(ResourceLoading, "Cancelled load of '%s'.\n", m_resource->url().getString().latin1().data());
    m_state = ConnectionStateCanceled;

    // If we don't immediately clear m_loader when cancelling, we might get
    // unexpected reentrancy. m_resource->error() can trigger JS events, which
    // could start a modal dialog. Normally, a modal dialog would defer loading
    // and prevent receiving messages for a cancelled ResourceLoader, but
    // m_fetcher->didFailLoading() severs the connection by which all of a
    // page's loads are deferred. A response can then arrive, see m_state
    // is ConnectionStateCanceled, and ASSERT or break in other ways.
    if (m_loader) {
        m_loader->cancel();
        m_loader.clear();
    }

    if (!m_notifiedLoadComplete) {
        m_notifiedLoadComplete = true;
        m_fetcher->didFailLoading(m_resource.get(), nonNullError);
    }

    if (m_state != ConnectionStateReleased)
        m_resource->error(nonNullError);
    if (m_state != ConnectionStateReleased)
        releaseResources();
}

void ResourceLoader::willFollowRedirect(WebURLLoader*, WebURLRequest& passedNewRequest, const WebURLResponse& passedRedirectResponse)
{
    ASSERT(m_state != ConnectionStateReleased);
    ASSERT(!passedNewRequest.isNull());
    ASSERT(!passedRedirectResponse.isNull());

    ResourceRequest& newRequest(passedNewRequest.toMutableResourceRequest());
    const ResourceResponse& redirectResponse(passedRedirectResponse.toResourceResponse());
    newRequest.setFollowedRedirect(true);

    if (m_fetcher->willFollowRedirect(m_resource.get(), newRequest, redirectResponse)) {
        m_resource->willFollowRedirect(newRequest, redirectResponse);
    } else {
        m_resource->willNotFollowRedirect();
        cancel(ResourceError::cancelledDueToAccessCheckError(newRequest.url()));
    }
}

void ResourceLoader::didReceiveCachedMetadata(WebURLLoader*, const char* data, int length)
{
    RELEASE_ASSERT(m_state == ConnectionStateReceivedResponse || m_state == ConnectionStateReceivingData);
    m_resource->setSerializedCachedMetadata(data, length);
}

void ResourceLoader::didSendData(WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

bool ResourceLoader::responseNeedsAccessControlCheck() const
{
    // If the fetch was (potentially) CORS enabled, an access control check of the response is required.
    return m_resource->options().corsEnabled == IsCORSEnabled;
}

void ResourceLoader::didReceiveResponse(WebURLLoader*, const WebURLResponse& response, WebDataConsumerHandle* rawHandle)
{
    ASSERT(!response.isNull());
    // |rawHandle|'s ownership is transferred to the callee.
    OwnPtr<WebDataConsumerHandle> handle = adoptPtr(rawHandle);

    bool isValidStateTransition = (m_state == ConnectionStateStarted || m_state == ConnectionStateReceivedResponse);
    RELEASE_ASSERT(isValidStateTransition);
    m_state = ConnectionStateReceivedResponse;

    const ResourceResponse& resourceResponse = response.toResourceResponse();

    if (responseNeedsAccessControlCheck()) {
        if (response.wasFetchedViaServiceWorker()) {
            if (response.wasFallbackRequiredByServiceWorker()) {
                m_loader->cancel();
                m_loader.clear();
                m_state = ConnectionStateStarted;
                m_loader = adoptPtr(Platform::current()->createURLLoader());
                ASSERT(m_loader);
                ResourceRequest request = m_resource->lastResourceRequest();
                ASSERT(!request.skipServiceWorker());
                request.setSkipServiceWorker(true);
                m_loader->loadAsynchronously(WrappedResourceRequest(request), this);
                return;
            }
        } else {
            if (!m_resource->isCacheValidator() || resourceResponse.httpStatusCode() != 304)
                m_resource->setResponse(resourceResponse);
            if (!m_fetcher->canAccessResource(m_resource.get(), m_resource->options().securityOrigin.get(), response.url(), ResourceFetcher::ShouldLogAccessControlErrors)) {
                m_fetcher->didReceiveResponse(m_resource.get(), resourceResponse);
                cancel(ResourceError::cancelledDueToAccessCheckError(KURL(response.url())));
                return;
            }
        }
    }

    m_resource->responseReceived(resourceResponse, handle.release());
    if (m_state == ConnectionStateReleased)
        return;

    m_fetcher->didReceiveResponse(m_resource.get(), resourceResponse);
    if (m_state == ConnectionStateReleased)
        return;

    if (m_resource->response().httpStatusCode() < 400 || m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return;
    cancel(ResourceError::cancelledError(resourceResponse.url()));
}

void ResourceLoader::didReceiveResponse(WebURLLoader* loader, const WebURLResponse& response)
{
    didReceiveResponse(loader, response, nullptr);
}

void ResourceLoader::didReceiveData(WebURLLoader*, const char* data, int length, int encodedDataLength)
{
    RELEASE_ASSERT(m_state == ConnectionStateReceivedResponse || m_state == ConnectionStateReceivingData);
    m_state = ConnectionStateReceivingData;

    // It is possible to receive data on uninitialized resources if it had an error status code, and we are running a nested message
    // loop. When this occurs, ignoring the data is the correct action.
    if (m_resource->response().httpStatusCode() >= 400 && !m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return;

    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    m_fetcher->didReceiveData(m_resource.get(), data, length, encodedDataLength);
    if (m_state == ConnectionStateReleased)
        return;
    RELEASE_ASSERT(length >= 0);
    m_resource->appendData(data, length);
}

void ResourceLoader::didFinishLoading(WebURLLoader*, double finishTime, int64_t encodedDataLength)
{

    RELEASE_ASSERT(m_state == ConnectionStateReceivedResponse || m_state == ConnectionStateReceivingData);
    m_state = ConnectionStateFinishedLoading;
    WTF_LOG(ResourceLoading, "Received '%s'.", m_resource->url().getString().latin1().data());
    didFinishLoadingOnePart(finishTime, encodedDataLength);
    if (m_state == ConnectionStateReleased)
        return;
    m_resource->finish(finishTime);

    // If the load has been cancelled by a delegate in response to didFinishLoad(), do not release
    // the resources a second time, they have been released by cancel.
    if (m_state == ConnectionStateReleased)
        return;
    releaseResources();
}

void ResourceLoader::didFail(WebURLLoader*, const WebURLError& error)
{

    ASSERT(m_state != ConnectionStateReleased);
    m_state = ConnectionStateFailed;
    WTF_LOG(ResourceLoading, "Failed to load '%s'.\n", m_resource->url().getString().latin1().data());
    if (!m_notifiedLoadComplete) {
        m_notifiedLoadComplete = true;
        m_fetcher->didFailLoading(m_resource.get(), error);
    }
    if (m_state == ConnectionStateReleased)
        return;

    m_resource->error(error);

    if (m_state == ConnectionStateReleased)
        return;

    releaseResources();
}

void ResourceLoader::requestSynchronously(ResourceRequest& request)
{
    // downloadToFile is not supported for synchronous requests.
    ASSERT(!request.downloadToFile());
    ASSERT(m_loader);

    // Synchronous requests should always be max priority, lest they hang the renderer.
    request.setPriority(ResourceLoadPriorityHighest);

    if (m_fetcher->defersLoading()) {
        cancel();
        return;
    }

    WrappedResourceRequest requestIn(request);
    WebURLResponse responseOut;
    responseOut.initialize();
    WebURLError errorOut;
    WebData dataOut;
    m_loader->loadSynchronously(requestIn, responseOut, errorOut, dataOut);
    if (errorOut.reason) {
        if (m_state == ConnectionStateReleased) {
            // A message dispatched while synchronously fetching the resource
            // can bring about the cancellation of this load.
            ASSERT(!m_resource);
            return;
        }
        didFail(0, errorOut);
        return;
    }
    didReceiveResponse(0, responseOut);
    if (m_state == ConnectionStateReleased)
        return;
    RefPtr<ResourceLoadInfo> resourceLoadInfo = responseOut.toResourceResponse().resourceLoadInfo();
    int64_t encodedDataLength = resourceLoadInfo ? resourceLoadInfo->encodedDataLength : WebURLLoaderClient::kUnknownEncodedDataLength;

    // Follow the async case convention of not calling didReceiveData or
    // appending data to m_resource if the response body is empty. Copying the
    // empty buffer is a noop in most cases, but is destructive in the case of
    // a 304, where it will overwrite the cached data we should be reusing.
    if (dataOut.size()) {
        m_fetcher->didReceiveData(m_resource.get(), dataOut.data(), dataOut.size(), encodedDataLength);
        m_resource->setResourceBuffer(dataOut);
    }
    didFinishLoading(0, monotonicallyIncreasingTime(), encodedDataLength);
}

} // namespace blink

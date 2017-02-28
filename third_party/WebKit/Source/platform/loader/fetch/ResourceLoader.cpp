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

#include "platform/loader/fetch/ResourceLoader.h"

#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/NetworkInstrumentation.h"
#include "platform/network/ResourceError.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

ResourceLoader* ResourceLoader::create(ResourceFetcher* fetcher,
                                       Resource* resource) {
  return new ResourceLoader(fetcher, resource);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher, Resource* resource)
    : m_fetcher(fetcher),
      m_resource(resource),
      m_isCacheAwareLoadingActivated(false) {
  DCHECK(m_resource);
  DCHECK(m_fetcher);

  m_resource->setLoader(this);
}

ResourceLoader::~ResourceLoader() {}

DEFINE_TRACE(ResourceLoader) {
  visitor->trace(m_fetcher);
  visitor->trace(m_resource);
}

void ResourceLoader::start(const ResourceRequest& request) {
  DCHECK(!m_loader);

  if (m_resource->options().synchronousPolicy == RequestSynchronously &&
      context().defersLoading()) {
    cancel();
    return;
  }

  m_loader = WTF::wrapUnique(Platform::current()->createURLLoader());
  DCHECK(m_loader);
  m_loader->setDefersLoading(context().defersLoading());
  m_loader->setLoadingTaskRunner(context().loadingTaskRunner().get());

  if (m_isCacheAwareLoadingActivated) {
    // Override cache policy for cache-aware loading. If this request fails, a
    // reload with original request will be triggered in didFail().
    ResourceRequest cacheAwareRequest(request);
    cacheAwareRequest.setCachePolicy(WebCachePolicy::ReturnCacheDataIfValid);
    m_loader->loadAsynchronously(WrappedResourceRequest(cacheAwareRequest),
                                 this);
    return;
  }

  if (m_resource->options().synchronousPolicy == RequestSynchronously)
    requestSynchronously(request);
  else
    m_loader->loadAsynchronously(WrappedResourceRequest(request), this);
}

void ResourceLoader::restart(const ResourceRequest& request) {
  CHECK_EQ(m_resource->options().synchronousPolicy, RequestAsynchronously);

  m_loader.reset();
  start(request);
}

void ResourceLoader::setDefersLoading(bool defers) {
  DCHECK(m_loader);

  m_loader->setDefersLoading(defers);
}

void ResourceLoader::didChangePriority(ResourceLoadPriority loadPriority,
                                       int intraPriorityValue) {
  if (m_loader) {
    m_loader->didChangePriority(
        static_cast<WebURLRequest::Priority>(loadPriority), intraPriorityValue);
  }
}

void ResourceLoader::cancel() {
  handleError(
      ResourceError::cancelledError(m_resource->lastResourceRequest().url()));
}

void ResourceLoader::cancelForRedirectAccessCheckError(
    const KURL& newURL,
    ResourceRequestBlockedReason blockedReason) {
  m_resource->willNotFollowRedirect();

  if (m_loader) {
    handleError(
        ResourceError::cancelledDueToAccessCheckError(newURL, blockedReason));
  }
}

static bool isManualRedirectFetchRequest(const ResourceRequest& request) {
  return request.fetchRedirectMode() ==
             WebURLRequest::FetchRedirectModeManual &&
         request.requestContext() == WebURLRequest::RequestContextFetch;
}

bool ResourceLoader::willFollowRedirect(
    WebURLRequest& passedNewRequest,
    const WebURLResponse& passedRedirectResponse) {
  DCHECK(!passedNewRequest.isNull());
  DCHECK(!passedRedirectResponse.isNull());

  if (m_isCacheAwareLoadingActivated) {
    // Fail as cache miss if cached response is a redirect.
    handleError(
        ResourceError::cacheMissError(m_resource->lastResourceRequest().url()));
    return false;
  }

  ResourceRequest& newRequest(passedNewRequest.toMutableResourceRequest());
  const ResourceResponse& redirectResponse(
      passedRedirectResponse.toResourceResponse());

  newRequest.setRedirectStatus(
      ResourceRequest::RedirectStatus::FollowedRedirect);

  const KURL originalURL = newRequest.url();

  if (!isManualRedirectFetchRequest(m_resource->resourceRequest())) {
    ResourceRequestBlockedReason blockedReason = context().canRequest(
        m_resource->getType(), newRequest, newRequest.url(),
        m_resource->options(),
        /* Don't send security violation reports for unused preloads */
        (m_resource->isUnusedPreload()
             ? SecurityViolationReportingPolicy::SuppressReporting
             : SecurityViolationReportingPolicy::Report),
        FetchRequest::UseDefaultOriginRestrictionForType);
    if (blockedReason != ResourceRequestBlockedReason::None) {
      cancelForRedirectAccessCheckError(newRequest.url(), blockedReason);
      return false;
    }

    if (m_resource->options().corsEnabled == IsCORSEnabled) {
      RefPtr<SecurityOrigin> sourceOrigin =
          m_resource->options().securityOrigin;
      if (!sourceOrigin.get())
        sourceOrigin = context().getSecurityOrigin();

      String errorMessage;
      StoredCredentials withCredentials =
          m_resource->lastResourceRequest().allowStoredCredentials()
              ? AllowStoredCredentials
              : DoNotAllowStoredCredentials;
      if (!CrossOriginAccessControl::handleRedirect(
              sourceOrigin, newRequest, redirectResponse, withCredentials,
              m_resource->mutableOptions(), errorMessage)) {
        m_resource->setCORSFailed();
        context().addConsoleMessage(errorMessage);
        cancelForRedirectAccessCheckError(newRequest.url(),
                                          ResourceRequestBlockedReason::Other);
        return false;
      }
    }
    if (m_resource->getType() == Resource::Image &&
        m_fetcher->shouldDeferImageLoad(newRequest.url())) {
      cancelForRedirectAccessCheckError(newRequest.url(),
                                        ResourceRequestBlockedReason::Other);
      return false;
    }
  }

  bool crossOrigin = !SecurityOrigin::areSameSchemeHostPort(
      redirectResponse.url(), newRequest.url());
  m_fetcher->recordResourceTimingOnRedirect(m_resource.get(), redirectResponse,
                                            crossOrigin);

  newRequest.setAllowStoredCredentials(m_resource->options().allowCredentials ==
                                       AllowStoredCredentials);

  context().dispatchWillSendRequest(m_resource->identifier(), newRequest,
                                    redirectResponse,
                                    m_resource->options().initiatorInfo);

  // ResourceFetcher::willFollowRedirect() may rewrite the URL to
  // something else not for rejecting redirect but for other reasons.
  // E.g. WebFrameTestClient::willSendRequest() and
  // RenderFrameImpl::willSendRequest(). We should reflect the
  // rewriting but currently we cannot. So, return false to make the
  // redirect fail.
  if (newRequest.url() != originalURL) {
    cancelForRedirectAccessCheckError(newRequest.url(),
                                      ResourceRequestBlockedReason::Other);
    return false;
  }

  if (!m_resource->willFollowRedirect(newRequest, redirectResponse)) {
    cancelForRedirectAccessCheckError(newRequest.url(),
                                      ResourceRequestBlockedReason::Other);
    return false;
  }

  return true;
}

void ResourceLoader::didReceiveCachedMetadata(const char* data, int length) {
  m_resource->setSerializedCachedMetadata(data, length);
}

void ResourceLoader::didSendData(unsigned long long bytesSent,
                                 unsigned long long totalBytesToBeSent) {
  m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

FetchContext& ResourceLoader::context() const {
  return m_fetcher->context();
}

ResourceRequestBlockedReason ResourceLoader::canAccessResponse(
    Resource* resource,
    const ResourceResponse& response) const {
  // Redirects can change the response URL different from one of request.
  bool unusedPreload = resource->isUnusedPreload();
  ResourceRequestBlockedReason blockedReason = context().canRequest(
      resource->getType(), resource->resourceRequest(), response.url(),
      resource->options(),
      /* Don't send security violation reports for unused preloads */
      (unusedPreload ? SecurityViolationReportingPolicy::SuppressReporting
                     : SecurityViolationReportingPolicy::Report),
      FetchRequest::UseDefaultOriginRestrictionForType);
  if (blockedReason != ResourceRequestBlockedReason::None)
    return blockedReason;

  SecurityOrigin* sourceOrigin = resource->options().securityOrigin.get();
  if (!sourceOrigin)
    sourceOrigin = context().getSecurityOrigin();

  if (sourceOrigin->canRequestNoSuborigin(response.url()))
    return ResourceRequestBlockedReason::None;

  // Use the original response instead of the 304 response for a successful
  // revaldiation.
  const ResourceResponse& responseForAccessControl =
      (resource->isCacheValidator() && response.httpStatusCode() == 304)
          ? resource->response()
          : response;

  CrossOriginAccessControl::AccessStatus corsStatus =
      CrossOriginAccessControl::checkAccess(
          responseForAccessControl, resource->options().allowCredentials,
          sourceOrigin);
  if (corsStatus != CrossOriginAccessControl::kAccessAllowed) {
    resource->setCORSFailed();
    if (!unusedPreload) {
      String resourceType = Resource::resourceTypeToString(
          resource->getType(), resource->options().initiatorInfo.name);
      StringBuilder builder;
      builder.append("Access to ");
      builder.append(resourceType);
      builder.append(" at '");
      builder.append(response.url().getString());
      builder.append("' from origin '");
      builder.append(sourceOrigin->toString());
      builder.append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::accessControlErrorString(
          builder, corsStatus, responseForAccessControl, sourceOrigin,
          resource->lastResourceRequest().requestContext());
      context().addConsoleMessage(builder.toString());
    }
    return ResourceRequestBlockedReason::Other;
  }
  return ResourceRequestBlockedReason::None;
}

void ResourceLoader::didReceiveResponse(
    const WebURLResponse& webURLResponse,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!webURLResponse.isNull());

  const ResourceResponse& response = webURLResponse.toResourceResponse();

  if (response.wasFetchedViaServiceWorker()) {
    if (m_resource->options().corsEnabled == IsCORSEnabled &&
        response.wasFallbackRequiredByServiceWorker()) {
      ResourceRequest request = m_resource->lastResourceRequest();
      DCHECK_EQ(request.getServiceWorkerMode(),
                WebURLRequest::ServiceWorkerMode::All);
      // This code handles the case when a regular controlling service worker
      // doesn't handle a cross origin request. When this happens we still want
      // to give foreign fetch a chance to handle the request, so only skip the
      // controlling service worker for the fallback request. This is currently
      // safe because of http://crbug.com/604084 the
      // wasFallbackRequiredByServiceWorker flag is never set when foreign fetch
      // handled a request.
      if (!context().shouldLoadNewResource(m_resource->getType())) {
        // Cancel the request if we should not trigger a reload now.
        handleError(ResourceError::cancelledError(response.url()));
        return;
      }
      request.setServiceWorkerMode(WebURLRequest::ServiceWorkerMode::Foreign);
      restart(request);
      return;
    }

    // If the response is fetched via ServiceWorker, the original URL of the
    // response could be different from the URL of the request. We check the URL
    // not to load the resources which are forbidden by the page CSP.
    // https://w3c.github.io/webappsec-csp/#should-block-response
    const KURL& originalURL = response.originalURLViaServiceWorker();
    if (!originalURL.isEmpty()) {
      ResourceRequestBlockedReason blockedReason = context().allowResponse(
          m_resource->getType(), m_resource->resourceRequest(), originalURL,
          m_resource->options());
      if (blockedReason != ResourceRequestBlockedReason::None) {
        handleError(ResourceError::cancelledDueToAccessCheckError(
            originalURL, blockedReason));
        return;
      }
    }
  } else if (m_resource->options().corsEnabled == IsCORSEnabled) {
    ResourceRequestBlockedReason blockedReason =
        canAccessResponse(m_resource, response);
    if (blockedReason != ResourceRequestBlockedReason::None) {
      handleError(ResourceError::cancelledDueToAccessCheckError(response.url(),
                                                                blockedReason));
      return;
    }
  }

  context().dispatchDidReceiveResponse(
      m_resource->identifier(), response,
      m_resource->resourceRequest().frameType(),
      m_resource->resourceRequest().requestContext(), m_resource);

  m_resource->responseReceived(response, std::move(handle));
  if (!m_resource->loader())
    return;

  if (response.httpStatusCode() >= 400 &&
      !m_resource->shouldIgnoreHTTPStatusCodeErrors())
    handleError(ResourceError::cancelledError(response.url()));
}

void ResourceLoader::didReceiveResponse(const WebURLResponse& response) {
  didReceiveResponse(response, nullptr);
}

void ResourceLoader::didDownloadData(int length, int encodedDataLength) {
  context().dispatchDidDownloadData(m_resource->identifier(), length,
                                    encodedDataLength);
  m_resource->didDownloadData(length);
}

void ResourceLoader::didReceiveData(const char* data, int length) {
  CHECK_GE(length, 0);

  context().dispatchDidReceiveData(m_resource->identifier(), data, length);
  m_resource->addToDecodedBodyLength(length);
  m_resource->appendData(data, length);
}

void ResourceLoader::didReceiveTransferSizeUpdate(int transferSizeDiff) {
  DCHECK_GT(transferSizeDiff, 0);
  context().dispatchDidReceiveEncodedData(m_resource->identifier(),
                                          transferSizeDiff);
}

void ResourceLoader::didFinishLoadingFirstPartInMultipart() {
  network_instrumentation::endResourceLoad(
      m_resource->identifier(),
      network_instrumentation::RequestOutcome::Success);

  m_fetcher->handleLoaderFinish(m_resource.get(), 0,
                                ResourceFetcher::DidFinishFirstPartInMultipart);
}

void ResourceLoader::didFinishLoading(double finishTime,
                                      int64_t encodedDataLength,
                                      int64_t encodedBodyLength) {
  m_resource->setEncodedDataLength(encodedDataLength);
  m_resource->addToEncodedBodyLength(encodedBodyLength);

  m_loader.reset();

  network_instrumentation::endResourceLoad(
      m_resource->identifier(),
      network_instrumentation::RequestOutcome::Success);

  m_fetcher->handleLoaderFinish(m_resource.get(), finishTime,
                                ResourceFetcher::DidFinishLoading);
}

void ResourceLoader::didFail(const WebURLError& error,
                             int64_t encodedDataLength,
                             int64_t encodedBodyLength) {
  m_resource->setEncodedDataLength(encodedDataLength);
  m_resource->addToEncodedBodyLength(encodedBodyLength);
  handleError(error);
}

void ResourceLoader::handleError(const ResourceError& error) {
  if (m_isCacheAwareLoadingActivated && error.isCacheMiss() &&
      context().shouldLoadNewResource(m_resource->getType())) {
    m_resource->willReloadAfterDiskCacheMiss();
    m_isCacheAwareLoadingActivated = false;
    restart(m_resource->resourceRequest());
    return;
  }

  m_loader.reset();

  network_instrumentation::endResourceLoad(
      m_resource->identifier(), network_instrumentation::RequestOutcome::Fail);

  m_fetcher->handleLoaderError(m_resource.get(), error);
}

void ResourceLoader::requestSynchronously(const ResourceRequest& request) {
  // downloadToFile is not supported for synchronous requests.
  DCHECK(!request.downloadToFile());
  DCHECK(m_loader);
  DCHECK_EQ(request.priority(), ResourceLoadPriorityHighest);

  WrappedResourceRequest requestIn(request);
  WebURLResponse responseOut;
  WebURLError errorOut;
  WebData dataOut;
  int64_t encodedDataLength = WebURLLoaderClient::kUnknownEncodedDataLength;
  int64_t encodedBodyLength = 0;
  m_loader->loadSynchronously(requestIn, responseOut, errorOut, dataOut,
                              encodedDataLength, encodedBodyLength);

  // A message dispatched while synchronously fetching the resource
  // can bring about the cancellation of this load.
  if (!m_loader)
    return;
  if (errorOut.reason) {
    didFail(errorOut, encodedDataLength, encodedBodyLength);
    return;
  }
  didReceiveResponse(responseOut);
  if (!m_loader)
    return;
  DCHECK_GE(responseOut.toResourceResponse().encodedBodyLength(), 0);

  // Follow the async case convention of not calling didReceiveData or
  // appending data to m_resource if the response body is empty. Copying the
  // empty buffer is a noop in most cases, but is destructive in the case of
  // a 304, where it will overwrite the cached data we should be reusing.
  if (dataOut.size()) {
    context().dispatchDidReceiveData(m_resource->identifier(), dataOut.data(),
                                     dataOut.size());
    m_resource->setResourceBuffer(dataOut);
  }
  didFinishLoading(monotonicallyIncreasingTime(), encodedDataLength,
                   encodedBodyLength);
}

void ResourceLoader::dispose() {
  m_loader = nullptr;
}

void ResourceLoader::activateCacheAwareLoadingIfNeeded(
    const ResourceRequest& request) {
  DCHECK(!m_isCacheAwareLoadingActivated);

  if (m_resource->options().cacheAwareLoadingEnabled !=
      IsCacheAwareLoadingEnabled)
    return;

  // Synchronous requests are not supported.
  if (m_resource->options().synchronousPolicy == RequestSynchronously)
    return;

  // Don't activate on Resource revalidation.
  if (m_resource->isCacheValidator())
    return;

  // Don't activate if cache policy is explicitly set.
  if (request.getCachePolicy() != WebCachePolicy::UseProtocolCachePolicy)
    return;

  m_isCacheAwareLoadingActivated = true;
}

}  // namespace blink

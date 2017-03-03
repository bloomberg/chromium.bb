/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
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

#include "core/loader/DocumentThreadableLoader.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/loader/private/CrossOriginPreflightResultCache.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include "wtf/WeakPtr.h"

namespace blink {

namespace {

class EmptyDataHandle final : public WebDataConsumerHandle {
 private:
  class EmptyDataReader final : public WebDataConsumerHandle::Reader {
   public:
    explicit EmptyDataReader(WebDataConsumerHandle::Client* client)
        : m_factory(this) {
      Platform::current()->currentThread()->getWebTaskRunner()->postTask(
          BLINK_FROM_HERE,
          WTF::bind(&EmptyDataReader::notify, m_factory.createWeakPtr(),
                    WTF::unretained(client)));
    }

   private:
    Result beginRead(const void** buffer,
                     WebDataConsumerHandle::Flags,
                     size_t* available) override {
      *available = 0;
      *buffer = nullptr;
      return Done;
    }
    Result endRead(size_t) override {
      return WebDataConsumerHandle::UnexpectedError;
    }
    void notify(WebDataConsumerHandle::Client* client) {
      client->didGetReadable();
    }
    WeakPtrFactory<EmptyDataReader> m_factory;
  };

  std::unique_ptr<Reader> obtainReader(Client* client) override {
    return WTF::makeUnique<EmptyDataReader>(client);
  }
  const char* debugName() const override { return "EmptyDataHandle"; }
};

// No-CORS requests are allowed for all these contexts, and plugin contexts with
// private permission when we set ServiceWorkerMode to None in
// PepperURLLoaderHost.
bool IsNoCORSAllowedContext(
    WebURLRequest::RequestContext context,
    WebURLRequest::ServiceWorkerMode serviceWorkerMode) {
  switch (context) {
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextVideo:
    case WebURLRequest::RequestContextObject:
    case WebURLRequest::RequestContextFavicon:
    case WebURLRequest::RequestContextImage:
    case WebURLRequest::RequestContextScript:
    case WebURLRequest::RequestContextWorker:
    case WebURLRequest::RequestContextSharedWorker:
      return true;
    case WebURLRequest::RequestContextPlugin:
      return serviceWorkerMode == WebURLRequest::ServiceWorkerMode::None;
    default:
      return false;
  }
}

}  // namespace

// Max number of CORS redirects handled in DocumentThreadableLoader. Same number
// as net/url_request/url_request.cc, and same number as
// https://fetch.spec.whatwg.org/#concept-http-fetch, Step 4.
// FIXME: currently the number of redirects is counted and limited here and in
// net/url_request/url_request.cc separately.
static const int kMaxCORSRedirects = 20;

void DocumentThreadableLoader::loadResourceSynchronously(
    Document& document,
    const ResourceRequest& request,
    ThreadableLoaderClient& client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions) {
  (new DocumentThreadableLoader(*ThreadableLoadingContext::create(document),
                                &client, LoadSynchronously, options,
                                resourceLoaderOptions))
      ->start(request);
}

DocumentThreadableLoader* DocumentThreadableLoader::create(
    ThreadableLoadingContext& loadingContext,
    ThreadableLoaderClient* client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions) {
  return new DocumentThreadableLoader(loadingContext, client,
                                      LoadAsynchronously, options,
                                      resourceLoaderOptions);
}

DocumentThreadableLoader::DocumentThreadableLoader(
    ThreadableLoadingContext& loadingContext,
    ThreadableLoaderClient* client,
    BlockingBehavior blockingBehavior,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions)
    : m_client(client),
      m_loadingContext(&loadingContext),
      m_options(options),
      m_resourceLoaderOptions(resourceLoaderOptions),
      m_forceDoNotAllowStoredCredentials(false),
      m_securityOrigin(m_resourceLoaderOptions.securityOrigin),
      m_sameOriginRequest(false),
      m_isUsingDataConsumerHandle(false),
      m_async(blockingBehavior == LoadAsynchronously),
      m_requestContext(WebURLRequest::RequestContextUnspecified),
      m_timeoutTimer(m_loadingContext->getTaskRunner(TaskType::Networking),
                     this,
                     &DocumentThreadableLoader::didTimeout),
      m_requestStartedSeconds(0.0),
      m_corsRedirectLimit(m_options.crossOriginRequestPolicy == UseAccessControl
                              ? kMaxCORSRedirects
                              : 0),
      m_redirectMode(WebURLRequest::FetchRedirectModeFollow),
      m_overrideReferrer(false) {
  DCHECK(client);
}

void DocumentThreadableLoader::start(const ResourceRequest& request) {
  // Setting an outgoing referer is only supported in the async code path.
  DCHECK(m_async || request.httpReferrer().isEmpty());

  m_sameOriginRequest =
      getSecurityOrigin()->canRequestNoSuborigin(request.url());
  m_requestContext = request.requestContext();
  m_redirectMode = request.fetchRedirectMode();

  if (!m_sameOriginRequest &&
      m_options.crossOriginRequestPolicy == DenyCrossOriginRequests) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(document(),
                                                                 m_client);
    ThreadableLoaderClient* client = m_client;
    clear();
    client->didFail(ResourceError(errorDomainBlinkInternal, 0,
                                  request.url().getString(),
                                  "Cross origin requests are not supported."));
    return;
  }

  m_requestStartedSeconds = monotonicallyIncreasingTime();

  // Save any headers on the request here. If this request redirects
  // cross-origin, we cancel the old request create a new one, and copy these
  // headers.
  m_requestHeaders = request.httpHeaderFields();

  // DocumentThreadableLoader is used by all javascript initiated fetch, so we
  // use this chance to record non-GET fetch script requests. However, this is
  // based on the following assumptions, so please be careful when adding
  // similar logic:
  // - ThreadableLoader is used as backend for all javascript initiated network
  //   fetches.
  // - Note that ThreadableLoader is also used for non-network fetch such as
  //   FileReaderLoader. However it emulates GET method so signal is not
  //   recorded here.
  // - ThreadableLoader w/ non-GET request is only created from javascript
  //   initiated fetch.
  // - Some non-script initiated fetches such as WorkerScriptLoader also use
  //   ThreadableLoader, but they are guaranteed to use GET method.
  if (request.httpMethod() != HTTPNames::GET && document()) {
    if (Page* page = document()->page())
      page->chromeClient().didObserveNonGetFetchFromScript();
  }

  ResourceRequest newRequest(request);
  if (m_requestContext != WebURLRequest::RequestContextFetch) {
    // When the request context is not "fetch", |crossOriginRequestPolicy|
    // represents the fetch request mode, and |credentialsRequested| represents
    // the fetch credentials mode. So we set those flags here so that we can see
    // the correct request mode and credentials mode in the service worker's
    // fetch event handler.
    switch (m_options.crossOriginRequestPolicy) {
      case DenyCrossOriginRequests:
        newRequest.setFetchRequestMode(
            WebURLRequest::FetchRequestModeSameOrigin);
        break;
      case UseAccessControl:
        if (m_options.preflightPolicy == ForcePreflight) {
          newRequest.setFetchRequestMode(
              WebURLRequest::FetchRequestModeCORSWithForcedPreflight);
        } else {
          newRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORS);
        }
        break;
      case AllowCrossOriginRequests:
        SECURITY_CHECK(IsNoCORSAllowedContext(m_requestContext,
                                              request.getServiceWorkerMode()));
        newRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeNoCORS);
        break;
    }
    if (m_resourceLoaderOptions.allowCredentials == AllowStoredCredentials) {
      newRequest.setFetchCredentialsMode(
          WebURLRequest::FetchCredentialsModeInclude);
    } else {
      newRequest.setFetchCredentialsMode(
          WebURLRequest::FetchCredentialsModeSameOrigin);
    }
  }

  // We assume that ServiceWorker is skipped for sync requests and unsupported
  // protocol requests by content/ code.
  if (m_async &&
      request.getServiceWorkerMode() == WebURLRequest::ServiceWorkerMode::All &&
      SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(
          request.url().protocol()) &&
      m_loadingContext->getResourceFetcher()->isControlledByServiceWorker()) {
    if (newRequest.fetchRequestMode() == WebURLRequest::FetchRequestModeCORS ||
        newRequest.fetchRequestMode() ==
            WebURLRequest::FetchRequestModeCORSWithForcedPreflight) {
      m_fallbackRequestForServiceWorker = ResourceRequest(request);
      // m_fallbackRequestForServiceWorker is used when a regular controlling
      // service worker doesn't handle a cross origin request. When this happens
      // we still want to give foreign fetch a chance to handle the request, so
      // only skip the controlling service worker for the fallback request. This
      // is currently safe because of http://crbug.com/604084 the
      // wasFallbackRequiredByServiceWorker flag is never set when foreign fetch
      // handled a request.
      m_fallbackRequestForServiceWorker.setServiceWorkerMode(
          WebURLRequest::ServiceWorkerMode::Foreign);
    }
    loadRequest(newRequest, m_resourceLoaderOptions);
    return;
  }

  dispatchInitialRequest(newRequest);
}

void DocumentThreadableLoader::dispatchInitialRequest(
    const ResourceRequest& request) {
  if (!request.isExternalRequest() &&
      (m_sameOriginRequest ||
       m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)) {
    loadRequest(request, m_resourceLoaderOptions);
    return;
  }

  DCHECK(m_options.crossOriginRequestPolicy == UseAccessControl ||
         request.isExternalRequest());

  makeCrossOriginAccessRequest(request);
}

void DocumentThreadableLoader::prepareCrossOriginRequest(
    ResourceRequest& request) {
  if (getSecurityOrigin())
    request.setHTTPOrigin(getSecurityOrigin());
  if (m_overrideReferrer)
    request.setHTTPReferrer(m_referrerAfterRedirect);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(
    const ResourceRequest& request) {
  DCHECK(m_options.crossOriginRequestPolicy == UseAccessControl ||
         request.isExternalRequest());
  DCHECK(m_client);
  DCHECK(!resource());

  // Cross-origin requests are only allowed certain registered schemes. We would
  // catch this when checking response headers later, but there is no reason to
  // send a request, preflighted or not, that's guaranteed to be denied.
  if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(
          request.url().protocol())) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(document(),
                                                                 m_client);
    dispatchDidFailAccessControlCheck(ResourceError(
        errorDomainBlinkInternal, 0, request.url().getString(),
        "Cross origin requests are only supported for protocol schemes: " +
            SchemeRegistry::listOfCORSEnabledURLSchemes() + "."));
    return;
  }

  // Non-secure origins may not make "external requests":
  // https://mikewest.github.io/cors-rfc1918/#integration-fetch
  if (!m_loadingContext->isSecureContext() && request.isExternalRequest()) {
    dispatchDidFailAccessControlCheck(
        ResourceError(errorDomainBlinkInternal, 0, request.url().getString(),
                      "Requests to internal network resources are not allowed "
                      "from non-secure contexts (see https://goo.gl/Y0ZkNV). "
                      "This is an experimental restriction which is part of "
                      "'https://mikewest.github.io/cors-rfc1918/'."));
    return;
  }

  ResourceRequest crossOriginRequest(request);
  ResourceLoaderOptions crossOriginOptions(m_resourceLoaderOptions);

  crossOriginRequest.removeUserAndPassFromURL();

  crossOriginRequest.setAllowStoredCredentials(effectiveAllowCredentials() ==
                                               AllowStoredCredentials);

  // We update the credentials mode according to effectiveAllowCredentials()
  // here for backward compatibility. But this is not correct.
  // FIXME: We should set it in the caller of DocumentThreadableLoader.
  crossOriginRequest.setFetchCredentialsMode(
      effectiveAllowCredentials() == AllowStoredCredentials
          ? WebURLRequest::FetchCredentialsModeInclude
          : WebURLRequest::FetchCredentialsModeOmit);

  // We use isSimpleOrForbiddenRequest() here since |request| may have been
  // modified in the process of loading (not from the user's input). For
  // example, referrer. We need to accept them. For security, we must reject
  // forbidden headers/methods at the point we accept user's input. Not here.
  if (!request.isExternalRequest() &&
      ((m_options.preflightPolicy == ConsiderPreflight &&
        FetchUtils::isSimpleOrForbiddenRequest(request.httpMethod(),
                                               request.httpHeaderFields())) ||
       m_options.preflightPolicy == PreventPreflight)) {
    prepareCrossOriginRequest(crossOriginRequest);
    loadRequest(crossOriginRequest, crossOriginOptions);
  } else {
    // Explicitly set the ServiceWorkerMode to None here. Although the page is
    // not controlled by a SW at this point, a new SW may be controlling the
    // page when this request gets sent later. We should not send the actual
    // request to the SW. https://crbug.com/604583
    // Similarly we don't want any requests that could involve a CORS preflight
    // to get intercepted by a foreign fetch service worker, even if we have the
    // result of the preflight cached already. https://crbug.com/674370
    crossOriginRequest.setServiceWorkerMode(
        WebURLRequest::ServiceWorkerMode::None);

    bool shouldForcePreflight = request.isExternalRequest() ||
                                probe::shouldForceCORSPreflight(document());
    bool canSkipPreflight =
        CrossOriginPreflightResultCache::shared().canSkipPreflight(
            getSecurityOrigin()->toString(), crossOriginRequest.url(),
            effectiveAllowCredentials(), crossOriginRequest.httpMethod(),
            crossOriginRequest.httpHeaderFields());
    if (canSkipPreflight && !shouldForcePreflight) {
      prepareCrossOriginRequest(crossOriginRequest);
      loadRequest(crossOriginRequest, crossOriginOptions);
    } else {
      ResourceRequest preflightRequest =
          createAccessControlPreflightRequest(crossOriginRequest);
      // TODO(tyoshino): Call prepareCrossOriginRequest(preflightRequest) to
      // also set the referrer header.
      if (getSecurityOrigin())
        preflightRequest.setHTTPOrigin(getSecurityOrigin());

      // Create a ResourceLoaderOptions for preflight.
      ResourceLoaderOptions preflightOptions = crossOriginOptions;
      preflightOptions.allowCredentials = DoNotAllowStoredCredentials;

      m_actualRequest = crossOriginRequest;
      m_actualOptions = crossOriginOptions;

      loadRequest(preflightRequest, preflightOptions);
    }
  }
}

DocumentThreadableLoader::~DocumentThreadableLoader() {
  CHECK(!m_client);
  DCHECK(!m_resource);
}

void DocumentThreadableLoader::overrideTimeout(
    unsigned long timeoutMilliseconds) {
  DCHECK(m_async);

  // |m_requestStartedSeconds| == 0.0 indicates loading is already finished and
  // |m_timeoutTimer| is already stopped, and thus we do nothing for such cases.
  // See https://crbug.com/551663 for details.
  if (m_requestStartedSeconds <= 0.0)
    return;

  m_timeoutTimer.stop();
  // At the time of this method's implementation, it is only ever called by
  // XMLHttpRequest, when the timeout attribute is set after sending the
  // request.
  //
  // The XHR request says to resolve the time relative to when the request
  // was initially sent, however other uses of this method may need to
  // behave differently, in which case this should be re-arranged somehow.
  if (timeoutMilliseconds) {
    double elapsedTime =
        monotonicallyIncreasingTime() - m_requestStartedSeconds;
    double nextFire = timeoutMilliseconds / 1000.0;
    double resolvedTime = std::max(nextFire - elapsedTime, 0.0);
    m_timeoutTimer.startOneShot(resolvedTime, BLINK_FROM_HERE);
  }
}

void DocumentThreadableLoader::cancel() {
  // Cancel can re-enter, and therefore |resource()| might be null here as a
  // result.
  if (!m_client || !resource()) {
    clear();
    return;
  }

  // FIXME: This error is sent to the client in didFail(), so it should not be
  // an internal one. Use LocalFrameClient::cancelledError() instead.
  ResourceError error(errorDomainBlinkInternal, 0, resource()->url(),
                      "Load cancelled");
  error.setIsCancellation(true);

  dispatchDidFail(error);
}

void DocumentThreadableLoader::setDefersLoading(bool value) {
  if (resource())
    resource()->setDefersLoading(value);
}

void DocumentThreadableLoader::clear() {
  m_client = nullptr;
  m_timeoutTimer.stop();
  m_requestStartedSeconds = 0.0;
  clearResource();
}

// In this method, we can clear |request| to tell content::WebURLLoaderImpl of
// Chromium not to follow the redirect. This works only when this method is
// called by RawResource::willSendRequest(). If called by
// RawResource::didAddClient(), clearing |request| won't be propagated to
// content::WebURLLoaderImpl. So, this loader must also get detached from the
// resource by calling clearResource().
bool DocumentThreadableLoader::redirectReceived(
    Resource* resource,
    const ResourceRequest& request,
    const ResourceResponse& redirectResponse) {
  DCHECK(m_client);
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_async);

  m_checker.redirectReceived();

  if (!m_actualRequest.isNull()) {
    reportResponseReceived(resource->identifier(), redirectResponse);

    handlePreflightFailure(redirectResponse.url().getString(),
                           "Response for preflight is invalid (redirect)");

    return false;
  }

  if (m_redirectMode == WebURLRequest::FetchRedirectModeManual) {
    // We use |m_redirectMode| to check the original redirect mode. |request| is
    // a new request for redirect. So we don't set the redirect mode of it in
    // WebURLLoaderImpl::Context::OnReceivedRedirect().
    DCHECK(request.useStreamOnResponse());
    // There is no need to read the body of redirect response because there is
    // no way to read the body of opaque-redirect filtered response's internal
    // response.
    // TODO(horo): If we support any API which expose the internal body, we will
    // have to read the body. And also HTTPCache changes will be needed because
    // it doesn't store the body of redirect responses.
    responseReceived(resource, redirectResponse,
                     WTF::makeUnique<EmptyDataHandle>());

    if (m_client) {
      DCHECK(m_actualRequest.isNull());
      notifyFinished(resource);
    }

    return false;
  }

  if (m_redirectMode == WebURLRequest::FetchRedirectModeError) {
    ThreadableLoaderClient* client = m_client;
    clear();
    client->didFailRedirectCheck();

    return false;
  }

  // Allow same origin requests to continue after allowing clients to audit the
  // redirect.
  if (isAllowedRedirect(request.url())) {
    m_client->didReceiveRedirectTo(request.url());
    if (m_client->isDocumentThreadableLoaderClient()) {
      return static_cast<DocumentThreadableLoaderClient*>(m_client)
          ->willFollowRedirect(request, redirectResponse);
    }
    return true;
  }

  if (m_corsRedirectLimit <= 0) {
    ThreadableLoaderClient* client = m_client;
    clear();
    client->didFailRedirectCheck();
    return false;
  }

  --m_corsRedirectLimit;

  if (document() && document()->frame()) {
    probe::didReceiveCORSRedirectResponse(
        document()->frame(), resource->identifier(),
        document()->frame()->loader().documentLoader(), redirectResponse,
        resource);
  }

  String accessControlErrorDescription;

  CrossOriginAccessControl::RedirectStatus redirectStatus =
      CrossOriginAccessControl::checkRedirectLocation(request.url());
  bool allowRedirect =
      redirectStatus == CrossOriginAccessControl::kRedirectSuccess;
  if (!allowRedirect) {
    StringBuilder builder;
    builder.append("Redirect from '");
    builder.append(redirectResponse.url().getString());
    builder.append("' has been blocked by CORS policy: ");
    CrossOriginAccessControl::redirectErrorString(builder, redirectStatus,
                                                  request.url());
    accessControlErrorDescription = builder.toString();
  } else if (!m_sameOriginRequest) {
    // The redirect response must pass the access control check if the original
    // request was not same-origin.
    CrossOriginAccessControl::AccessStatus corsStatus =
        CrossOriginAccessControl::checkAccess(
            redirectResponse, effectiveAllowCredentials(), getSecurityOrigin());
    allowRedirect = corsStatus == CrossOriginAccessControl::kAccessAllowed;
    if (!allowRedirect) {
      StringBuilder builder;
      builder.append("Redirect from '");
      builder.append(redirectResponse.url().getString());
      builder.append("' to '");
      builder.append(request.url().getString());
      builder.append("' has been blocked by CORS policy: ");
      CrossOriginAccessControl::accessControlErrorString(
          builder, corsStatus, redirectResponse, getSecurityOrigin(),
          m_requestContext);
      accessControlErrorDescription = builder.toString();
    }
  }

  if (!allowRedirect) {
    dispatchDidFailAccessControlCheck(ResourceError(
        errorDomainBlinkInternal, 0, redirectResponse.url().getString(),
        accessControlErrorDescription));
    return false;
  }

  m_client->didReceiveRedirectTo(request.url());

  // FIXME: consider combining this with CORS redirect handling performed by
  // CrossOriginAccessControl::handleRedirect().
  clearResource();

  // If the original request wasn't same-origin, then if the request URL origin
  // is not same origin with the original URL origin, set the source origin to a
  // globally unique identifier. (If the original request was same-origin, the
  // origin of the new request should be the original URL origin.)
  if (!m_sameOriginRequest) {
    RefPtr<SecurityOrigin> originalOrigin =
        SecurityOrigin::create(redirectResponse.url());
    RefPtr<SecurityOrigin> requestOrigin =
        SecurityOrigin::create(request.url());
    if (!originalOrigin->isSameSchemeHostPort(requestOrigin.get()))
      m_securityOrigin = SecurityOrigin::createUnique();
  }
  // Force any subsequent requests to use these checks.
  m_sameOriginRequest = false;

  // Since the request is no longer same-origin, if the user didn't request
  // credentials in the first place, update our state so we neither request them
  // nor expect they must be allowed.
  if (m_resourceLoaderOptions.credentialsRequested ==
      ClientDidNotRequestCredentials)
    m_forceDoNotAllowStoredCredentials = true;

  // Save the referrer to use when following the redirect.
  m_overrideReferrer = true;
  m_referrerAfterRedirect =
      Referrer(request.httpReferrer(), request.getReferrerPolicy());

  ResourceRequest crossOriginRequest(request);

  // Remove any headers that may have been added by the network layer that cause
  // access control to fail.
  crossOriginRequest.clearHTTPReferrer();
  crossOriginRequest.clearHTTPOrigin();
  crossOriginRequest.clearHTTPUserAgent();
  // Add any request headers which we previously saved from the
  // original request.
  for (const auto& header : m_requestHeaders)
    crossOriginRequest.setHTTPHeaderField(header.key, header.value);
  makeCrossOriginAccessRequest(crossOriginRequest);

  return false;
}

void DocumentThreadableLoader::redirectBlocked() {
  m_checker.redirectBlocked();

  // Tells the client that a redirect was received but not followed (for an
  // unknown reason).
  ThreadableLoaderClient* client = m_client;
  clear();
  client->didFailRedirectCheck();
}

void DocumentThreadableLoader::dataSent(Resource* resource,
                                        unsigned long long bytesSent,
                                        unsigned long long totalBytesToBeSent) {
  DCHECK(m_client);
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_async);

  m_checker.dataSent();
  m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::dataDownloaded(Resource* resource,
                                              int dataLength) {
  DCHECK(m_client);
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_actualRequest.isNull());
  DCHECK(m_async);

  m_checker.dataDownloaded();
  m_client->didDownloadData(dataLength);
}

void DocumentThreadableLoader::didReceiveResourceTiming(
    Resource* resource,
    const ResourceTimingInfo& info) {
  DCHECK(m_client);
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_async);

  m_client->didReceiveResourceTiming(info);
}

void DocumentThreadableLoader::responseReceived(
    Resource* resource,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_async);

  m_checker.responseReceived();

  if (handle)
    m_isUsingDataConsumerHandle = true;

  handleResponse(resource->identifier(), response, std::move(handle));
}

void DocumentThreadableLoader::handlePreflightResponse(
    const ResourceResponse& response) {
  String accessControlErrorDescription;

  CrossOriginAccessControl::AccessStatus corsStatus =
      CrossOriginAccessControl::checkAccess(
          response, effectiveAllowCredentials(), getSecurityOrigin());
  if (corsStatus != CrossOriginAccessControl::kAccessAllowed) {
    StringBuilder builder;
    builder.append(
        "Response to preflight request doesn't pass access "
        "control check: ");
    CrossOriginAccessControl::accessControlErrorString(
        builder, corsStatus, response, getSecurityOrigin(), m_requestContext);
    handlePreflightFailure(response.url().getString(), builder.toString());
    return;
  }

  CrossOriginAccessControl::PreflightStatus preflightStatus =
      CrossOriginAccessControl::checkPreflight(response);
  if (preflightStatus != CrossOriginAccessControl::kPreflightSuccess) {
    StringBuilder builder;
    CrossOriginAccessControl::preflightErrorString(builder, preflightStatus,
                                                   response);
    handlePreflightFailure(response.url().getString(), builder.toString());
    return;
  }

  if (m_actualRequest.isExternalRequest()) {
    CrossOriginAccessControl::PreflightStatus externalPreflightStatus =
        CrossOriginAccessControl::checkExternalPreflight(response);
    if (externalPreflightStatus !=
        CrossOriginAccessControl::kPreflightSuccess) {
      StringBuilder builder;
      CrossOriginAccessControl::preflightErrorString(
          builder, externalPreflightStatus, response);
      handlePreflightFailure(response.url().getString(), builder.toString());
      return;
    }
  }

  std::unique_ptr<CrossOriginPreflightResultCacheItem> preflightResult =
      WTF::wrapUnique(
          new CrossOriginPreflightResultCacheItem(effectiveAllowCredentials()));
  if (!preflightResult->parse(response, accessControlErrorDescription) ||
      !preflightResult->allowsCrossOriginMethod(
          m_actualRequest.httpMethod(), accessControlErrorDescription) ||
      !preflightResult->allowsCrossOriginHeaders(
          m_actualRequest.httpHeaderFields(), accessControlErrorDescription)) {
    handlePreflightFailure(response.url().getString(),
                           accessControlErrorDescription);
    return;
  }

  CrossOriginPreflightResultCache::shared().appendEntry(
      getSecurityOrigin()->toString(), m_actualRequest.url(),
      std::move(preflightResult));
}

void DocumentThreadableLoader::reportResponseReceived(
    unsigned long identifier,
    const ResourceResponse& response) {
  LocalFrame* frame = document() ? document()->frame() : nullptr;
  if (!frame)
    return;
  TRACE_EVENT1(
      "devtools.timeline", "ResourceReceiveResponse", "data",
      InspectorReceiveResponseEvent::data(identifier, frame, response));
  DocumentLoader* loader = frame->loader().documentLoader();
  probe::didReceiveResourceResponse(frame, identifier, loader, response,
                                    resource());
  frame->console().reportResourceResponseReceived(loader, identifier, response);
}

void DocumentThreadableLoader::handleResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(m_client);

  if (!m_actualRequest.isNull()) {
    reportResponseReceived(identifier, response);
    handlePreflightResponse(response);
    return;
  }

  if (response.wasFetchedViaServiceWorker()) {
    if (response.wasFetchedViaForeignFetch())
      m_loadingContext->recordUseCount(UseCounter::ForeignFetchInterception);
    if (response.wasFallbackRequiredByServiceWorker()) {
      // At this point we must have m_fallbackRequestForServiceWorker. (For
      // SharedWorker the request won't be CORS or CORS-with-preflight,
      // therefore fallback-to-network is handled in the browser process when
      // the ServiceWorker does not call respondWith().)
      DCHECK(!m_fallbackRequestForServiceWorker.isNull());
      reportResponseReceived(identifier, response);
      loadFallbackRequestForServiceWorker();
      return;
    }
    m_fallbackRequestForServiceWorker = ResourceRequest();
    m_client->didReceiveResponse(identifier, response, std::move(handle));
    return;
  }

  // Even if the request met the conditions to get handled by a Service Worker
  // in the constructor of this class (and therefore
  // |m_fallbackRequestForServiceWorker| is set), the Service Worker may skip
  // processing the request. Only if the request is same origin, the skipped
  // response may come here (wasFetchedViaServiceWorker() returns false) since
  // such a request doesn't have to go through the CORS algorithm by calling
  // loadFallbackRequestForServiceWorker().
  // FIXME: We should use |m_sameOriginRequest| when we will support Suborigins
  // (crbug.com/336894) for Service Worker.
  DCHECK(
      m_fallbackRequestForServiceWorker.isNull() ||
      getSecurityOrigin()->canRequest(m_fallbackRequestForServiceWorker.url()));
  m_fallbackRequestForServiceWorker = ResourceRequest();

  if (!m_sameOriginRequest &&
      m_options.crossOriginRequestPolicy == UseAccessControl) {
    CrossOriginAccessControl::AccessStatus corsStatus =
        CrossOriginAccessControl::checkAccess(
            response, effectiveAllowCredentials(), getSecurityOrigin());
    if (corsStatus != CrossOriginAccessControl::kAccessAllowed) {
      reportResponseReceived(identifier, response);
      StringBuilder builder;
      CrossOriginAccessControl::accessControlErrorString(
          builder, corsStatus, response, getSecurityOrigin(), m_requestContext);
      dispatchDidFailAccessControlCheck(
          ResourceError(errorDomainBlinkInternal, 0, response.url().getString(),
                        builder.toString()));
      return;
    }
  }

  m_client->didReceiveResponse(identifier, response, std::move(handle));
}

void DocumentThreadableLoader::setSerializedCachedMetadata(Resource*,
                                                           const char* data,
                                                           size_t size) {
  m_checker.setSerializedCachedMetadata();

  if (!m_actualRequest.isNull())
    return;
  m_client->didReceiveCachedMetadata(data, size);
}

void DocumentThreadableLoader::dataReceived(Resource* resource,
                                            const char* data,
                                            size_t dataLength) {
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_async);

  m_checker.dataReceived();

  if (m_isUsingDataConsumerHandle)
    return;

  // TODO(junov): Fix the ThreadableLoader ecosystem to use size_t. Until then,
  // we use safeCast to trap potential overflows.
  handleReceivedData(data, safeCast<unsigned>(dataLength));
}

void DocumentThreadableLoader::handleReceivedData(const char* data,
                                                  size_t dataLength) {
  DCHECK(m_client);

  // Preflight data should be invisible to clients.
  if (!m_actualRequest.isNull())
    return;

  DCHECK(m_fallbackRequestForServiceWorker.isNull());

  m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::notifyFinished(Resource* resource) {
  DCHECK(m_client);
  DCHECK_EQ(resource, this->resource());
  DCHECK(m_async);

  m_checker.notifyFinished(resource);

  if (resource->errorOccurred()) {
    dispatchDidFail(resource->resourceError());
  } else {
    handleSuccessfulFinish(resource->identifier(), resource->loadFinishTime());
  }
}

void DocumentThreadableLoader::handleSuccessfulFinish(unsigned long identifier,
                                                      double finishTime) {
  DCHECK(m_fallbackRequestForServiceWorker.isNull());

  if (!m_actualRequest.isNull()) {
    // FIXME: Timeout should be applied to whole fetch, not for each of
    // preflight and actual request.
    m_timeoutTimer.stop();
    DCHECK(!m_sameOriginRequest);
    DCHECK_EQ(m_options.crossOriginRequestPolicy, UseAccessControl);
    loadActualRequest();
    return;
  }

  ThreadableLoaderClient* client = m_client;
  // Protect the resource in |didFinishLoading| in order not to release the
  // downloaded file.
  Persistent<Resource> protect = resource();
  clear();
  client->didFinishLoading(identifier, finishTime);
}

void DocumentThreadableLoader::didTimeout(TimerBase* timer) {
  DCHECK(m_async);
  DCHECK_EQ(timer, &m_timeoutTimer);
  // clearResource() may be called in clear() and some other places. clear()
  // calls stop() on |m_timeoutTimer|. In the other places, the resource is set
  // again. If the creation fails, clear() is called. So, here, resource() is
  // always non-nullptr.
  DCHECK(resource());
  // When |m_client| is set to nullptr only in clear() where |m_timeoutTimer|
  // is stopped. So, |m_client| is always non-nullptr here.
  DCHECK(m_client);

  // Using values from net/base/net_error_list.h ERR_TIMED_OUT, Same as existing
  // FIXME above - this error should be coming from LocalFrameClient to be
  // identifiable.
  static const int timeoutError = -7;
  ResourceError error("net", timeoutError, resource()->url(), String());
  error.setIsTimeout(true);

  dispatchDidFail(error);
}

void DocumentThreadableLoader::loadFallbackRequestForServiceWorker() {
  clearResource();
  ResourceRequest fallbackRequest(m_fallbackRequestForServiceWorker);
  m_fallbackRequestForServiceWorker = ResourceRequest();
  dispatchInitialRequest(fallbackRequest);
}

void DocumentThreadableLoader::loadActualRequest() {
  ResourceRequest actualRequest = m_actualRequest;
  ResourceLoaderOptions actualOptions = m_actualOptions;
  m_actualRequest = ResourceRequest();
  m_actualOptions = ResourceLoaderOptions();

  clearResource();

  prepareCrossOriginRequest(actualRequest);
  loadRequest(actualRequest, actualOptions);
}

void DocumentThreadableLoader::handlePreflightFailure(
    const String& url,
    const String& errorDescription) {
  ResourceError error(errorDomainBlinkInternal, 0, url, errorDescription);

  // Prevent handleSuccessfulFinish() from bypassing access check.
  m_actualRequest = ResourceRequest();

  dispatchDidFailAccessControlCheck(error);
}

void DocumentThreadableLoader::dispatchDidFailAccessControlCheck(
    const ResourceError& error) {
  ThreadableLoaderClient* client = m_client;
  clear();
  client->didFailAccessControlCheck(error);
}

void DocumentThreadableLoader::dispatchDidFail(const ResourceError& error) {
  ThreadableLoaderClient* client = m_client;
  clear();
  client->didFail(error);
}

void DocumentThreadableLoader::loadRequestAsync(
    const ResourceRequest& request,
    ResourceLoaderOptions resourceLoaderOptions) {
  if (!m_actualRequest.isNull())
    resourceLoaderOptions.dataBufferingPolicy = BufferData;

  if (m_options.timeoutMilliseconds > 0) {
    m_timeoutTimer.startOneShot(m_options.timeoutMilliseconds / 1000.0,
                                BLINK_FROM_HERE);
  }

  FetchRequest newRequest(request, m_options.initiator, resourceLoaderOptions);
  if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
    newRequest.setOriginRestriction(FetchRequest::NoOriginRestriction);
  DCHECK(!resource());

  ResourceFetcher* fetcher = m_loadingContext->getResourceFetcher();
  if (request.requestContext() == WebURLRequest::RequestContextVideo ||
      request.requestContext() == WebURLRequest::RequestContextAudio)
    setResource(RawResource::fetchMedia(newRequest, fetcher));
  else if (request.requestContext() == WebURLRequest::RequestContextManifest)
    setResource(RawResource::fetchManifest(newRequest, fetcher));
  else
    setResource(RawResource::fetch(newRequest, fetcher));

  if (!resource()) {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(document(),
                                                                 m_client);
    ThreadableLoaderClient* client = m_client;
    clear();
    // setResource() might call notifyFinished() and thus clear()
    // synchronously, and in such cases ThreadableLoaderClient is already
    // notified and |client| is null.
    if (!client)
      return;
    client->didFail(ResourceError(errorDomainBlinkInternal, 0,
                                  request.url().getString(),
                                  "Failed to start loading."));
    return;
  }

  if (resource()->isLoading()) {
    unsigned long identifier = resource()->identifier();
    probe::documentThreadableLoaderStartedLoadingForClient(
        document(), identifier, m_client);
  } else {
    probe::documentThreadableLoaderFailedToStartLoadingForClient(document(),
                                                                 m_client);
  }
}

void DocumentThreadableLoader::loadRequestSync(
    const ResourceRequest& request,
    ResourceLoaderOptions resourceLoaderOptions) {
  FetchRequest fetchRequest(request, m_options.initiator,
                            resourceLoaderOptions);
  if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
    fetchRequest.setOriginRestriction(FetchRequest::NoOriginRestriction);
  Resource* resource = RawResource::fetchSynchronously(
      fetchRequest, m_loadingContext->getResourceFetcher());
  ResourceResponse response =
      resource ? resource->response() : ResourceResponse();
  unsigned long identifier = resource
                                 ? resource->identifier()
                                 : std::numeric_limits<unsigned long>::max();
  ResourceError error = resource ? resource->resourceError() : ResourceError();

  probe::documentThreadableLoaderStartedLoadingForClient(document(), identifier,
                                                         m_client);
  ThreadableLoaderClient* client = m_client;

  if (!resource) {
    m_client = nullptr;
    client->didFail(error);
    return;
  }

  const KURL& requestURL = request.url();

  // No exception for file:/// resources, see <rdar://problem/4962298>. Also, if
  // we have an HTTP response, then it wasn't a network error in fact.
  if (!error.isNull() && !requestURL.isLocalFile() &&
      response.httpStatusCode() <= 0) {
    m_client = nullptr;
    client->didFail(error);
    return;
  }

  // FIXME: A synchronous request does not tell us whether a redirect happened
  // or not, so we guess by comparing the request and response URLs. This isn't
  // a perfect test though, since a server can serve a redirect to the same URL
  // that was requested. Also comparing the request and response URLs as strings
  // will fail if the requestURL still has its credentials.
  if (requestURL != response.url() && !isAllowedRedirect(response.url())) {
    m_client = nullptr;
    client->didFailRedirectCheck();
    return;
  }

  handleResponse(identifier, response, nullptr);

  // handleResponse() may detect an error. In such a case (check |m_client| as
  // it gets reset by clear() call), skip the rest.
  //
  // |this| is alive here since loadResourceSynchronously() keeps it alive until
  // the end of the function.
  if (!m_client)
    return;

  RefPtr<const SharedBuffer> data = resource->resourceBuffer();
  if (data)
    handleReceivedData(data->data(), data->size());

  // The client may cancel this loader in handleReceivedData(). In such a case,
  // skip the rest.
  if (!m_client)
    return;

  handleSuccessfulFinish(identifier, 0.0);
}

void DocumentThreadableLoader::loadRequest(
    const ResourceRequest& request,
    ResourceLoaderOptions resourceLoaderOptions) {
  // Any credential should have been removed from the cross-site requests.
  const KURL& requestURL = request.url();
  DCHECK(m_sameOriginRequest || requestURL.user().isEmpty());
  DCHECK(m_sameOriginRequest || requestURL.pass().isEmpty());

  // Update resourceLoaderOptions with enforced values.
  if (m_forceDoNotAllowStoredCredentials)
    resourceLoaderOptions.allowCredentials = DoNotAllowStoredCredentials;
  resourceLoaderOptions.securityOrigin = m_securityOrigin;
  if (m_async)
    loadRequestAsync(request, resourceLoaderOptions);
  else
    loadRequestSync(request, resourceLoaderOptions);
}

bool DocumentThreadableLoader::isAllowedRedirect(const KURL& url) const {
  if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
    return true;

  return m_sameOriginRequest && getSecurityOrigin()->canRequest(url);
}

StoredCredentials DocumentThreadableLoader::effectiveAllowCredentials() const {
  if (m_forceDoNotAllowStoredCredentials)
    return DoNotAllowStoredCredentials;
  return m_resourceLoaderOptions.allowCredentials;
}

const SecurityOrigin* DocumentThreadableLoader::getSecurityOrigin() const {
  return m_securityOrigin ? m_securityOrigin.get()
                          : m_loadingContext->getSecurityOrigin();
}

Document* DocumentThreadableLoader::document() const {
  DCHECK(m_loadingContext);
  return m_loadingContext->getLoadingDocument();
}

DEFINE_TRACE(DocumentThreadableLoader) {
  visitor->trace(m_resource);
  visitor->trace(m_loadingContext);
  ThreadableLoader::trace(visitor);
  RawResourceClient::trace(visitor);
}

}  // namespace blink

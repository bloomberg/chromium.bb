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

#include "config.h"
#include "core/loader/DocumentThreadableLoader.h"

#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/CrossOriginPreflightResultCache.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/Frame.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceRequest.h"
#include "weborigin/SchemeRegistry.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/Assertions.h"

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document* document, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    // The loader will be deleted as soon as this function exits.
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, &client, LoadSynchronously, request, options));
    ASSERT(loader->hasOneRef());
}

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, const ThreadableLoaderOptions& options)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, request, options));
    if (!loader->m_resource)
        loader = 0;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document* document, ThreadableLoaderClient* client, BlockingBehavior blockingBehavior, const ResourceRequest& request, const ThreadableLoaderOptions& options)
    : m_client(client)
    , m_document(document)
    , m_options(options)
    , m_sameOriginRequest(securityOrigin()->canRequest(request.url()))
    , m_simpleRequest(true)
    , m_async(blockingBehavior == LoadAsynchronously)
    , m_timeoutTimer(this, &DocumentThreadableLoader::didTimeout)
{
    ASSERT(document);
    ASSERT(client);
    // Setting an outgoing referer is only supported in the async code path.
    ASSERT(m_async || request.httpReferrer().isEmpty());

    if (m_sameOriginRequest || m_options.crossOriginRequestPolicy == AllowCrossOriginRequests) {
        loadRequest(request, DoSecurityCheck);
        return;
    }

    if (m_options.crossOriginRequestPolicy == DenyCrossOriginRequests) {
        m_client->didFail(ResourceError(errorDomainWebKitInternal, 0, request.url().string(), "Cross origin requests are not supported."));
        return;
    }

    makeCrossOriginAccessRequest(request);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(const ResourceRequest& request)
{
    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);

    OwnPtr<ResourceRequest> crossOriginRequest = adoptPtr(new ResourceRequest(request));

    if ((m_options.preflightPolicy == ConsiderPreflight && isSimpleCrossOriginAccessRequest(crossOriginRequest->httpMethod(), crossOriginRequest->httpHeaderFields())) || m_options.preflightPolicy == PreventPreflight) {
        updateRequestForAccessControl(*crossOriginRequest, securityOrigin(), m_options.allowCredentials);
        makeSimpleCrossOriginAccessRequest(*crossOriginRequest);
    } else {
        m_simpleRequest = false;
        // Do not set the Origin header for preflight requests.
        updateRequestForAccessControl(*crossOriginRequest, 0, m_options.allowCredentials);
        m_actualRequest = crossOriginRequest.release();

        if (CrossOriginPreflightResultCache::shared().canSkipPreflight(securityOrigin()->toString(), m_actualRequest->url(), m_options.allowCredentials, m_actualRequest->httpMethod(), m_actualRequest->httpHeaderFields()))
            preflightSuccess();
        else
            makeCrossOriginAccessRequestWithPreflight(*m_actualRequest);
    }
}

void DocumentThreadableLoader::makeSimpleCrossOriginAccessRequest(const ResourceRequest& request)
{
    ASSERT(m_options.preflightPolicy != ForcePreflight);
    ASSERT(m_options.preflightPolicy == PreventPreflight || isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields()));

    // Cross-origin requests are only allowed for HTTP and registered schemes. We would catch this when checking response headers later, but there is no reason to send a request that's guaranteed to be denied.
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(request.url().protocol())) {
        m_client->didFailAccessControlCheck(ResourceError(errorDomainWebKitInternal, 0, request.url().string(), "Cross origin requests are only supported for HTTP."));
        return;
    }

    loadRequest(request, DoSecurityCheck);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequestWithPreflight(const ResourceRequest& request)
{
    ResourceRequest preflightRequest = createAccessControlPreflightRequest(request, securityOrigin());
    loadRequest(preflightRequest, DoSecurityCheck);
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
    if (m_resource)
        m_resource->removeClient(this);
}

void DocumentThreadableLoader::cancel()
{
    cancelWithError(ResourceError());
}

void DocumentThreadableLoader::cancelWithError(const ResourceError& error)
{
    RefPtr<DocumentThreadableLoader> protect(this);

    // Cancel can re-enter and m_resource might be null here as a result.
    if (m_client && m_resource) {
        ResourceError errorForCallback = error;
        if (errorForCallback.isNull()) {
            // FIXME: This error is sent to the client in didFail(), so it should not be an internal one. Use FrameLoaderClient::cancelledError() instead.
            errorForCallback = ResourceError(errorDomainWebKitInternal, 0, m_resource->url().string(), "Load cancelled");
            errorForCallback.setIsCancellation(true);
        }
        didFail(m_resource->identifier(), errorForCallback);
    }
    clearResource();
    m_client = 0;
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (m_resource)
        m_resource->setDefersLoading(value);
}

void DocumentThreadableLoader::clearResource()
{
    // Script can cancel and restart a request reentrantly within removeClient(),
    // which could lead to calling Resource::removeClient() multiple times for
    // this DocumentThreadableLoader. Save off a copy of m_resource and clear it to
    // prevent the reentrancy.
    if (ResourcePtr<RawResource> resource = m_resource) {
        m_resource = 0;
        resource->removeClient(this);
    }
}

void DocumentThreadableLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<DocumentThreadableLoader> protect(this);
    if (!isAllowedByPolicy(request.url())) {
        m_client->didFailRedirectCheck();
        request = ResourceRequest();
        return;
    }

    // Allow same origin requests to continue after allowing clients to audit the redirect.
    if (isAllowedRedirect(request.url())) {
        if (m_client->isDocumentThreadableLoaderClient())
            static_cast<DocumentThreadableLoaderClient*>(m_client)->willSendRequest(request, redirectResponse);
        return;
    }

    // When using access control, only simple cross origin requests are allowed to redirect. The new request URL must have a supported
    // scheme and not contain the userinfo production. In addition, the redirect response must pass the access control check if the
    // original request was not same-origin.
    if (m_options.crossOriginRequestPolicy == UseAccessControl) {

        InspectorInstrumentation::didReceiveCORSRedirectResponse(m_document->frame(), resource->identifier(), m_document->frame()->loader()->documentLoader(), redirectResponse, 0);

        bool allowRedirect = false;
        String accessControlErrorDescription;

        if (m_simpleRequest) {
            allowRedirect = checkCrossOriginAccessRedirectionUrl(request.url(), accessControlErrorDescription)
                            && (m_sameOriginRequest || passesAccessControlCheck(redirectResponse, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription));
        } else {
            accessControlErrorDescription = "The request was redirected to '"+ request.url().string() + "', which is disallowed for cross-origin requests that require preflight.";
        }

        if (allowRedirect) {
            if (m_resource)
                clearResource();

            RefPtr<SecurityOrigin> originalOrigin = SecurityOrigin::create(redirectResponse.url());
            RefPtr<SecurityOrigin> requestOrigin = SecurityOrigin::create(request.url());
            // If the original request wasn't same-origin, then if the request URL origin is not same origin with the original URL origin,
            // set the source origin to a globally unique identifier. (If the original request was same-origin, the origin of the new request
            // should be the original URL origin.)
            if (!m_sameOriginRequest && !originalOrigin->isSameSchemeHostPort(requestOrigin.get()))
                m_options.securityOrigin = SecurityOrigin::createUnique();
            // Force any subsequent requests to use these checks.
            m_sameOriginRequest = false;

            // Since the request is no longer same-origin, if the user didn't request credentials in
            // the first place, update our state so we neither request them nor expect they must be allowed.
            if (m_options.credentialsRequested == ClientDidNotRequestCredentials)
                m_options.allowCredentials = DoNotAllowStoredCredentials;

            // Remove any headers that may have been added by the network layer that cause access control to fail.
            request.clearHTTPContentType();
            request.clearHTTPReferrer();
            request.clearHTTPOrigin();
            request.clearHTTPUserAgent();
            request.clearHTTPAccept();
            makeCrossOriginAccessRequest(request);
            return;
        }

        ResourceError error(errorDomainWebKitInternal, 0, redirectResponse.url().string(), accessControlErrorDescription);
        m_client->didFailAccessControlCheck(error);
    } else {
        m_client->didFailRedirectCheck();
    }
    request = ResourceRequest();
}

void DocumentThreadableLoader::dataSent(Resource* resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == m_resource);
    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::dataDownloaded(Resource* resource, int dataLength)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == m_resource);
    ASSERT(!m_actualRequest);

    m_client->didDownloadData(dataLength);
}

void DocumentThreadableLoader::responseReceived(Resource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    didReceiveResponse(m_resource->identifier(), response);
}

void DocumentThreadableLoader::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    ASSERT(m_client);

    String accessControlErrorDescription;
    if (m_actualRequest) {
        DocumentLoader* loader = m_document->frame()->loader()->documentLoader();
        InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceResponse(m_document->frame(), identifier, response);
        InspectorInstrumentation::didReceiveResourceResponse(cookie, identifier, loader, response, m_resource ? m_resource->loader() : 0);

        if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
            preflightFailure(identifier, response.url().string(), accessControlErrorDescription);
            return;
        }

        if (!passesPreflightStatusCheck(response, accessControlErrorDescription)) {
            preflightFailure(identifier, response.url().string(), accessControlErrorDescription);
            return;
        }

        OwnPtr<CrossOriginPreflightResultCacheItem> preflightResult = adoptPtr(new CrossOriginPreflightResultCacheItem(m_options.allowCredentials));
        if (!preflightResult->parse(response, accessControlErrorDescription)
            || !preflightResult->allowsCrossOriginMethod(m_actualRequest->httpMethod(), accessControlErrorDescription)
            || !preflightResult->allowsCrossOriginHeaders(m_actualRequest->httpHeaderFields(), accessControlErrorDescription)) {
            preflightFailure(identifier, response.url().string(), accessControlErrorDescription);
            return;
        }

        CrossOriginPreflightResultCache::shared().appendEntry(securityOrigin()->toString(), m_actualRequest->url(), preflightResult.release());
    } else {
        if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == UseAccessControl) {
            if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
                m_client->didFailAccessControlCheck(ResourceError(errorDomainWebKitInternal, 0, response.url().string(), accessControlErrorDescription));
                return;
            }
        }

        m_client->didReceiveResponse(identifier, response);
    }
}

void DocumentThreadableLoader::dataReceived(Resource* resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    didReceiveData(m_resource->identifier(), data, dataLength);
}

void DocumentThreadableLoader::didReceiveData(unsigned long identifier, const char* data, int dataLength)
{
    ASSERT(m_client);

    // Preflight data should be invisible to clients.
    if (m_actualRequest) {
        InspectorInstrumentation::didReceiveData(m_document->frame(), identifier, 0, 0, dataLength);
        return;
    }

    m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::notifyFinished(Resource* resource)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == m_resource);

    m_timeoutTimer.stop();

    if (m_resource->errorOccurred())
        didFail(m_resource->identifier(), m_resource->resourceError());
    else
        didFinishLoading(m_resource->identifier(), m_resource->loadFinishTime());
}

void DocumentThreadableLoader::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (m_actualRequest) {
        InspectorInstrumentation::didFinishLoading(m_document->frame(), identifier, m_document->frame()->loader()->documentLoader(), finishTime);
        ASSERT(!m_sameOriginRequest);
        ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);
        preflightSuccess();
    } else
        m_client->didFinishLoading(identifier, finishTime);
}

void DocumentThreadableLoader::didFail(unsigned long identifier, const ResourceError& error)
{
    if (m_actualRequest)
        InspectorInstrumentation::didFailLoading(m_document->frame(), identifier, m_document->frame()->loader()->documentLoader(), error);

    m_client->didFail(error);
}

void DocumentThreadableLoader::didTimeout(Timer<DocumentThreadableLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timeoutTimer);

    // Using values from net/base/net_error_list.h ERR_TIMED_OUT,
    // Same as existing FIXME above - this error should be coming from FrameLoaderClient to be identifiable.
    static const int timeoutError = -7;
    ResourceError error("net", timeoutError, m_resource->url(), String());
    error.setIsTimeout(true);
    cancelWithError(error);
}

void DocumentThreadableLoader::preflightSuccess()
{
    OwnPtr<ResourceRequest> actualRequest;
    actualRequest.swap(m_actualRequest);

    actualRequest->setHTTPOrigin(securityOrigin()->toString());

    clearResource();

    // It should be ok to skip the security check since we already asked about the preflight request.
    loadRequest(*actualRequest, SkipSecurityCheck);
}

void DocumentThreadableLoader::preflightFailure(unsigned long identifier, const String& url, const String& errorDescription)
{
    ResourceError error(errorDomainWebKitInternal, 0, url, errorDescription);
    if (m_actualRequest)
        InspectorInstrumentation::didFailLoading(m_document->frame(), identifier, m_document->frame()->loader()->documentLoader(), error);
    m_actualRequest = nullptr; // Prevent didFinishLoading() from bypassing access check.
    m_client->didFailAccessControlCheck(error);
}

void DocumentThreadableLoader::loadRequest(const ResourceRequest& request, SecurityCheckPolicy securityCheck)
{
    // Any credential should have been removed from the cross-site requests.
    const KURL& requestURL = request.url();
    m_options.securityCheck = securityCheck;
    ASSERT(m_sameOriginRequest || requestURL.user().isEmpty());
    ASSERT(m_sameOriginRequest || requestURL.pass().isEmpty());

    ThreadableLoaderOptions options = m_options;
    if (m_async) {
        options.crossOriginCredentialPolicy = DoNotAskClientForCrossOriginCredentials;
        if (m_actualRequest) {
            // Don't sniff content or send load callbacks for the preflight request.
            options.sendLoadCallbacks = DoNotSendCallbacks;
            options.sniffContent = DoNotSniffContent;
            // Keep buffering the data for the preflight request.
            options.dataBufferingPolicy = BufferData;
        }

        if (m_options.timeoutMilliseconds > 0)
            m_timeoutTimer.startOneShot(m_options.timeoutMilliseconds / 1000.0);

        FetchRequest newRequest(request, m_options.initiator, options);
        ASSERT(!m_resource);
        m_resource = m_document->fetcher()->fetchRawResource(newRequest);
        if (m_resource) {
            if (m_resource->loader()) {
                unsigned long identifier = m_resource->identifier();
                InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(m_document, identifier, m_client);
            }
            m_resource->addClient(this);
        }
        return;
    }

    // FIXME: ThreadableLoaderOptions.sniffContent is not supported for synchronous requests.
    Vector<char> data;
    ResourceError error;
    ResourceResponse response;
    unsigned long identifier = std::numeric_limits<unsigned long>::max();
    if (Frame* frame = m_document->frame()) {
        if (!m_document->fetcher()->checkInsecureContent(Resource::Raw, requestURL, options.mixedContentBlockingTreatment)) {
            m_client->didFail(error);
            return;
        }
        identifier = m_document->fetcher()->fetchSynchronously(request, m_options.allowCredentials, error, response, data);
    }

    InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(m_document, identifier, m_client);

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (!error.isNull() && !requestURL.isLocalFile() && response.httpStatusCode() <= 0) {
        m_client->didFail(error);
        return;
    }

    // FIXME: FrameLoader::loadSynchronously() does not tell us whether a redirect happened or not, so we guess by comparing the
    // request and response URLs. This isn't a perfect test though, since a server can serve a redirect to the same URL that was
    // requested. Also comparing the request and response URLs as strings will fail if the requestURL still has its credentials.
    if (requestURL != response.url() && (!isAllowedByPolicy(response.url()) || !isAllowedRedirect(response.url()))) {
        m_client->didFailRedirectCheck();
        return;
    }

    didReceiveResponse(identifier, response);

    const char* bytes = static_cast<const char*>(data.data());
    int len = static_cast<int>(data.size());
    didReceiveData(identifier, bytes, len);

    didFinishLoading(identifier, 0.0);
}

bool DocumentThreadableLoader::isAllowedRedirect(const KURL& url) const
{
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        return true;

    return m_sameOriginRequest && securityOrigin()->canRequest(url);
}

bool DocumentThreadableLoader::isAllowedByPolicy(const KURL& url) const
{
    if (m_options.contentSecurityPolicyEnforcement != EnforceConnectSrcDirective)
        return true;
    return m_document->contentSecurityPolicy()->allowConnectToSource(url);
}

SecurityOrigin* DocumentThreadableLoader::securityOrigin() const
{
    return m_options.securityOrigin ? m_options.securityOrigin.get() : m_document->securityOrigin();
}

bool DocumentThreadableLoader::checkCrossOriginAccessRedirectionUrl(const KURL& requestUrl, String& errorDescription)
{
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(requestUrl.protocol())) {
        errorDescription = "The request was redirected to a URL ('" + requestUrl.string() + "') which has a disallowed scheme for cross-origin requests.";
        return false;
    }

    if (!(requestUrl.user().isEmpty() && requestUrl.pass().isEmpty())) {
        errorDescription = "The request was redirected to a URL ('" + requestUrl.string() + "') containing userinfo, which is disallowed for cross-origin requests.";
        return false;
    }

    return true;
}

} // namespace WebCore

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

#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/CrossOriginPreflightResultCache.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/SharedBuffer.h"
#include "platform/Task.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Assertions.h"

namespace blink {

namespace {

class EmptyDataHandle final : public WebDataConsumerHandle {
private:
    class EmptyDataReader final : public WebDataConsumerHandle::Reader {
    public:
        explicit EmptyDataReader(WebDataConsumerHandle::Client* client) : m_factory(this)
        {
            Platform::current()->currentThread()->taskRunner()->postTask(BLINK_FROM_HERE, new Task(bind(&EmptyDataReader::notify, m_factory.createWeakPtr(), client)));
        }
    private:
        Result beginRead(const void** buffer, WebDataConsumerHandle::Flags, size_t *available) override
        {
            *available = 0;
            *buffer = nullptr;
            return Done;
        }
        Result endRead(size_t) override
        {
            return WebDataConsumerHandle::UnexpectedError;
        }
        void notify(WebDataConsumerHandle::Client* client)
        {
            client->didGetReadable();
        }
        WeakPtrFactory<EmptyDataReader> m_factory;
    };

    Reader* obtainReaderInternal(Client* client) override
    {
        return new EmptyDataReader(client);
    }
    const char* debugName() const override { return "EmptyDataHandle"; }
};

} // namespace

// Max number of CORS redirects handled in DocumentThreadableLoader.
// Same number as net/url_request/url_request.cc, and
// same number as https://fetch.spec.whatwg.org/#concept-http-fetch, Step 4.
// FIXME: currently the number of redirects is counted and limited here and in
// net/url_request/url_request.cc separately.
static const int kMaxCORSRedirects = 20;

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
{
    // The loader will be deleted as soon as this function exits.
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, &client, LoadSynchronously, request, options, resourceLoaderOptions));
    ASSERT(loader->hasOneRef());
}

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document& document, ThreadableLoaderClient* client, const ResourceRequest& request, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, request, options, resourceLoaderOptions));
    if (!loader->resource())
        loader = nullptr;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document& document, ThreadableLoaderClient* client, BlockingBehavior blockingBehavior, const ResourceRequest& request, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
    : m_client(client)
    , m_document(&document)
    , m_options(options)
    , m_resourceLoaderOptions(resourceLoaderOptions)
    , m_forceDoNotAllowStoredCredentials(false)
    , m_securityOrigin(m_resourceLoaderOptions.securityOrigin)
    , m_sameOriginRequest(securityOrigin()->canRequestNoSuborigin(request.url()))
    , m_crossOriginNonSimpleRequest(false)
    , m_isUsingDataConsumerHandle(false)
    , m_async(blockingBehavior == LoadAsynchronously)
    , m_requestContext(request.requestContext())
    , m_timeoutTimer(this, &DocumentThreadableLoader::didTimeout)
    , m_requestStartedSeconds(0.0)
    , m_corsRedirectLimit(kMaxCORSRedirects)
    , m_redirectMode(request.fetchRedirectMode())
{
    ASSERT(client);
    // Setting an outgoing referer is only supported in the async code path.
    ASSERT(m_async || request.httpReferrer().isEmpty());

    if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == DenyCrossOriginRequests) {
        ThreadableLoaderClient* client = m_client;
        clear();
        client->didFail(ResourceError(errorDomainBlinkInternal, 0, request.url().string(), "Cross origin requests are not supported."));
        // |this| may be dead here.
        return;
    }

    m_requestStartedSeconds = monotonicallyIncreasingTime();

    // Save any CORS simple headers on the request here. If this request redirects cross-origin, we cancel the old request
    // create a new one, and copy these headers.
    const HTTPHeaderMap& headerMap = request.httpHeaderFields();
    for (const auto& header : headerMap) {
        if (FetchUtils::isSimpleHeader(header.key, header.value)) {
            m_simpleRequestHeaders.add(header.key, header.value);
        } else if (equalIgnoringCase(header.key, HTTPNames::Range) && m_options.crossOriginRequestPolicy == UseAccessControl && m_options.preflightPolicy == PreventPreflight) {
            // Allow an exception for the "range" header for when CORS callers request no preflight, this ensures cross-origin
            // redirects work correctly for crossOrigin enabled WebURLRequest::RequestContextVideo type requests.
            m_simpleRequestHeaders.add(header.key, header.value);
        }
    }

    // DocumentThreadableLoader is used by all javascript initiated fetch, so
    // we use this chance to record non-GET fetch script requests.
    // However, this is based on the following assumptions, so please be careful
    // when adding similar logic:
    // - ThreadableLoader is used as backend for all javascript initiated network
    //   fetches.
    //   - Note that ThreadableLoader is also used for non-network fetch such as
    //     FileReaderLoader. However it emulates GET method so signal is not
    //     recorded here.
    // - ThreadableLoader w/ non-GET request is only created from javascript
    //   initiated fetch.
    //   - Some non-script initiated fetches such as WorkerScriptLoader also use
    //     ThreadableLoader, but they are guaranteed to use GET method.
    if (request.httpMethod() != HTTPNames::GET) {
        if (Page* page = document.page())
            page->chromeClient().didObserveNonGetFetchFromScript();
    }

    // If the fetch request will be handled by the ServiceWorker, the
    // FetchRequestMode of the request must be FetchRequestModeCORS or
    // FetchRequestModeCORSWithForcedPreflight. Otherwise the ServiceWorker can
    // return a opaque response which is from the other origin site and the
    // script in the page can read the content.
    //
    // We assume that ServiceWorker is skipped for sync requests and unsupported
    // protocol requests by content/ code.
    if (m_async && !request.skipServiceWorker() && SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(request.url().protocol()) && document.fetcher()->isControlledByServiceWorker()) {
        ResourceRequest newRequest(request);
        // FetchRequestMode should be set by the caller. But the expected value
        // of FetchRequestMode is not speced yet except for XHR. So we set here.
        // FIXME: When we support fetch API in document, this value should not
        // be overridden here.
        if (options.preflightPolicy == ForcePreflight)
            newRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORSWithForcedPreflight);
        else
            newRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORS);

        m_fallbackRequestForServiceWorker = ResourceRequest(request);
        m_fallbackRequestForServiceWorker.setSkipServiceWorker(true);

        loadRequest(newRequest, m_resourceLoaderOptions);
        return;
    }

    dispatchInitialRequest(request);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::dispatchInitialRequest(const ResourceRequest& request)
{
    if (m_sameOriginRequest || m_options.crossOriginRequestPolicy == AllowCrossOriginRequests) {
        loadRequest(request, m_resourceLoaderOptions);
        return;
    }

    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);

    makeCrossOriginAccessRequest(request);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(const ResourceRequest& request)
{
    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);
    ASSERT(m_client);
    ASSERT(!resource());

    // Cross-origin requests are only allowed certain registered schemes.
    // We would catch this when checking response headers later, but there
    // is no reason to send a request, preflighted or not, that's guaranteed
    // to be denied.
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(request.url().protocol())) {
        ThreadableLoaderClient* client = m_client;
        clear();
        client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, request.url().string(), "Cross origin requests are only supported for protocol schemes: " + SchemeRegistry::listOfCORSEnabledURLSchemes() + "."));
        // |this| may be dead here in async mode.
        return;
    }

    // We use isSimpleOrForbiddenRequest() here since |request| may have been
    // modified in the process of loading (not from the user's input). For
    // example, referrer. We need to accept them. For security, we must reject
    // forbidden headers/methods at the point we accept user's input. Not here.
    if ((m_options.preflightPolicy == ConsiderPreflight && FetchUtils::isSimpleOrForbiddenRequest(request.httpMethod(), request.httpHeaderFields())) || m_options.preflightPolicy == PreventPreflight) {
        ResourceRequest crossOriginRequest(request);
        ResourceLoaderOptions crossOriginOptions(m_resourceLoaderOptions);
        updateRequestForAccessControl(crossOriginRequest, securityOrigin(), effectiveAllowCredentials());
        // We update the credentials mode according to effectiveAllowCredentials() here for backward compatibility. But this is not correct.
        // FIXME: We should set it in the caller of DocumentThreadableLoader.
        crossOriginRequest.setFetchCredentialsMode(effectiveAllowCredentials() == AllowStoredCredentials ? WebURLRequest::FetchCredentialsModeInclude : WebURLRequest::FetchCredentialsModeOmit);
        loadRequest(crossOriginRequest, crossOriginOptions);
    } else {
        m_crossOriginNonSimpleRequest = true;

        ResourceRequest crossOriginRequest(request);
        ResourceLoaderOptions crossOriginOptions(m_resourceLoaderOptions);
        // Do not set the Origin header for preflight requests.
        updateRequestForAccessControl(crossOriginRequest, 0, effectiveAllowCredentials());
        // We update the credentials mode according to effectiveAllowCredentials() here for backward compatibility. But this is not correct.
        // FIXME: We should set it in the caller of DocumentThreadableLoader.
        crossOriginRequest.setFetchCredentialsMode(effectiveAllowCredentials() == AllowStoredCredentials ? WebURLRequest::FetchCredentialsModeInclude : WebURLRequest::FetchCredentialsModeOmit);
        m_actualRequest = crossOriginRequest;
        m_actualOptions = crossOriginOptions;

        bool shouldForcePreflight = InspectorInstrumentation::shouldForceCORSPreflight(m_document);
        bool canSkipPreflight = CrossOriginPreflightResultCache::shared().canSkipPreflight(securityOrigin()->toString(), m_actualRequest.url(), effectiveAllowCredentials(), m_actualRequest.httpMethod(), m_actualRequest.httpHeaderFields());
        if (canSkipPreflight && !shouldForcePreflight) {
            loadActualRequest();
        } else {
            ResourceRequest preflightRequest = createAccessControlPreflightRequest(m_actualRequest, securityOrigin());
            // Create a ResourceLoaderOptions for preflight.
            ResourceLoaderOptions preflightOptions = m_actualOptions;
            preflightOptions.allowCredentials = DoNotAllowStoredCredentials;
            loadRequest(preflightRequest, preflightOptions);
        }
    }
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
    // TODO(oilpan): Remove this once DocumentThreadableLoader is once again a ResourceOwner.
    clearResource();
}

void DocumentThreadableLoader::overrideTimeout(unsigned long timeoutMilliseconds)
{
    ASSERT(m_async);

    // |m_requestStartedSeconds| == 0.0 indicates loading is already finished
    // and |m_timeoutTimer| is already stopped, and thus we do nothing for such
    // cases. See https://crbug.com/551663 for details.
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
        double elapsedTime = monotonicallyIncreasingTime() - m_requestStartedSeconds;
        double nextFire = timeoutMilliseconds / 1000.0;
        double resolvedTime = std::max(nextFire - elapsedTime, 0.0);
        m_timeoutTimer.startOneShot(resolvedTime, BLINK_FROM_HERE);
    }
}

void DocumentThreadableLoader::cancel()
{
    cancelWithError(ResourceError());
    // |this| may be dead here.
}

void DocumentThreadableLoader::cancelWithError(const ResourceError& error)
{
    // Cancel can re-enter and m_resource might be null here as a result.
    if (!m_client || !resource()) {
        clear();
        return;
    }

    ResourceError errorForCallback = error;
    if (errorForCallback.isNull()) {
        // FIXME: This error is sent to the client in didFail(), so it should not be an internal one. Use FrameLoaderClient::cancelledError() instead.
        errorForCallback = ResourceError(errorDomainBlinkInternal, 0, resource()->url().string(), "Load cancelled");
        errorForCallback.setIsCancellation(true);
    }

    ThreadableLoaderClient* client = m_client;
    clear();
    client->didFail(errorForCallback);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (resource())
        resource()->setDefersLoading(value);
}

void DocumentThreadableLoader::clear()
{
    m_client = 0;

    if (!m_async)
        return;

    m_timeoutTimer.stop();
    m_requestStartedSeconds = 0.0;
    clearResource();
}

// In this method, we can clear |request| to tell content::WebURLLoaderImpl of
// Chromium not to follow the redirect. This works only when this method is
// called by RawResource::willSendRequest(). If called by
// RawResource::didAddClient(), clearing |request| won't be propagated
// to content::WebURLLoaderImpl. So, this loader must also get detached from
// the resource by calling clearResource().
void DocumentThreadableLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    if (!m_actualRequest.isNull()) {
        reportResponseReceived(resource->identifier(), redirectResponse);

        handlePreflightFailure(redirectResponse.url().string(), "Response for preflight is invalid (redirect)");
        // |this| may be dead here.

        request = ResourceRequest();

        return;
    }

    if (m_redirectMode == WebURLRequest::FetchRedirectModeManual) {
        // Keep |this| alive even if the client release a reference in
        // responseReceived().
        RefPtr<DocumentThreadableLoader> protect(this);

        // We use |m_redirectMode| to check the original redirect mode.
        // |request| is a new request for redirect. So we don't set the redirect
        // mode of it in WebURLLoaderImpl::Context::OnReceivedRedirect().
        ASSERT(request.useStreamOnResponse());
        // There is no need to read the body of redirect response because there
        // is no way to read the body of opaque-redirect filtered response's
        // internal response.
        // TODO(horo): If we support any API which expose the internal body, we
        // will have to read the body. And also HTTPCache changes will be needed
        // because it doesn't store the body of redirect responses.
        responseReceived(resource, redirectResponse, adoptPtr(new EmptyDataHandle()));

        if (m_client) {
            ASSERT(m_actualRequest.isNull());
            notifyFinished(resource);
        }

        request = ResourceRequest();

        return;
    }

    if (m_redirectMode == WebURLRequest::FetchRedirectModeError || !isAllowedByContentSecurityPolicy(request.url(), ContentSecurityPolicy::DidRedirect)) {
        ThreadableLoaderClient* client = m_client;
        clear();
        client->didFailRedirectCheck();
        // |this| may be dead here.

        request = ResourceRequest();

        return;
    }

    // Allow same origin requests to continue after allowing clients to audit the redirect.
    if (isAllowedRedirect(request.url())) {
        if (m_client->isDocumentThreadableLoaderClient())
            static_cast<DocumentThreadableLoaderClient*>(m_client)->willFollowRedirect(request, redirectResponse);
        return;
    }

    if (m_corsRedirectLimit <= 0) {
        ThreadableLoaderClient* client = m_client;
        clear();
        client->didFailRedirectCheck();
        // |this| may be dead here.
    } else if (m_options.crossOriginRequestPolicy == UseAccessControl) {
        --m_corsRedirectLimit;

        InspectorInstrumentation::didReceiveCORSRedirectResponse(document().frame(), resource->identifier(), document().frame()->loader().documentLoader(), redirectResponse, 0);

        bool allowRedirect = false;
        String accessControlErrorDescription;

        // Non-simple cross origin requests (both preflight and actual one) are
        // not allowed to follow redirect.
        if (m_crossOriginNonSimpleRequest) {
            accessControlErrorDescription = "The request was redirected to '"+ request.url().string() + "', which is disallowed for cross-origin requests that require preflight.";
        } else {
            // The redirect response must pass the access control check if the
            // original request was not same-origin.
            allowRedirect = CrossOriginAccessControl::isLegalRedirectLocation(request.url(), accessControlErrorDescription)
                && (m_sameOriginRequest || passesAccessControlCheck(redirectResponse, effectiveAllowCredentials(), securityOrigin(), accessControlErrorDescription, m_requestContext));
        }

        if (allowRedirect) {
            // FIXME: consider combining this with CORS redirect handling performed by
            // CrossOriginAccessControl::handleRedirect().
            clearResource();

            RefPtr<SecurityOrigin> originalOrigin = SecurityOrigin::create(redirectResponse.url());
            RefPtr<SecurityOrigin> requestOrigin = SecurityOrigin::create(request.url());
            // If the original request wasn't same-origin, then if the request URL origin is not same origin with the original URL origin,
            // set the source origin to a globally unique identifier. (If the original request was same-origin, the origin of the new request
            // should be the original URL origin.)
            if (!m_sameOriginRequest && !originalOrigin->isSameSchemeHostPort(requestOrigin.get()))
                m_securityOrigin = SecurityOrigin::createUnique();
            // Force any subsequent requests to use these checks.
            m_sameOriginRequest = false;

            // Since the request is no longer same-origin, if the user didn't request credentials in
            // the first place, update our state so we neither request them nor expect they must be allowed.
            if (m_resourceLoaderOptions.credentialsRequested == ClientDidNotRequestCredentials)
                m_forceDoNotAllowStoredCredentials = true;

            // Remove any headers that may have been added by the network layer that cause access control to fail.
            request.clearHTTPReferrer();
            request.clearHTTPOrigin();
            request.clearHTTPUserAgent();
            // Add any CORS simple request headers which we previously saved from the original request.
            for (const auto& header : m_simpleRequestHeaders)
                request.setHTTPHeaderField(header.key, header.value);
            makeCrossOriginAccessRequest(request);
            // |this| may be dead here.
            return;
        }

        ThreadableLoaderClient* client = m_client;
        clear();
        client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, redirectResponse.url().string(), accessControlErrorDescription));
        // |this| may be dead here.
    } else {
        ThreadableLoaderClient* client = m_client;
        clear();
        client->didFailRedirectCheck();
        // |this| may be dead here.
    }

    request = ResourceRequest();
}

void DocumentThreadableLoader::dataSent(Resource* resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    m_client->didSendData(bytesSent, totalBytesToBeSent);
    // |this| may be dead here.
}

void DocumentThreadableLoader::dataDownloaded(Resource* resource, int dataLength)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_actualRequest.isNull());
    ASSERT(m_async);

    m_client->didDownloadData(dataLength);
    // |this| may be dead here.
}

void DocumentThreadableLoader::didReceiveResourceTiming(Resource* resource, const ResourceTimingInfo& info)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    m_client->didReceiveResourceTiming(info);
    // |this| may be dead here.
}

void DocumentThreadableLoader::responseReceived(Resource* resource, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    if (handle)
        m_isUsingDataConsumerHandle = true;

    handleResponse(resource->identifier(), response, handle);
    // |this| may be dead here.
}

void DocumentThreadableLoader::handlePreflightResponse(const ResourceResponse& response)
{
    String accessControlErrorDescription;

    if (!passesAccessControlCheck(response, effectiveAllowCredentials(), securityOrigin(), accessControlErrorDescription, m_requestContext)) {
        handlePreflightFailure(response.url().string(), "Response to preflight request doesn't pass access control check: " + accessControlErrorDescription);
        // |this| may be dead here in async mode.
        return;
    }

    if (!passesPreflightStatusCheck(response, accessControlErrorDescription)) {
        handlePreflightFailure(response.url().string(), accessControlErrorDescription);
        // |this| may be dead here in async mode.
        return;
    }

    OwnPtr<CrossOriginPreflightResultCacheItem> preflightResult = adoptPtr(new CrossOriginPreflightResultCacheItem(effectiveAllowCredentials()));
    if (!preflightResult->parse(response, accessControlErrorDescription)
        || !preflightResult->allowsCrossOriginMethod(m_actualRequest.httpMethod(), accessControlErrorDescription)
        || !preflightResult->allowsCrossOriginHeaders(m_actualRequest.httpHeaderFields(), accessControlErrorDescription)) {
        handlePreflightFailure(response.url().string(), accessControlErrorDescription);
        // |this| may be dead here in async mode.
        return;
    }

    CrossOriginPreflightResultCache::shared().appendEntry(securityOrigin()->toString(), m_actualRequest.url(), preflightResult.release());
}

void DocumentThreadableLoader::reportResponseReceived(unsigned long identifier, const ResourceResponse& response)
{
    LocalFrame* frame = document().frame();
    // We are seeing crashes caused by nullptr (crbug.com/578849). But the frame
    // must be set here. TODO(horo): Find the root cause of the unset frame.
    ASSERT(frame);
    if (!frame)
        return;
    DocumentLoader* loader = frame->loader().documentLoader();
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceiveResponse", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveResponseEvent::data(identifier, frame, response));
    InspectorInstrumentation::didReceiveResourceResponse(frame, identifier, loader, response, resource() ? resource()->loader() : 0);
    frame->console().reportResourceResponseReceived(loader, identifier, response);
}

void DocumentThreadableLoader::handleResponse(unsigned long identifier, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    ASSERT(m_client);

    if (!m_actualRequest.isNull()) {
        reportResponseReceived(identifier, response);
        handlePreflightResponse(response);
        // |this| may be dead here in async mode.
        return;
    }

    if (response.wasFetchedViaServiceWorker()) {
        // It's still possible to reach here with null m_fallbackRequestForServiceWorker
        // if the request was for main resource loading (i.e. for SharedWorker), for which
        // we create DocumentLoader before the controller ServiceWorker is set.
        ASSERT(!m_fallbackRequestForServiceWorker.isNull() || m_requestContext == WebURLRequest::RequestContextSharedWorker);
        if (response.wasFallbackRequiredByServiceWorker()) {
            // At this point we must have m_fallbackRequestForServiceWorker.
            // (For SharedWorker the request won't be CORS or CORS-with-preflight,
            // therefore fallback-to-network is handled in the browser process
            // when the ServiceWorker does not call respondWith().)
            ASSERT(!m_fallbackRequestForServiceWorker.isNull());
            reportResponseReceived(identifier, response);
            loadFallbackRequestForServiceWorker();
            // |this| may be dead here in async mode.
            return;
        }
        m_fallbackRequestForServiceWorker = ResourceRequest();
        m_client->didReceiveResponse(identifier, response, handle);
        return;
    }

    // Even if the request met the conditions to get handled by a Service Worker
    // in the constructor of this class (and therefore
    // |m_fallbackRequestForServiceWorker| is set), the Service Worker may skip
    // processing the request. Only if the request is same origin, the skipped
    // response may come here (wasFetchedViaServiceWorker() returns false) since
    // such a request doesn't have to go through the CORS algorithm by calling
    // loadFallbackRequestForServiceWorker().
    // FIXME: We should use |m_sameOriginRequest| when we will support
    // Suborigins (crbug.com/336894) for Service Worker.
    ASSERT(m_fallbackRequestForServiceWorker.isNull() || securityOrigin()->canRequest(m_fallbackRequestForServiceWorker.url()));
    m_fallbackRequestForServiceWorker = ResourceRequest();

    if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == UseAccessControl) {
        String accessControlErrorDescription;
        if (!passesAccessControlCheck(response, effectiveAllowCredentials(), securityOrigin(), accessControlErrorDescription, m_requestContext)) {
            reportResponseReceived(identifier, response);

            ThreadableLoaderClient* client = m_client;
            clear();
            client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, response.url().string(), accessControlErrorDescription));
            // |this| may be dead here.
            return;
        }
    }

    m_client->didReceiveResponse(identifier, response, handle);
}

void DocumentThreadableLoader::setSerializedCachedMetadata(Resource*, const char* data, size_t size)
{
    if (!m_actualRequest.isNull())
        return;
    m_client->didReceiveCachedMetadata(data, size);
    // |this| may be dead here.
}

void DocumentThreadableLoader::dataReceived(Resource* resource, const char* data, size_t dataLength)
{
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    if (m_isUsingDataConsumerHandle)
        return;

    // TODO(junov): Fix the ThreadableLoader ecosystem to use size_t.
    // Until then, we use safeCast to trap potential overflows.
    handleReceivedData(data, safeCast<unsigned>(dataLength));
    // |this| may be dead here.
}

void DocumentThreadableLoader::handleReceivedData(const char* data, size_t dataLength)
{
    ASSERT(m_client);

    // Preflight data should be invisible to clients.
    if (!m_actualRequest.isNull())
        return;

    ASSERT(m_fallbackRequestForServiceWorker.isNull());

    m_client->didReceiveData(data, dataLength);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::notifyFinished(Resource* resource)
{
    ASSERT(m_client);
    ASSERT(resource == this->resource());
    ASSERT(m_async);

    if (resource->errorOccurred()) {
        handleError(resource->resourceError());
        // |this| may be dead here.
    } else {
        handleSuccessfulFinish(resource->identifier(), resource->loadFinishTime());
        // |this| may be dead here.
    }
}

void DocumentThreadableLoader::handleSuccessfulFinish(unsigned long identifier, double finishTime)
{
    ASSERT(m_fallbackRequestForServiceWorker.isNull());

    if (!m_actualRequest.isNull()) {
        // FIXME: Timeout should be applied to whole fetch, not for each of
        // preflight and actual request.
        m_timeoutTimer.stop();
        ASSERT(!m_sameOriginRequest);
        ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);
        loadActualRequest();
        return;
    }

    ThreadableLoaderClient* client = m_client;
    m_client = 0;
    // Don't clear the resource as the client may need to access the downloaded
    // file which will be released when the resource is destoryed.
    if (m_async) {
        m_timeoutTimer.stop();
        m_requestStartedSeconds = 0.0;
    }
    client->didFinishLoading(identifier, finishTime);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::didTimeout(Timer<DocumentThreadableLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timeoutTimer);

    // Using values from net/base/net_error_list.h ERR_TIMED_OUT,
    // Same as existing FIXME above - this error should be coming from FrameLoaderClient to be identifiable.
    static const int timeoutError = -7;
    ResourceError error("net", timeoutError, resource()->url(), String());
    error.setIsTimeout(true);
    cancelWithError(error);
    // |this| may be dead here.
}

void DocumentThreadableLoader::loadFallbackRequestForServiceWorker()
{
    clearResource();
    ResourceRequest fallbackRequest(m_fallbackRequestForServiceWorker);
    m_fallbackRequestForServiceWorker = ResourceRequest();
    dispatchInitialRequest(fallbackRequest);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::loadActualRequest()
{
    ResourceRequest actualRequest = m_actualRequest;
    ResourceLoaderOptions actualOptions = m_actualOptions;
    m_actualRequest = ResourceRequest();
    m_actualOptions = ResourceLoaderOptions();

    actualRequest.setHTTPOrigin(securityOrigin());

    clearResource();

    loadRequest(actualRequest, actualOptions);
}

void DocumentThreadableLoader::handlePreflightFailure(const String& url, const String& errorDescription)
{
    ResourceError error(errorDomainBlinkInternal, 0, url, errorDescription);

    // Prevent handleSuccessfulFinish() from bypassing access check.
    m_actualRequest = ResourceRequest();

    ThreadableLoaderClient* client = m_client;
    clear();
    client->didFailAccessControlCheck(error);
    // |this| may be dead here in async mode.
}

void DocumentThreadableLoader::handleError(const ResourceError& error)
{
    // Copy the ResourceError instance to make it sure that the passed
    // ResourceError is alive during didFail() even when the Resource is
    // destructed during didFail().
    ResourceError copiedError = error;

    ThreadableLoaderClient* client = m_client;
    clear();
    client->didFail(copiedError);
    // |this| may be dead here.
}

void DocumentThreadableLoader::loadRequest(const ResourceRequest& request, ResourceLoaderOptions resourceLoaderOptions)
{
    // Any credential should have been removed from the cross-site requests.
    const KURL& requestURL = request.url();
    ASSERT(m_sameOriginRequest || requestURL.user().isEmpty());
    ASSERT(m_sameOriginRequest || requestURL.pass().isEmpty());

    // Update resourceLoaderOptions with enforced values.
    if (m_forceDoNotAllowStoredCredentials)
        resourceLoaderOptions.allowCredentials = DoNotAllowStoredCredentials;
    resourceLoaderOptions.securityOrigin = m_securityOrigin;
    if (m_async) {
        if (!m_actualRequest.isNull())
            resourceLoaderOptions.dataBufferingPolicy = BufferData;

        if (m_options.timeoutMilliseconds > 0)
            m_timeoutTimer.startOneShot(m_options.timeoutMilliseconds / 1000.0, BLINK_FROM_HERE);

        FetchRequest newRequest(request, m_options.initiator, resourceLoaderOptions);
        if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
            newRequest.setOriginRestriction(FetchRequest::NoOriginRestriction);
        ASSERT(!resource());
        if (request.requestContext() == WebURLRequest::RequestContextVideo || request.requestContext() == WebURLRequest::RequestContextAudio)
            setResource(RawResource::fetchMedia(newRequest, document().fetcher()));
        else if (request.requestContext() == WebURLRequest::RequestContextManifest)
            setResource(RawResource::fetchManifest(newRequest, document().fetcher()));
        else
            setResource(RawResource::fetch(newRequest, document().fetcher()));
        if (resource() && resource()->loader()) {
            unsigned long identifier = resource()->identifier();
            InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(m_document, identifier, m_client);
        }
        return;
    }

    FetchRequest fetchRequest(request, m_options.initiator, resourceLoaderOptions);
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        fetchRequest.setOriginRestriction(FetchRequest::NoOriginRestriction);
    ResourcePtr<Resource> resource = RawResource::fetchSynchronously(fetchRequest, document().fetcher());
    ResourceResponse response = resource ? resource->response() : ResourceResponse();
    unsigned long identifier = resource ? resource->identifier() : std::numeric_limits<unsigned long>::max();
    ResourceError error = resource ? resource->resourceError() : ResourceError();

    InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(m_document, identifier, m_client);

    if (!resource) {
        m_client->didFail(error);
        return;
    }

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (!error.isNull() && !requestURL.isLocalFile() && response.httpStatusCode() <= 0) {
        m_client->didFail(error);
        return;
    }

    // FIXME: A synchronous request does not tell us whether a redirect happened or not, so we guess by comparing the
    // request and response URLs. This isn't a perfect test though, since a server can serve a redirect to the same URL that was
    // requested. Also comparing the request and response URLs as strings will fail if the requestURL still has its credentials.
    if (requestURL != response.url() && (!isAllowedByContentSecurityPolicy(response.url(), ContentSecurityPolicy::DidRedirect) || !isAllowedRedirect(response.url()))) {
        m_client->didFailRedirectCheck();
        return;
    }

    handleResponse(identifier, response, nullptr);

    // handleResponse() may detect an error. In such a case (check |m_client|
    // as it gets reset by clear() call), skip the rest.
    //
    // |this| is alive here since loadResourceSynchronously() keeps it alive
    // until the end of the function.
    if (!m_client)
        return;

    SharedBuffer* data = resource->resourceBuffer();
    if (data)
        handleReceivedData(data->data(), data->size());

    // The client may cancel this loader in handleReceivedData(). In such a
    // case, skip the rest.
    if (!m_client)
        return;

    handleSuccessfulFinish(identifier, 0.0);
}

bool DocumentThreadableLoader::isAllowedRedirect(const KURL& url) const
{
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        return true;

    return m_sameOriginRequest && securityOrigin()->canRequest(url);
}

bool DocumentThreadableLoader::isAllowedByContentSecurityPolicy(const KURL& url, ContentSecurityPolicy::RedirectStatus redirectStatus) const
{
    if (m_options.contentSecurityPolicyEnforcement != EnforceConnectSrcDirective)
        return true;
    return document().contentSecurityPolicy()->allowConnectToSource(url, redirectStatus);
}

StoredCredentials DocumentThreadableLoader::effectiveAllowCredentials() const
{
    if (m_forceDoNotAllowStoredCredentials)
        return DoNotAllowStoredCredentials;
    return m_resourceLoaderOptions.allowCredentials;
}

SecurityOrigin* DocumentThreadableLoader::securityOrigin() const
{
    return m_securityOrigin ? m_securityOrigin.get() : document().securityOrigin();
}

Document& DocumentThreadableLoader::document() const
{
    ASSERT(m_document);
    return *m_document;
}

} // namespace blink

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/loader/FrameFetchContext.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/PingLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/Page.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/Logging.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebFrameScheduler.h"

#include <algorithm>

namespace blink {

FrameFetchContext::FrameFetchContext(DocumentLoader* loader)
    : m_document(nullptr)
    , m_documentLoader(loader)
    , m_imageFetched(false)
{
}

FrameFetchContext::~FrameFetchContext()
{
    m_document = nullptr;
    m_documentLoader = nullptr;
}

LocalFrame* FrameFetchContext::frame() const
{
    LocalFrame* frame = nullptr;
    if (m_documentLoader)
        frame = m_documentLoader->frame();
    else if (m_document && m_document->importsController())
        frame = m_document->importsController()->master()->frame();
    ASSERT(frame);
    return frame;
}

void FrameFetchContext::addAdditionalRequestHeaders(ResourceRequest& request, FetchResourceType type)
{
    bool isMainResource = type == FetchMainResource;
    if (!isMainResource) {
        RefPtr<SecurityOrigin> outgoingOrigin;
        if (!request.didSetHTTPReferrer()) {
            ASSERT(m_document);
            outgoingOrigin = m_document->securityOrigin();
            request.setHTTPReferrer(SecurityPolicy::generateReferrer(m_document->referrerPolicy(), request.url(), m_document->outgoingReferrer()));
        } else {
            RELEASE_ASSERT(SecurityPolicy::generateReferrer(request.referrerPolicy(), request.url(), request.httpReferrer()).referrer == request.httpReferrer());
            outgoingOrigin = SecurityOrigin::createFromString(request.httpReferrer());
        }

        request.addHTTPOriginIfNeeded(outgoingOrigin);
    }

    if (m_document)
        request.setOriginatesFromReservedIPRange(m_document->isHostedInReservedIPRange());

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    if (frame()->settings() && frame()->settings()->dataSaverEnabled())
        request.addHTTPHeaderField("Save-Data", "on");

    frame()->loader().applyUserAgent(request);
}

void FrameFetchContext::setFirstPartyForCookies(ResourceRequest& request)
{
    if (frame()->tree().top()->isLocalFrame())
        request.setFirstPartyForCookies(toLocalFrame(frame()->tree().top())->document()->firstPartyForCookies());
}

CachePolicy FrameFetchContext::cachePolicy() const
{
    if (m_document && m_document->loadEventFinished())
        return CachePolicyVerify;

    FrameLoadType loadType = frame()->loader().loadType();
    if (loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    Frame* parentFrame = frame()->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame()) {
        CachePolicy parentCachePolicy = toLocalFrame(parentFrame)->document()->fetcher()->context().cachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    if (loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    if (m_documentLoader && m_documentLoader->request().cachePolicy() == ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;
    return CachePolicyVerify;

}

static ResourceRequestCachePolicy memoryCachePolicyToResourceRequestCachePolicy(
    const CachePolicy policy) {
    if (policy == CachePolicyVerify)
        return UseProtocolCachePolicy;
    if (policy == CachePolicyRevalidate)
        return ReloadIgnoringCacheData;
    if (policy == CachePolicyReload)
        return ReloadBypassingCache;
    if (policy == CachePolicyHistoryBuffer)
        return ReturnCacheDataElseLoad;
    return UseProtocolCachePolicy;
}

ResourceRequestCachePolicy FrameFetchContext::resourceRequestCachePolicy(const ResourceRequest& request, Resource::Type type) const
{
    ASSERT(frame());
    if (type == Resource::MainResource) {
        FrameLoadType frameLoadType = frame()->loader().loadType();
        if (request.httpMethod() == "POST" && frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataDontLoad;
        if (!frame()->host()->overrideEncoding().isEmpty())
            return ReturnCacheDataElseLoad;
        if (frameLoadType == FrameLoadTypeSame || request.isConditional() || request.httpMethod() == "POST")
            return ReloadIgnoringCacheData;

        for (Frame* f = frame(); f; f = f->tree().parent()) {
            if (!f->isLocalFrame())
                continue;
            frameLoadType = toLocalFrame(f)->loader().loadType();
            if (frameLoadType == FrameLoadTypeBackForward)
                return ReturnCacheDataElseLoad;
            if (frameLoadType == FrameLoadTypeReloadFromOrigin)
                return ReloadBypassingCache;
            if (frameLoadType == FrameLoadTypeReload)
                return ReloadIgnoringCacheData;
        }
        return UseProtocolCachePolicy;
    }

    // For users on slow connections, we want to avoid blocking the parser in
    // the main frame on script loads inserted via document.write, since it can
    // add significant delays before page content is displayed on the screen.
    // For now, as a prototype, we block fetches for main frame scripts
    // inserted via document.write as long as the
    // disallowFetchForDocWrittenScriptsInMainFrame setting is enabled. In the
    // future, we'll extend this logic to only block if estimated network RTT
    // is above some threshold.
    if (type == Resource::Script && isMainFrame()) {
        const bool isInDocumentWrite = m_document && m_document->isInDocumentWrite();
        const bool disallowFetchForDocWriteScripts = frame()->settings() && frame()->settings()->disallowFetchForDocWrittenScriptsInMainFrame();
        if (isInDocumentWrite && disallowFetchForDocWriteScripts)
            return ReturnCacheDataDontLoad;
    }

    if (request.isConditional())
        return ReloadIgnoringCacheData;

    if (m_documentLoader && m_document && !m_document->loadEventFinished()) {
        // For POST requests, we mutate the main resource's cache policy to avoid form resubmission.
        // This policy should not be inherited by subresources.
        ResourceRequestCachePolicy mainResourceCachePolicy = m_documentLoader->request().cachePolicy();
        if (m_documentLoader->request().httpMethod() == "POST") {
            if (mainResourceCachePolicy == ReturnCacheDataDontLoad)
                return ReturnCacheDataElseLoad;
            return UseProtocolCachePolicy;
        }
        return memoryCachePolicyToResourceRequestCachePolicy(cachePolicy());
    }
    return UseProtocolCachePolicy;
}

// FIXME(http://crbug.com/274173):
// |loader| can be null if the resource is loaded from imported document.
// This means inspector, which uses DocumentLoader as an grouping entity,
// cannot see imported documents.
inline DocumentLoader* FrameFetchContext::ensureLoaderForNotifications() const
{
    return m_documentLoader ? m_documentLoader.get() : frame()->loader().documentLoader();
}

void FrameFetchContext::dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    frame()->loader().client()->dispatchDidChangeResourcePriority(identifier, loadPriority, intraPriorityValue);
}

void FrameFetchContext::dispatchWillSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    frame()->loader().applyUserAgent(request);
    frame()->loader().client()->dispatchWillSendRequest(m_documentLoader, identifier, request, redirectResponse);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceSendRequest", TRACE_EVENT_SCOPE_THREAD, "data", InspectorSendRequestEvent::data(identifier, frame(), request));
    InspectorInstrumentation::willSendRequest(frame(), identifier, ensureLoaderForNotifications(), request, redirectResponse, initiatorInfo);
}

void FrameFetchContext::dispatchDidReceiveResponse(unsigned long identifier, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    MixedContentChecker::checkMixedPrivatePublic(frame(), response.remoteIPAddress());
    LinkLoader::loadLinkFromHeader(response.httpHeaderField(HTTPNames::Link), frame()->document(), NetworkHintsInterfaceImpl(), LinkLoader::DoNotLoadResources);
    if (m_documentLoader == frame()->loader().provisionalDocumentLoader()) {
        ResourceFetcher* fetcher = nullptr;
        if (frame()->document())
            fetcher = frame()->document()->fetcher();
        m_documentLoader->clientHintsPreferences().updateFromAcceptClientHintsHeader(response.httpHeaderField(HTTPNames::Accept_CH), fetcher);
    }

    if (response.hasMajorCertificateErrors() && resourceLoader)
        MixedContentChecker::handleCertificateError(frame(), resourceLoader->originalRequest(), response);

    frame()->loader().progress().incrementProgress(identifier, response);
    frame()->loader().client()->dispatchDidReceiveResponse(m_documentLoader, identifier, response);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceiveResponse", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveResponseEvent::data(identifier, frame(), response));
    DocumentLoader* documentLoader = ensureLoaderForNotifications();
    InspectorInstrumentation::didReceiveResourceResponse(frame(), identifier, documentLoader, response, resourceLoader);
    // It is essential that inspector gets resource response BEFORE console.
    frame()->console().reportResourceResponseReceived(documentLoader, identifier, response);
}

void FrameFetchContext::dispatchDidReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    frame()->loader().progress().incrementProgress(identifier, dataLength);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceivedData", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
    InspectorInstrumentation::didReceiveData(frame(), identifier, data, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidDownloadData(unsigned long identifier, int dataLength, int encodedDataLength)
{
    frame()->loader().progress().incrementProgress(identifier, dataLength);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceivedData", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
    InspectorInstrumentation::didReceiveData(frame(), identifier, 0, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidFinishLoading(unsigned long identifier, double finishTime, int64_t encodedDataLength)
{
    frame()->loader().progress().completeProgress(identifier);
    frame()->loader().client()->dispatchDidFinishLoading(m_documentLoader, identifier);

    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(identifier, finishTime, false));
    InspectorInstrumentation::didFinishLoading(frame(), identifier, finishTime, encodedDataLength);
}

void FrameFetchContext::dispatchDidFail(unsigned long identifier, const ResourceError& error, bool isInternalRequest)
{
    frame()->loader().progress().completeProgress(identifier);
    frame()->loader().client()->dispatchDidFinishLoading(m_documentLoader, identifier);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(identifier, 0, true));
    InspectorInstrumentation::didFailLoading(frame(), identifier, error);
    // Notification to FrameConsole should come AFTER InspectorInstrumentation call, DevTools front-end relies on this.
    if (!isInternalRequest)
        frame()->console().didFailLoading(identifier, error);
}


void FrameFetchContext::dispatchDidLoadResourceFromMemoryCache(const Resource* resource)
{
    ResourceRequest request(resource->url());
    unsigned long identifier = createUniqueIdentifier();
    frame()->loader().client()->dispatchDidLoadResourceFromMemoryCache(request, resource->response());
    dispatchWillSendRequest(identifier, request, ResourceResponse(), resource->options().initiatorInfo);

    InspectorInstrumentation::markResourceAsCached(frame(), identifier);
    if (!resource->response().isNull())
        dispatchDidReceiveResponse(identifier, resource->response());

    if (resource->encodedSize() > 0)
        dispatchDidReceiveData(identifier, 0, resource->encodedSize(), 0);

    dispatchDidFinishLoading(identifier, 0, 0);
}

bool FrameFetchContext::shouldLoadNewResource(Resource::Type type) const
{
    if (!m_documentLoader)
        return true;
    if (type == Resource::MainResource)
        return m_documentLoader == frame()->loader().provisionalDocumentLoader();
    return m_documentLoader == frame()->loader().documentLoader();
}

void FrameFetchContext::willStartLoadingResource(ResourceRequest& request)
{
    if (m_documentLoader)
        m_documentLoader->applicationCacheHost()->willStartLoadingResource(request);
}

void FrameFetchContext::didLoadResource(Resource* resource)
{
    if (resource->isLoadEventBlockingResourceType())
        frame()->loader().checkCompleted();
}

void FrameFetchContext::addResourceTiming(const ResourceTimingInfo& info)
{
    Document* initiatorDocument = m_document && info.isMainResource() ? m_document->parentDocument() : m_document.get();
    if (!initiatorDocument || !initiatorDocument->domWindow())
        return;
    DOMWindowPerformance::performance(*initiatorDocument->domWindow())->addResourceTiming(info);
}

bool FrameFetchContext::allowImage(bool imagesEnabled, const KURL& url) const
{
    return frame()->loader().client()->allowImage(imagesEnabled, url);
}

void FrameFetchContext::printAccessDeniedMessage(const KURL& url) const
{
    if (url.isNull())
        return;

    String message;
    if (!m_document || m_document->url().isNull())
        message = "Unsafe attempt to load URL " + url.elidedString() + '.';
    else if (url.isLocalFile() || m_document->url().isLocalFile())
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + m_document->url().elidedString() + ". 'file:' URLs are treated as unique security origins.\n";
    else
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + m_document->url().elidedString() + ". Domains, protocols and ports must match.\n";

    frame()->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
}

bool FrameFetchContext::canRequest(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options, bool forPreload, FetchRequest::OriginRestriction originRestriction) const
{
    // As of CSP2, for requests that are the results of redirects, the match
    // algorithm should ignore the path component of the URL.
    ContentSecurityPolicy::RedirectStatus redirectStatus = resourceRequest.followedRedirect() ? ContentSecurityPolicy::DidRedirect : ContentSecurityPolicy::DidNotRedirect;

    ResourceRequestBlockedReason reason = canRequestInternal(type, resourceRequest, url, options, forPreload, originRestriction, redirectStatus);
    if (reason != ResourceRequestBlockedReasonNone) {
        if (!forPreload)
            InspectorInstrumentation::didBlockRequest(frame(), resourceRequest, ensureLoaderForNotifications(), options.initiatorInfo, reason);
        return false;
    }
    return true;
}

bool FrameFetchContext::allowResponse(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options) const
{
    ResourceRequestBlockedReason reason = canRequestInternal(type, resourceRequest, url, options, false, FetchRequest::UseDefaultOriginRestrictionForType, ContentSecurityPolicy::DidRedirect);
    if (reason != ResourceRequestBlockedReasonNone) {
        InspectorInstrumentation::didBlockRequest(frame(), resourceRequest, ensureLoaderForNotifications(), options.initiatorInfo, reason);
        return false;
    }
    return true;
}

ResourceRequestBlockedReason FrameFetchContext::canRequestInternal(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options, bool forPreload, FetchRequest::OriginRestriction originRestriction, ContentSecurityPolicy::RedirectStatus redirectStatus) const
{
    InstrumentingAgents* agents = InspectorInstrumentation::instrumentingAgentsFor(frame());
    if (agents && agents->inspectorResourceAgent()) {
        if (agents->inspectorResourceAgent()->shouldBlockRequest(resourceRequest))
            return ResourceRequestBlockedReasonInspector;
    }

    SecurityOrigin* securityOrigin = options.securityOrigin.get();
    if (!securityOrigin && m_document)
        securityOrigin = m_document->securityOrigin();

    if (originRestriction != FetchRequest::NoOriginRestriction && securityOrigin && !securityOrigin->canDisplay(url)) {
        if (!forPreload)
            FrameLoader::reportLocalLoadFailed(frame(), url.elidedString());
        WTF_LOG(ResourceLoading, "ResourceFetcher::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return ResourceRequestBlockedReasonOther;
    }

    // Some types of resources can be loaded only from the same origin. Other
    // types of resources, like Images, Scripts, and CSS, can be loaded from
    // any URL.
    switch (type) {
    case Resource::MainResource:
    case Resource::Image:
    case Resource::CSSStyleSheet:
    case Resource::Script:
    case Resource::Font:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkPreload:
    case Resource::LinkSubresource:
    case Resource::TextTrack:
    case Resource::ImportResource:
    case Resource::Media:
    case Resource::Manifest:
        // By default these types of resources can be loaded from any origin.
        // FIXME: Are we sure about Resource::Font?
        if (originRestriction == FetchRequest::RestrictToSameOrigin && !securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return ResourceRequestBlockedReasonOrigin;
        }
        break;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::SVGDocument:
        if (!securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return ResourceRequestBlockedReasonOrigin;
        }
        break;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldCSP = frame()->script().shouldBypassMainWorldCSP() || options.contentSecurityPolicyOption == DoNotCheckContentSecurityPolicy;

    // Don't send CSP messages for preloads, we might never actually display those items.
    ContentSecurityPolicy::ReportingStatus cspReporting = forPreload ?
        ContentSecurityPolicy::SuppressReport : ContentSecurityPolicy::SendReport;

    // m_document can be null, but not in any of the cases where csp is actually used below.
    // ImageResourceTest.MultipartImage crashes w/o the m_document null check.
    // I believe it's the Resource::Raw case.
    const ContentSecurityPolicy* csp = m_document ? m_document->contentSecurityPolicy() : nullptr;

    // TODO(mkwst): This would be cleaner if moved this switch into an allowFromSource()
    // helper on this object which took a Resource::Type, then this block would
    // collapse to about 10 lines for handling Raw and Script special cases.
    switch (type) {
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        ASSERT(ContentSecurityPolicy::isScriptResource(resourceRequest));
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowScriptFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
        break;
    case Resource::Script:
    case Resource::ImportResource:
        ASSERT(ContentSecurityPolicy::isScriptResource(resourceRequest));
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowScriptFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
        ASSERT(frame());
        if (!frame()->loader().client()->allowScriptFromSource(!frame()->settings() || frame()->settings()->scriptEnabled(), url)) {
            frame()->loader().client()->didNotAllowScript();
            return ResourceRequestBlockedReasonCSP;
        }
        break;
    case Resource::CSSStyleSheet:
        ASSERT(ContentSecurityPolicy::isStyleResource(resourceRequest));
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowStyleFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
        break;
    case Resource::SVGDocument:
    case Resource::Image:
        ASSERT(ContentSecurityPolicy::isImageResource(resourceRequest));
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowImageFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
        break;
    case Resource::Font: {
        ASSERT(ContentSecurityPolicy::isFontResource(resourceRequest));
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowFontFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
        break;
    }
    case Resource::LinkPreload:
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowConnectToSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
        break;
    case Resource::MainResource:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkSubresource:
    case Resource::Manifest:
        break;
    case Resource::Media:
    case Resource::TextTrack:
        ASSERT(ContentSecurityPolicy::isMediaResource(resourceRequest));
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowMediaFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;

        if (!frame()->loader().client()->allowMedia(url))
            return ResourceRequestBlockedReasonOther;
        break;
    }

    // SVG Images have unique security rules that prevent all subresource requests
    // except for data urls.
    if (type != Resource::MainResource && frame()->chromeClient().isSVGImageChromeClient() && !url.protocolIsData())
        return ResourceRequestBlockedReasonOrigin;

    // FIXME: Once we use RequestContext for CSP (http://crbug.com/390497), remove this extra check.
    if (resourceRequest.requestContext() == WebURLRequest::RequestContextManifest) {
        ASSERT(csp);
        if (!shouldBypassMainWorldCSP && !csp->allowManifestFromSource(url, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
    }

    // Measure the number of legacy URL schemes ('ftp://') and the number of embedded-credential
    // ('http://user:password@...') resources embedded as subresources. in the hopes that we can
    // block them at some point in the future.
    if (resourceRequest.frameType() != WebURLRequest::FrameTypeTopLevel) {
        ASSERT(frame()->document());
        if (SchemeRegistry::shouldTreatURLSchemeAsLegacy(url.protocol()) && !SchemeRegistry::shouldTreatURLSchemeAsLegacy(frame()->document()->securityOrigin()->protocol()))
            UseCounter::count(frame()->document(), UseCounter::LegacyProtocolEmbeddedAsSubresource);
        if (!url.user().isEmpty() || !url.pass().isEmpty())
            UseCounter::count(frame()->document(), UseCounter::RequestedSubresourceWithEmbeddedCredentials);
    }

    // Measure the number of pages that load resources after a redirect
    // when a CSP is active, to see if implementing CSP
    // 'unsafe-redirect' is feasible.
    if (csp && csp->isActive() && resourceRequest.frameType() != WebURLRequest::FrameTypeTopLevel && resourceRequest.frameType() != WebURLRequest::FrameTypeAuxiliary && redirectStatus == ContentSecurityPolicy::DidRedirect) {
        ASSERT(frame()->document());
        UseCounter::count(frame()->document(), UseCounter::ResourceLoadedAfterRedirectWithCSP);
    }

    // Last of all, check for mixed content. We do this last so that when
    // folks block mixed content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.
    MixedContentChecker::ReportingStatus mixedContentReporting = forPreload ?
        MixedContentChecker::SuppressReport : MixedContentChecker::SendReport;
    if (MixedContentChecker::shouldBlockFetch(frame(), resourceRequest, url, mixedContentReporting))
        return ResourceRequestBlockedReasonMixedContent;

    return ResourceRequestBlockedReasonNone;
}

bool FrameFetchContext::isControlledByServiceWorker() const
{
    ASSERT(m_documentLoader || frame()->loader().documentLoader());
    if (m_documentLoader)
        return frame()->loader().client()->isControlledByServiceWorker(*m_documentLoader);
    // m_documentLoader is null while loading resources from an HTML import.
    // In such cases whether the request is controlled by ServiceWorker or not
    // is determined by the document loader of the frame.
    return frame()->loader().client()->isControlledByServiceWorker(*frame()->loader().documentLoader());
}

int64_t FrameFetchContext::serviceWorkerID() const
{
    ASSERT(m_documentLoader || frame()->loader().documentLoader());
    if (m_documentLoader)
        return frame()->loader().client()->serviceWorkerID(*m_documentLoader);
    // m_documentLoader is null while loading resources from an HTML import.
    // In such cases a service worker ID could be retrieved from the document
    // loader of the frame.
    return frame()->loader().client()->serviceWorkerID(*frame()->loader().documentLoader());
}

bool FrameFetchContext::isMainFrame() const
{
    return frame()->isMainFrame();
}

bool FrameFetchContext::defersLoading() const
{
    return frame()->page()->defersLoading();
}

bool FrameFetchContext::isLoadComplete() const
{
    return m_document && m_document->loadEventFinished();
}

bool FrameFetchContext::pageDismissalEventBeingDispatched() const
{
    return m_document && m_document->pageDismissalEventBeingDispatched() != Document::NoDismissal;
}

bool FrameFetchContext::updateTimingInfoForIFrameNavigation(ResourceTimingInfo* info)
{
    // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
    // FIXME: Resource timing is broken when the parent is a remote frame.
    if (!frame()->deprecatedLocalOwner() || frame()->deprecatedLocalOwner()->loadedNonEmptyDocument())
        return false;
    frame()->deprecatedLocalOwner()->didLoadNonEmptyDocument();
    // Do not report iframe navigation that restored from history, since its
    // location may have been changed after initial navigation.
    if (frame()->loader().loadType() == FrameLoadTypeInitialHistoryLoad)
        return false;
    info->setInitiatorType(frame()->deprecatedLocalOwner()->localName());
    return true;
}

void FrameFetchContext::sendImagePing(const KURL& url)
{
    PingLoader::loadImage(frame(), url);
}

void FrameFetchContext::addConsoleMessage(const String& message) const
{
    if (frame()->document())
        frame()->document()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

SecurityOrigin* FrameFetchContext::securityOrigin() const
{
    return m_document ? m_document->securityOrigin() : nullptr;
}

void FrameFetchContext::upgradeInsecureRequest(FetchRequest& fetchRequest)
{
    KURL url = fetchRequest.resourceRequest().url();

    // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational requests, as described in
    // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
    if (fetchRequest.resourceRequest().frameType() != WebURLRequest::FrameTypeNone)
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("Upgrade-Insecure-Requests", "1");

    if (m_document && m_document->insecureRequestsPolicy() == SecurityContext::InsecureRequestsUpgrade && url.protocolIs("http")) {
        ASSERT(m_document->insecureNavigationsToUpgrade());

        // We always upgrade requests that meet any of the following criteria:
        //
        // 1. Are for subresources (including nested frames).
        // 2. Are form submissions.
        // 3. Whose hosts are contained in the document's InsecureNavigationSet.
        const ResourceRequest& request = fetchRequest.resourceRequest();
        if (request.frameType() == WebURLRequest::FrameTypeNone
            || request.frameType() == WebURLRequest::FrameTypeNested
            || request.requestContext() == WebURLRequest::RequestContextForm
            || (!url.host().isNull() && m_document->insecureNavigationsToUpgrade()->contains(url.host().impl()->hash())))
        {
            UseCounter::count(m_document, UseCounter::UpgradeInsecureRequestsUpgradedRequest);
            url.setProtocol("https");
            if (url.port() == 80)
                url.setPort(443);
            fetchRequest.mutableResourceRequest().setURL(url);
        }
    }
}

void FrameFetchContext::addClientHintsIfNecessary(FetchRequest& fetchRequest)
{
    if (!RuntimeEnabledFeatures::clientHintsEnabled() || !m_document)
        return;

    bool shouldSendDPR = m_document->clientHintsPreferences().shouldSendDPR() || fetchRequest.clientHintsPreferences().shouldSendDPR();
    bool shouldSendResourceWidth = m_document->clientHintsPreferences().shouldSendResourceWidth() || fetchRequest.clientHintsPreferences().shouldSendResourceWidth();
    bool shouldSendViewportWidth = m_document->clientHintsPreferences().shouldSendViewportWidth() || fetchRequest.clientHintsPreferences().shouldSendViewportWidth();

    if (shouldSendDPR)
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("DPR", AtomicString(String::number(m_document->devicePixelRatio())));

    if (shouldSendResourceWidth) {
        FetchRequest::ResourceWidth resourceWidth = fetchRequest.resourceWidth();
        if (resourceWidth.isSet) {
            float physicalWidth = resourceWidth.width * m_document->devicePixelRatio();
            fetchRequest.mutableResourceRequest().addHTTPHeaderField("Width", AtomicString(String::number(ceil(physicalWidth))));
        }
    }

    if (shouldSendViewportWidth && frame()->view())
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("Viewport-Width", AtomicString(String::number(frame()->view()->viewportWidth())));
}

void FrameFetchContext::addCSPHeaderIfNecessary(Resource::Type type, FetchRequest& fetchRequest)
{
    if (!m_document)
        return;

    const ContentSecurityPolicy* csp = m_document->contentSecurityPolicy();
    if (csp->shouldSendCSPHeader(type))
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("CSP", "active");
}

void FrameFetchContext::countClientHintsDPR()
{
    UseCounter::count(frame(), UseCounter::ClientHintsDPR);
}

void FrameFetchContext::countClientHintsResourceWidth()
{
    UseCounter::count(frame(), UseCounter::ClientHintsResourceWidth);
}

void FrameFetchContext::countClientHintsViewportWidth()
{
    UseCounter::count(frame(), UseCounter::ClientHintsViewportWidth);
}

ResourceLoadPriority FrameFetchContext::modifyPriorityForExperiments(ResourceLoadPriority priority, Resource::Type type, const FetchRequest& request, ResourcePriority::VisibilityStatus visibility)
{
    // An image fetch is used to distinguish between "early" and "late" scripts in a document
    if (type == Resource::Image)
        m_imageFetched = true;

    // If Settings is null, we can't verify any experiments are in force.
    if (!frame()->settings())
        return priority;

    // If enabled, drop the priority of all resources in a subframe.
    if (frame()->settings()->lowPriorityIframes() && !frame()->isMainFrame())
        return ResourceLoadPriorityVeryLow;

    // Async/Defer scripts.
    if (type == Resource::Script && FetchRequest::LazyLoad == request.defer())
        return frame()->settings()->fetchIncreaseAsyncScriptPriority() ? ResourceLoadPriorityMedium : ResourceLoadPriorityLow;

    // Runtime experiment that change how we prioritize resources.
    // The toggles do not depend on each other and can be flipped individually
    // though the cumulative result will depend on the interaction between them.
    // Background doc: https://docs.google.com/document/d/1bCDuq9H1ih9iNjgzyAL0gpwNFiEP4TZS-YLRp_RuMlc/edit?usp=sharing

    // Increases the priorities for CSS, Scripts, Fonts and Images all by one level
    // and parser-blocking scripts and visible images by 2.
    // This is used in conjunction with logic on the Chrome side to raise the threshold
    // of "layout-blocking" resources and provide a boost to resources that are needed
    // as soon as possible for something currently on the screen.
    int modifiedPriority = static_cast<int>(priority);
    if (frame()->settings()->fetchIncreasePriorities()) {
        if (type == Resource::CSSStyleSheet || type == Resource::Script || type == Resource::Font || type == Resource::Image)
            modifiedPriority++;
    }

    // Always give visible resources a bump, and an additional bump if generally increasing priorities.
    if (visibility == ResourcePriority::Visible) {
        modifiedPriority++;
        if (frame()->settings()->fetchIncreasePriorities())
            modifiedPriority++;
    }

    if (frame()->settings()->fetchIncreaseFontPriority() && type == Resource::Font)
        modifiedPriority++;

    if (type == Resource::Script) {
        // Reduce the priority of late-body scripts.
        if (frame()->settings()->fetchDeferLateScripts() && request.forPreload() && m_imageFetched)
            modifiedPriority--;
        // Parser-blocking scripts.
        if (frame()->settings()->fetchIncreasePriorities() && !request.forPreload())
            modifiedPriority++;
    }

    // Clamp priority
    modifiedPriority = std::min(static_cast<int>(ResourceLoadPriorityHighest), std::max(static_cast<int>(ResourceLoadPriorityLowest), modifiedPriority));
    return static_cast<ResourceLoadPriority>(modifiedPriority);
}

WebTaskRunner* FrameFetchContext::loadingTaskRunner() const
{
    return frame()->frameScheduler()->loadingTaskRunner();
}

DEFINE_TRACE(FrameFetchContext)
{
    visitor->trace(m_document);
    visitor->trace(m_documentLoader);
    FetchContext::trace(visitor);
}

} // namespace blink

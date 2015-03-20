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

#include "config.h"
#include "core/loader/FrameFetchContext.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/AcceptClientHints.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/PingLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/Page.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/timing/ResourceTimingInfo.h"
#include "platform/Logging.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

FrameFetchContext::FrameFetchContext(DocumentLoader* loader)
    : m_document(nullptr)
    , m_documentLoader(loader)
{
}

FrameFetchContext::~FrameFetchContext()
{
    m_document = nullptr;
    m_documentLoader = nullptr;
}

LocalFrame* FrameFetchContext::frame() const
{
    if (m_documentLoader)
        return m_documentLoader->frame();
    if (m_document && m_document->importsController())
        return m_document->importsController()->master()->frame();
    return 0;
}

void FrameFetchContext::addAdditionalRequestHeaders(ResourceRequest& request, FetchResourceType type)
{
    if (!frame())
        return;

    bool isMainResource = type == FetchMainResource;
    if (!isMainResource) {
        String outgoingOrigin;
        if (!request.didSetHTTPReferrer()) {
            outgoingOrigin = m_document->outgoingOrigin();
            request.setHTTPReferrer(SecurityPolicy::generateReferrer(m_document->referrerPolicy(), request.url(), m_document->outgoingReferrer()));
        } else {
            RELEASE_ASSERT(SecurityPolicy::generateReferrer(request.referrerPolicy(), request.url(), request.httpReferrer()).referrer == request.httpReferrer());
            outgoingOrigin = SecurityOrigin::createFromString(request.httpReferrer())->toString();
        }

        request.addHTTPOriginIfNeeded(AtomicString(outgoingOrigin));
    }

    if (m_document)
        request.setOriginatesFromReservedIPRange(m_document->isHostedInReservedIPRange());

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    frame()->loader().applyUserAgent(request);
}

void FrameFetchContext::setFirstPartyForCookies(ResourceRequest& request)
{
    if (!frame())
        return;

    if (frame()->tree().top()->isLocalFrame())
        request.setFirstPartyForCookies(toLocalFrame(frame()->tree().top())->document()->firstPartyForCookies());
}

CachePolicy FrameFetchContext::cachePolicy() const
{
    if (!frame())
        return CachePolicyVerify;

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
    if (!frame())
        return UseProtocolCachePolicy;

    if (type == Resource::MainResource) {
        FrameLoadType frameLoadType = frame()->loader().loadType();
        if (request.httpMethod() == "POST" && frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataDontLoad;
        if (!frame()->host()->overrideEncoding().isEmpty() || frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataElseLoad;
        if (frameLoadType == FrameLoadTypeReloadFromOrigin)
            return ReloadBypassingCache;
        if (frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeSame || request.isConditional() || request.httpMethod() == "POST")
            return ReloadIgnoringCacheData;
        Frame* parent = frame()->tree().parent();
        if (parent && parent->isLocalFrame())
            return toLocalFrame(parent)->document()->fetcher()->context().resourceRequestCachePolicy(request, type);
        return UseProtocolCachePolicy;
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
inline DocumentLoader* FrameFetchContext::ensureLoaderForNotifications()
{
    return m_documentLoader ? m_documentLoader : frame()->loader().documentLoader();
}

void FrameFetchContext::dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    if (!frame())
        return;

    frame()->loader().client()->dispatchDidChangeResourcePriority(identifier, loadPriority, intraPriorityValue);
}

void FrameFetchContext::dispatchWillSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    if (!frame())
        return;

    frame()->loader().applyUserAgent(request);
    frame()->loader().client()->dispatchWillSendRequest(m_documentLoader, identifier, request, redirectResponse);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceSendRequest", "data", InspectorSendRequestEvent::data(identifier, frame(), request));
    InspectorInstrumentation::willSendRequest(frame(), identifier, ensureLoaderForNotifications(), request, redirectResponse, initiatorInfo);
}

void FrameFetchContext::dispatchDidLoadResourceFromMemoryCache(const ResourceRequest& request, const ResourceResponse& response)
{
    if (!frame())
        return;

    frame()->loader().client()->dispatchDidLoadResourceFromMemoryCache(request, response);
}

void FrameFetchContext::dispatchDidReceiveResponse(unsigned long identifier, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (!frame())
        return;

    MixedContentChecker::checkMixedPrivatePublic(frame(), response.remoteIPAddress());
    LinkLoader::loadLinkFromHeader(response.httpHeaderField("Link"), frame()->document());
    if (m_documentLoader == frame()->loader().provisionalDocumentLoader())
        handleAcceptClientHintsHeader(response.httpHeaderField("accept-ch"), frame());

    frame()->loader().progress().incrementProgress(identifier, response);
    frame()->loader().client()->dispatchDidReceiveResponse(m_documentLoader, identifier, response);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceiveResponse", "data", InspectorReceiveResponseEvent::data(identifier, frame(), response));
    DocumentLoader* documentLoader = ensureLoaderForNotifications();
    InspectorInstrumentation::didReceiveResourceResponse(frame(), identifier, documentLoader, response, resourceLoader);
    // It is essential that inspector gets resource response BEFORE console.
    frame()->console().reportResourceResponseReceived(documentLoader, identifier, response);
}

void FrameFetchContext::dispatchDidReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (!frame())
        return;

    frame()->loader().progress().incrementProgress(identifier, dataLength);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceivedData", "data", InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
    InspectorInstrumentation::didReceiveData(frame(), identifier, data, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidDownloadData(unsigned long identifier, int dataLength, int encodedDataLength)
{
    if (!frame())
        return;

    frame()->loader().progress().incrementProgress(identifier, dataLength);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceivedData", "data", InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
    InspectorInstrumentation::didReceiveData(frame(), identifier, 0, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidFinishLoading(unsigned long identifier, double finishTime, int64_t encodedDataLength)
{
    if (!frame())
        return;

    frame()->loader().progress().completeProgress(identifier);
    frame()->loader().client()->dispatchDidFinishLoading(m_documentLoader, identifier);

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceFinish", "data", InspectorResourceFinishEvent::data(identifier, finishTime, false));
    InspectorInstrumentation::didFinishLoading(frame(), identifier, finishTime, encodedDataLength);
}

void FrameFetchContext::dispatchDidFail(unsigned long identifier, const ResourceError& error, bool isInternalRequest)
{
    if (!frame())
        return;

    frame()->loader().progress().completeProgress(identifier);
    frame()->loader().client()->dispatchDidFinishLoading(m_documentLoader, identifier);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceFinish", "data", InspectorResourceFinishEvent::data(identifier, 0, true));
    InspectorInstrumentation::didFailLoading(frame(), identifier, error);
    // Notification to FrameConsole should come AFTER InspectorInstrumentation call, DevTools front-end relies on this.
    if (!isInternalRequest)
        frame()->console().didFailLoading(identifier, error);
}

void FrameFetchContext::sendRemainingDelegateMessages(unsigned long identifier, const ResourceResponse& response, int dataLength)
{
    if (!frame())
        return;

    InspectorInstrumentation::markResourceAsCached(frame(), identifier);
    if (!response.isNull())
        dispatchDidReceiveResponse(identifier, response);

    if (dataLength > 0)
        dispatchDidReceiveData(identifier, 0, dataLength, 0);

    dispatchDidFinishLoading(identifier, 0, 0);
}

bool FrameFetchContext::shouldLoadNewResource(Resource::Type type) const
{
    if (!frame())
        return false;
    if (!m_documentLoader)
        return true;
    if (type == Resource::MainResource)
        return m_documentLoader == frame()->loader().provisionalDocumentLoader();
    return m_documentLoader == frame()->loader().documentLoader();
}

void FrameFetchContext::dispatchWillRequestResource(FetchRequest* request)
{
    if (frame())
        frame()->loader().client()->dispatchWillRequestResource(request);
}

void FrameFetchContext::willStartLoadingResource(ResourceRequest& request)
{
    if (m_documentLoader)
        m_documentLoader->applicationCacheHost()->willStartLoadingResource(request);
}

void FrameFetchContext::didLoadResource()
{
    if (frame())
        frame()->loader().checkCompleted();
}

void FrameFetchContext::addResourceTiming(ResourceTimingInfo* info, bool isMainResource)
{
    Document* initiatorDocument = m_document && isMainResource ? m_document->parentDocument() : m_document.get();
    if (!frame() || !initiatorDocument || !initiatorDocument->domWindow())
        return;
    DOMWindowPerformance::performance(*initiatorDocument->domWindow())->addResourceTiming(*info, initiatorDocument);
}

bool FrameFetchContext::allowImage(bool imagesEnabled, const KURL& url) const
{
    return !frame() || frame()->loader().client()->allowImage(imagesEnabled, url);
}

void FrameFetchContext::printAccessDeniedMessage(const KURL& url) const
{
    if (url.isNull())
        return;

    String message;
    if (!m_document || m_document->url().isNull())
        message = "Unsafe attempt to load URL " + url.elidedString() + '.';
    else
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + m_document->url().elidedString() + ". Domains, protocols and ports must match.\n";

    frame()->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
}

bool FrameFetchContext::canRequest(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options, bool forPreload, FetchRequest::OriginRestriction originRestriction) const
{
    // FIXME: CachingCorrectnessTest needs us to permit loads when there isn't a frame.
    // Once CachingCorrectnessTest can create a FetchContext mock to meet its needs (without
    // also needing to mock Document, DocumentLoader, and Frame), this should either be reverted to
    // return false or removed entirely.
    if (!frame())
        return true;

    SecurityOrigin* securityOrigin = options.securityOrigin.get();
    if (!securityOrigin && m_document)
        securityOrigin = m_document->securityOrigin();

    if (originRestriction != FetchRequest::NoOriginRestriction && securityOrigin && !securityOrigin->canDisplay(url)) {
        if (!forPreload)
            FrameLoader::reportLocalLoadFailed(frame(), url.elidedString());
        WTF_LOG(ResourceLoading, "ResourceFetcher::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return false;
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
    case Resource::LinkSubresource:
    case Resource::TextTrack:
    case Resource::ImportResource:
    case Resource::Media:
        // By default these types of resources can be loaded from any origin.
        // FIXME: Are we sure about Resource::Font?
        if (originRestriction == FetchRequest::RestrictToSameOrigin && !securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::SVGDocument:
        if (!securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldCSP = frame()->script().shouldBypassMainWorldCSP() || options.contentSecurityPolicyOption == DoNotCheckContentSecurityPolicy;

    // Don't send CSP messages for preloads, we might never actually display those items.
    ContentSecurityPolicy::ReportingStatus cspReporting = forPreload ?
        ContentSecurityPolicy::SuppressReport : ContentSecurityPolicy::SendReport;

    // As of CSP2, for requests that are the results of redirects, the match
    // algorithm should ignore the path component of the URL.
    ContentSecurityPolicy::RedirectStatus redirectStatus = resourceRequest.followedRedirect() ? ContentSecurityPolicy::DidRedirect : ContentSecurityPolicy::DidNotRedirect;

    // m_document can be null, but not in any of the cases where csp is actually used below.
    // ImageResourceTest.MultipartImage crashes w/o the m_document null check.
    // I believe it's the Resource::Raw case.
    const ContentSecurityPolicy* csp = m_document ? m_document->contentSecurityPolicy() : nullptr;

    // FIXME: This would be cleaner if moved this switch into an allowFromSource()
    // helper on this object which took a Resource::Type, then this block would
    // collapse to about 10 lines for handling Raw and Script special cases.
    switch (type) {
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        if (!shouldBypassMainWorldCSP && !csp->allowScriptFromSource(url, redirectStatus, cspReporting))
            return false;
        break;
    case Resource::Script:
    case Resource::ImportResource:
        if (!shouldBypassMainWorldCSP && !csp->allowScriptFromSource(url, redirectStatus, cspReporting))
            return false;

        if (!frame()->loader().client()->allowScriptFromSource(!frame()->settings() || frame()->settings()->scriptEnabled(), url)) {
            frame()->loader().client()->didNotAllowScript();
            return false;
        }
        break;
    case Resource::CSSStyleSheet:
        if (!shouldBypassMainWorldCSP && !csp->allowStyleFromSource(url, redirectStatus, cspReporting))
            return false;
        break;
    case Resource::SVGDocument:
    case Resource::Image:
        if (!shouldBypassMainWorldCSP && !csp->allowImageFromSource(url, redirectStatus, cspReporting))
            return false;
        break;
    case Resource::Font: {
        if (!shouldBypassMainWorldCSP && !csp->allowFontFromSource(url, redirectStatus, cspReporting))
            return false;
        break;
    }
    case Resource::MainResource:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkSubresource:
        break;
    case Resource::Media:
    case Resource::TextTrack:
        if (!shouldBypassMainWorldCSP && !csp->allowMediaFromSource(url, redirectStatus, cspReporting))
            return false;

        if (!frame()->loader().client()->allowMedia(url))
            return false;
        break;
    }

    // SVG Images have unique security rules that prevent all subresource requests
    // except for data urls.
    if (type != Resource::MainResource && frame()->chromeClient().isSVGImageChromeClient() && !url.protocolIsData())
        return false;

    // FIXME: Once we use RequestContext for CSP (http://crbug.com/390497), remove this extra check.
    if (resourceRequest.requestContext() == WebURLRequest::RequestContextManifest) {
        if (!shouldBypassMainWorldCSP && !csp->allowManifestFromSource(url, redirectStatus, cspReporting))
            return false;
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

    // If we're loading the main resource of a subframe, ensure that we check
    // against the parent of the active frame, rather than the frame itself.
    LocalFrame* effectiveFrame = frame();
    if (resourceRequest.frameType() == WebURLRequest::FrameTypeNested) {
        // FIXME: Deal with RemoteFrames.
        Frame* parentFrame = effectiveFrame->tree().parent();
        ASSERT(parentFrame);
        if (parentFrame->isLocalFrame())
            effectiveFrame = toLocalFrame(parentFrame);
    }

    MixedContentChecker::ReportingStatus mixedContentReporting = forPreload ?
        MixedContentChecker::SuppressReport : MixedContentChecker::SendReport;
    return !MixedContentChecker::shouldBlockFetch(effectiveFrame, resourceRequest, url, mixedContentReporting);
}

bool FrameFetchContext::isControlledByServiceWorker() const
{
    if (!frame())
        return false;
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
    if (!frame())
        return -1;
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
    return frame() && frame()->isMainFrame();
}

bool FrameFetchContext::hasSubstituteData() const
{
    return m_documentLoader && m_documentLoader->substituteData().isValid();
}

bool FrameFetchContext::defersLoading() const
{
    return frame() && frame()->page()->defersLoading();
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
    if (!frame() || !frame()->deprecatedLocalOwner() || frame()->deprecatedLocalOwner()->loadedNonEmptyDocument())
        return false;
    info->setInitiatorType(frame()->deprecatedLocalOwner()->localName());
    frame()->deprecatedLocalOwner()->didLoadNonEmptyDocument();
    return true;
}

void FrameFetchContext::sendImagePing(const KURL& url)
{
    if (frame())
        PingLoader::loadImage(frame(), url);
}

void FrameFetchContext::addConsoleMessage(const String& message) const
{
    if (frame() && frame()->document())
        frame()->document()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

ExecutionContext* FrameFetchContext::executionContext() const
{
    return m_document.get();
}

SecurityOrigin* FrameFetchContext::securityOrigin() const
{
    return m_document ? m_document->securityOrigin() : nullptr;
}

String FrameFetchContext::charset() const
{
    return m_document ? m_document->charset().string() : String();
}

void FrameFetchContext::upgradeInsecureRequest(FetchRequest& fetchRequest)
{
    if (!m_document || !RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled())
        return;

    KURL url = fetchRequest.resourceRequest().url();

    // Tack an 'HTTPS' header to outgoing navigational requests, as described in
    // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
    if (fetchRequest.resourceRequest().frameType() != WebURLRequest::FrameTypeNone)
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("HTTPS", "1");

    if (m_document->insecureRequestsPolicy() == SecurityContext::InsecureRequestsUpgrade && url.protocolIs("http")) {
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
            url.setProtocol("https");
            if (url.port() == 80)
                url.setPort(443);
            fetchRequest.mutableResourceRequest().setURL(url);
        }
    }
}

void FrameFetchContext::addClientHintsIfNecessary(FetchRequest& fetchRequest)
{
    if (!frame() || !RuntimeEnabledFeatures::clientHintsEnabled() || !m_document)
        return;

    if (frame()->shouldSendDPRHint())
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("DPR", AtomicString(String::number(m_document->devicePixelRatio())));

    // FIXME: Send the RW hint based on the actual resource width, when we have it.
    if (frame()->shouldSendRWHint() && frame()->view())
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("RW", AtomicString(String::number(frame()->view()->viewportWidth())));
}

DEFINE_TRACE(FrameFetchContext)
{
    visitor->trace(m_document);
    FetchContext::trace(visitor);
}

} // namespace blink

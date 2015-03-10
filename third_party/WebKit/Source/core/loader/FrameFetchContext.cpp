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

#include "core/dom/Document.h"
#include "core/fetch/AcceptClientHints.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
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

void FrameFetchContext::reportLocalLoadFailed(const KURL& url)
{
    FrameLoader::reportLocalLoadFailed(frame(), url.elidedString());
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
    if (documentLoader() == frame()->loader().provisionalDocumentLoader())
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
    InspectorInstrumentation::markResourceAsCached(frame(), identifier);
    if (!response.isNull())
        dispatchDidReceiveResponse(identifier, response);

    if (dataLength > 0)
        dispatchDidReceiveData(identifier, 0, dataLength, 0);

    dispatchDidFinishLoading(identifier, 0, 0);
}

DEFINE_TRACE(FrameFetchContext)
{
    visitor->trace(m_document);
    FetchContext::trace(visitor);
}

} // namespace blink

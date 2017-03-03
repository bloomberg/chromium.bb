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

#include <algorithm>
#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/PingLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/SubresourceFilter.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/private/FrameClientHintsPreferencesContext.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/page/Page.h"
#include "core/paint/FirstMeaningfulPaintDetector.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/WebFrameScheduler.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/network/NetworkUtils.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebViewScheduler.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

void emitWarningForDocWriteScripts(const String& url, Document& document) {
  String message =
      "A Parser-blocking, cross site (i.e. different eTLD+1) script, " + url +
      ", is invoked via document.write. The network request for this script "
      "MAY be blocked by the browser in this or a future page load due to poor "
      "network connectivity. If blocked in this page load, it will be "
      "confirmed in a subsequent console message."
      "See https://www.chromestatus.com/feature/5718547946799104 "
      "for more details.";
  document.addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
  WTFLogAlways("%s", message.utf8().data());
}

bool isConnectionEffectively2G(WebEffectiveConnectionType effectiveType) {
  switch (effectiveType) {
    case WebEffectiveConnectionType::TypeSlow2G:
    case WebEffectiveConnectionType::Type2G:
      return true;
    case WebEffectiveConnectionType::Type3G:
    case WebEffectiveConnectionType::Type4G:
    case WebEffectiveConnectionType::TypeUnknown:
    case WebEffectiveConnectionType::TypeOffline:
      return false;
  }
  NOTREACHED();
  return false;
}

bool shouldDisallowFetchForMainFrameScript(ResourceRequest& request,
                                           FetchRequest::DeferOption defer,
                                           Document& document) {
  // Only scripts inserted via document.write are candidates for having their
  // fetch disallowed.
  if (!document.isInDocumentWrite())
    return false;

  if (!document.settings())
    return false;

  if (!document.frame())
    return false;

  // Only block synchronously loaded (parser blocking) scripts.
  if (defer != FetchRequest::NoDefer)
    return false;

  probe::documentWriteFetchScript(&document);

  if (!request.url().protocolIsInHTTPFamily())
    return false;

  // Avoid blocking same origin scripts, as they may be used to render main
  // page content, whereas cross-origin scripts inserted via document.write
  // are likely to be third party content.
  String requestHost = request.url().host();
  String documentHost = document.getSecurityOrigin()->domain();

  bool sameSite = false;
  if (requestHost == documentHost)
    sameSite = true;

  // If the hosts didn't match, then see if the domains match. For example, if
  // a script is served from static.example.com for a document served from
  // www.example.com, we consider that a first party script and allow it.
  String requestDomain = NetworkUtils::getDomainAndRegistry(
      requestHost, NetworkUtils::IncludePrivateRegistries);
  String documentDomain = NetworkUtils::getDomainAndRegistry(
      documentHost, NetworkUtils::IncludePrivateRegistries);
  // getDomainAndRegistry will return the empty string for domains that are
  // already top-level, such as localhost. Thus we only compare domains if we
  // get non-empty results back from getDomainAndRegistry.
  if (!requestDomain.isEmpty() && !documentDomain.isEmpty() &&
      requestDomain == documentDomain)
    sameSite = true;

  if (sameSite) {
    // This histogram is introduced to help decide whether we should also check
    // same scheme while deciding whether or not to block the script as is done
    // in other cases of "same site" usage. On the other hand we do not want to
    // block more scripts than necessary.
    if (request.url().protocol() != document.getSecurityOrigin()->protocol()) {
      document.loader()->didObserveLoadingBehavior(
          WebLoadingBehaviorFlag::
              WebLoadingBehaviorDocumentWriteBlockDifferentScheme);
    }
    return false;
  }

  emitWarningForDocWriteScripts(request.url().getString(), document);
  request.setHTTPHeaderField("Intervention",
                             "<https://www.chromestatus.com/feature/"
                             "5718547946799104>; level=\"warning\"");

  // Do not block scripts if it is a page reload. This is to enable pages to
  // recover if blocking of a script is leading to a page break and the user
  // reloads the page.
  const FrameLoadType loadType = document.loader()->loadType();
  if (isReloadLoadType(loadType)) {
    // Recording this metric since an increase in number of reloads for pages
    // where a script was blocked could be indicative of a page break.
    document.loader()->didObserveLoadingBehavior(
        WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlockReload);
    return false;
  }

  // Add the metadata that this page has scripts inserted via document.write
  // that are eligible for blocking. Note that if there are multiple scripts
  // the flag will be conveyed to the browser process only once.
  document.loader()->didObserveLoadingBehavior(
      WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlock);

  const bool is2G =
      networkStateNotifier().connectionType() == WebConnectionTypeCellular2G;
  WebEffectiveConnectionType effectiveConnection =
      document.frame()->client()->getEffectiveConnectionType();

  return document.settings()
             ->getDisallowFetchForDocWrittenScriptsInMainFrame() ||
         (document.settings()
              ->getDisallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections() &&
          is2G) ||
         (document.settings()
              ->getDisallowFetchForDocWrittenScriptsInMainFrameIfEffectively2G() &&
          isConnectionEffectively2G(effectiveConnection));
}

}  // namespace

FrameFetchContext::FrameFetchContext(DocumentLoader* loader, Document* document)
    : m_document(document), m_documentLoader(loader) {
  DCHECK(frame());
}

FrameFetchContext::~FrameFetchContext() {
  m_document = nullptr;
  m_documentLoader = nullptr;
}

LocalFrame* FrameFetchContext::frameOfImportsController() const {
  DCHECK(m_document);
  HTMLImportsController* importsController = m_document->importsController();
  DCHECK(importsController);
  LocalFrame* frame = importsController->master()->frame();
  DCHECK(frame);
  return frame;
}

LocalFrame* FrameFetchContext::frame() const {
  if (!m_documentLoader)
    return frameOfImportsController();

  LocalFrame* frame = m_documentLoader->frame();
  DCHECK(frame);
  return frame;
}

LocalFrameClient* FrameFetchContext::localFrameClient() const {
  return frame()->client();
}

void FrameFetchContext::addAdditionalRequestHeaders(ResourceRequest& request,
                                                    FetchResourceType type) {
  bool isMainResource = type == FetchMainResource;
  if (!isMainResource) {
    if (!request.didSetHTTPReferrer()) {
      DCHECK(m_document);
      request.setHTTPReferrer(SecurityPolicy::generateReferrer(
          m_document->getReferrerPolicy(), request.url(),
          m_document->outgoingReferrer()));
      request.addHTTPOriginIfNeeded(m_document->getSecurityOrigin());
    } else {
      DCHECK_EQ(SecurityPolicy::generateReferrer(request.getReferrerPolicy(),
                                                 request.url(),
                                                 request.httpReferrer())
                    .referrer,
                request.httpReferrer());
      request.addHTTPOriginIfNeeded(request.httpReferrer());
    }
  }

  if (m_document) {
    request.setExternalRequestStateFromRequestorAddressSpace(
        m_document->addressSpace());
  }

  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
    return;

  if (masterDocumentLoader()->loadType() == FrameLoadTypeReload)
    request.clearHTTPHeaderField("Save-Data");

  if (frame()->settings() && frame()->settings()->getDataSaverEnabled())
    request.setHTTPHeaderField("Save-Data", "on");
}

CachePolicy FrameFetchContext::getCachePolicy() const {
  if (m_document && m_document->loadEventFinished())
    return CachePolicyVerify;

  FrameLoadType loadType = masterDocumentLoader()->loadType();
  if (loadType == FrameLoadTypeReloadBypassingCache)
    return CachePolicyReload;

  Frame* parentFrame = frame()->tree().parent();
  if (parentFrame && parentFrame->isLocalFrame()) {
    CachePolicy parentCachePolicy = toLocalFrame(parentFrame)
                                        ->document()
                                        ->fetcher()
                                        ->context()
                                        .getCachePolicy();
    if (parentCachePolicy != CachePolicyVerify)
      return parentCachePolicy;
  }

  if (loadType == FrameLoadTypeReload)
    return CachePolicyRevalidate;

  if (m_documentLoader &&
      m_documentLoader->getRequest().getCachePolicy() ==
          WebCachePolicy::ReturnCacheDataElseLoad)
    return CachePolicyHistoryBuffer;

  // Returns CachePolicyVerify for other cases, mainly FrameLoadTypeStandard and
  // FrameLoadTypeReloadMainResource. See public/web/WebFrameLoadType.h to know
  // how these load types work.
  return CachePolicyVerify;
}

static WebCachePolicy memoryCachePolicyToResourceRequestCachePolicy(
    const CachePolicy policy) {
  if (policy == CachePolicyVerify)
    return WebCachePolicy::UseProtocolCachePolicy;
  if (policy == CachePolicyRevalidate)
    return WebCachePolicy::ValidatingCacheData;
  if (policy == CachePolicyReload)
    return WebCachePolicy::BypassingCache;
  if (policy == CachePolicyHistoryBuffer)
    return WebCachePolicy::ReturnCacheDataElseLoad;
  return WebCachePolicy::UseProtocolCachePolicy;
}

static WebCachePolicy frameLoadTypeToWebCachePolicy(FrameLoadType type) {
  if (type == FrameLoadTypeBackForward)
    return WebCachePolicy::ReturnCacheDataElseLoad;
  if (type == FrameLoadTypeReloadBypassingCache)
    return WebCachePolicy::BypassingCache;
  if (type == FrameLoadTypeReload)
    return WebCachePolicy::ValidatingCacheData;
  return WebCachePolicy::UseProtocolCachePolicy;
}

WebCachePolicy FrameFetchContext::resourceRequestCachePolicy(
    ResourceRequest& request,
    Resource::Type type,
    FetchRequest::DeferOption defer) const {
  DCHECK(frame());
  if (type == Resource::MainResource) {
    FrameLoadType frameLoadType = masterDocumentLoader()->loadType();
    if (request.httpMethod() == "POST" &&
        frameLoadType == FrameLoadTypeBackForward)
      return WebCachePolicy::ReturnCacheDataDontLoad;
    if (frameLoadType == FrameLoadTypeReloadMainResource ||
        request.isConditional() || request.httpMethod() == "POST")
      return WebCachePolicy::ValidatingCacheData;

    WebCachePolicy policy = frameLoadTypeToWebCachePolicy(frameLoadType);
    if (policy != WebCachePolicy::UseProtocolCachePolicy)
      return policy;

    for (Frame* f = frame()->tree().parent(); f; f = f->tree().parent()) {
      if (!f->isLocalFrame())
        continue;
      policy = frameLoadTypeToWebCachePolicy(
          toLocalFrame(f)->loader().documentLoader()->loadType());
      if (policy != WebCachePolicy::UseProtocolCachePolicy)
        return policy;
    }
    // Returns UseProtocolCachePolicy for other cases, parent frames not having
    // special kinds of FrameLoadType as they are checked inside the for loop
    // above, or |frameLoadType| being FrameLoadTypeStandard. See
    // public/web/WebFrameLoadType.h to know how these load types work.
    return WebCachePolicy::UseProtocolCachePolicy;
  }

  // For users on slow connections, we want to avoid blocking the parser in
  // the main frame on script loads inserted via document.write, since it can
  // add significant delays before page content is displayed on the screen.
  if (type == Resource::Script && isMainFrame() && m_document &&
      shouldDisallowFetchForMainFrameScript(request, defer, *m_document))
    return WebCachePolicy::ReturnCacheDataDontLoad;

  if (request.isConditional())
    return WebCachePolicy::ValidatingCacheData;

  if (m_documentLoader && m_document && !m_document->loadEventFinished()) {
    // For POST requests, we mutate the main resource's cache policy to avoid
    // form resubmission. This policy should not be inherited by subresources.
    WebCachePolicy mainResourceCachePolicy =
        m_documentLoader->getRequest().getCachePolicy();
    if (m_documentLoader->getRequest().httpMethod() == "POST") {
      if (mainResourceCachePolicy == WebCachePolicy::ReturnCacheDataDontLoad)
        return WebCachePolicy::ReturnCacheDataElseLoad;
      return WebCachePolicy::UseProtocolCachePolicy;
    }
    return memoryCachePolicyToResourceRequestCachePolicy(getCachePolicy());
  }
  return WebCachePolicy::UseProtocolCachePolicy;
}

// The |m_documentLoader| is null in the FrameFetchContext of an imported
// document.
// FIXME(http://crbug.com/274173): This means Inspector, which uses
// DocumentLoader as a grouping entity, cannot see imported documents.
inline DocumentLoader* FrameFetchContext::masterDocumentLoader() const {
  if (m_documentLoader)
    return m_documentLoader.get();

  return frameOfImportsController()->loader().documentLoader();
}

void FrameFetchContext::dispatchDidChangeResourcePriority(
    unsigned long identifier,
    ResourceLoadPriority loadPriority,
    int intraPriorityValue) {
  TRACE_EVENT1(
      "devtools.timeline", "ResourceChangePriority", "data",
      InspectorChangeResourcePriorityEvent::data(identifier, loadPriority));
  probe::didChangeResourcePriority(frame(), identifier, loadPriority);
}

void FrameFetchContext::prepareRequest(ResourceRequest& request) {
  frame()->loader().applyUserAgent(request);
  localFrameClient()->dispatchWillSendRequest(request);
}

void FrameFetchContext::dispatchWillSendRequest(
    unsigned long identifier,
    ResourceRequest& request,
    const ResourceResponse& redirectResponse,
    const FetchInitiatorInfo& initiatorInfo) {
  TRACE_EVENT1("devtools.timeline", "ResourceSendRequest", "data",
               InspectorSendRequestEvent::data(identifier, frame(), request));
  // For initial requests, prepareRequest() is called in
  // willStartLoadingResource(), before revalidation policy is determined. That
  // call doesn't exist for redirects, so call preareRequest() here.
  if (!redirectResponse.isNull()) {
    prepareRequest(request);
  } else {
    frame()->loader().progress().willStartLoading(identifier,
                                                  request.priority());
  }
  probe::willSendRequest(frame(), identifier, masterDocumentLoader(), request,
                         redirectResponse, initiatorInfo);
  if (frame()->frameScheduler())
    frame()->frameScheduler()->didStartLoading(identifier);
}

void FrameFetchContext::dispatchDidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    Resource* resource) {
  dispatchDidReceiveResponseInternal(identifier, response, frameType,
                                     requestContext, resource,
                                     LinkLoader::LoadResourcesAndPreconnect);
}

void FrameFetchContext::dispatchDidReceiveData(unsigned long identifier,
                                               const char* data,
                                               int dataLength) {
  frame()->loader().progress().incrementProgress(identifier, dataLength);
  probe::didReceiveData(frame(), identifier, data, dataLength);
}

void FrameFetchContext::dispatchDidReceiveEncodedData(unsigned long identifier,
                                                      int encodedDataLength) {
  TRACE_EVENT1(
      "devtools.timeline", "ResourceReceivedData", "data",
      InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
  probe::didReceiveEncodedDataLength(frame(), identifier, encodedDataLength);
}

void FrameFetchContext::dispatchDidDownloadData(unsigned long identifier,
                                                int dataLength,
                                                int encodedDataLength) {
  TRACE_EVENT1(
      "devtools.timeline", "ResourceReceivedData", "data",
      InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
  frame()->loader().progress().incrementProgress(identifier, dataLength);
  probe::didReceiveData(frame(), identifier, 0, dataLength);
  probe::didReceiveEncodedDataLength(frame(), identifier, encodedDataLength);
}

void FrameFetchContext::dispatchDidFinishLoading(unsigned long identifier,
                                                 double finishTime,
                                                 int64_t encodedDataLength,
                                                 int64_t decodedBodyLength) {
  TRACE_EVENT1(
      "devtools.timeline", "ResourceFinish", "data",
      InspectorResourceFinishEvent::data(identifier, finishTime, false,
                                         encodedDataLength, decodedBodyLength));
  frame()->loader().progress().completeProgress(identifier);
  probe::didFinishLoading(frame(), identifier, finishTime, encodedDataLength);
  if (frame()->frameScheduler())
    frame()->frameScheduler()->didStopLoading(identifier);
}

void FrameFetchContext::dispatchDidFail(unsigned long identifier,
                                        const ResourceError& error,
                                        int64_t encodedDataLength,
                                        bool isInternalRequest) {
  TRACE_EVENT1("devtools.timeline", "ResourceFinish", "data",
               InspectorResourceFinishEvent::data(identifier, 0, true,
                                                  encodedDataLength, 0));
  frame()->loader().progress().completeProgress(identifier);
  probe::didFailLoading(frame(), identifier, error);
  // Notification to FrameConsole should come AFTER InspectorInstrumentation
  // call, DevTools front-end relies on this.
  if (!isInternalRequest)
    frame()->console().didFailLoading(identifier, error);
  if (frame()->frameScheduler())
    frame()->frameScheduler()->didStopLoading(identifier);
}

void FrameFetchContext::dispatchDidLoadResourceFromMemoryCache(
    unsigned long identifier,
    Resource* resource,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext) {
  ResourceRequest request(resource->url());
  request.setFrameType(frameType);
  request.setRequestContext(requestContext);
  localFrameClient()->dispatchDidLoadResourceFromMemoryCache(
      request, resource->response());
  dispatchWillSendRequest(identifier, request, ResourceResponse(),
                          resource->options().initiatorInfo);

  probe::markResourceAsCached(frame(), identifier);
  if (!resource->response().isNull()) {
    dispatchDidReceiveResponseInternal(identifier, resource->response(),
                                       frameType, requestContext, resource,
                                       LinkLoader::DoNotLoadResources);
  }

  if (resource->encodedSize() > 0)
    dispatchDidReceiveData(identifier, 0, resource->encodedSize());

  dispatchDidFinishLoading(identifier, 0, 0,
                           resource->response().decodedBodyLength());
}

bool FrameFetchContext::shouldLoadNewResource(Resource::Type type) const {
  if (!m_documentLoader)
    return true;

  FrameLoader& loader = m_documentLoader->frame()->loader();
  if (type == Resource::MainResource)
    return m_documentLoader == loader.provisionalDocumentLoader();
  return m_documentLoader == loader.documentLoader();
}

static std::unique_ptr<TracedValue>
loadResourceTraceData(unsigned long identifier, const KURL& url, int priority) {
  String requestId = IdentifiersFactory::requestId(identifier);

  std::unique_ptr<TracedValue> value = TracedValue::create();
  value->setString("requestId", requestId);
  value->setString("url", url.getString());
  value->setInteger("priority", priority);
  return value;
}

void FrameFetchContext::willStartLoadingResource(
    unsigned long identifier,
    ResourceRequest& request,
    Resource::Type type,
    const AtomicString& fetchInitiatorName,
    V8ActivityLoggingPolicy loggingPolicy) {
  TRACE_EVENT_ASYNC_BEGIN1(
      "blink.net", "Resource", identifier, "data",
      loadResourceTraceData(identifier, request.url(), request.priority()));
  prepareRequest(request);

  if (!m_documentLoader || m_documentLoader->fetcher()->archive() ||
      !request.url().isValid())
    return;
  if (type == Resource::MainResource) {
    m_documentLoader->applicationCacheHost()->willStartLoadingMainResource(
        request);
  } else {
    m_documentLoader->applicationCacheHost()->willStartLoadingResource(request);
  }
  if (loggingPolicy == V8ActivityLoggingPolicy::Log) {
    V8DOMActivityLogger* activityLogger = nullptr;
    if (fetchInitiatorName == FetchInitiatorTypeNames::xmlhttprequest) {
      activityLogger = V8DOMActivityLogger::currentActivityLogger();
    } else {
      activityLogger =
          V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    }

    if (activityLogger) {
      Vector<String> argv;
      argv.push_back(Resource::resourceTypeToString(type, fetchInitiatorName));
      argv.push_back(request.url());
      activityLogger->logEvent("blinkRequestResource", argv.size(),
                               argv.data());
    }
  }
}

void FrameFetchContext::didLoadResource(Resource* resource) {
  if (resource->isLoadEventBlockingResourceType())
    frame()->loader().checkCompleted();
  if (m_document)
    FirstMeaningfulPaintDetector::from(*m_document).checkNetworkStable();
}

void FrameFetchContext::addResourceTiming(const ResourceTimingInfo& info) {
  Document* initiatorDocument = m_document && info.isMainResource()
                                    ? m_document->parentDocument()
                                    : m_document.get();
  if (!initiatorDocument || !initiatorDocument->domWindow())
    return;
  DOMWindowPerformance::performance(*initiatorDocument->domWindow())
      ->addResourceTiming(info);
}

bool FrameFetchContext::allowImage(bool imagesEnabled, const KURL& url) const {
  return localFrameClient()->allowImage(imagesEnabled, url);
}

void FrameFetchContext::printAccessDeniedMessage(const KURL& url) const {
  if (url.isNull())
    return;

  String message;
  if (!m_document || m_document->url().isNull()) {
    message = "Unsafe attempt to load URL " + url.elidedString() + '.';
  } else if (url.isLocalFile() || m_document->url().isLocalFile()) {
    message = "Unsafe attempt to load URL " + url.elidedString() +
              " from frame with URL " + m_document->url().elidedString() +
              ". 'file:' URLs are treated as unique security origins.\n";
  } else {
    message = "Unsafe attempt to load URL " + url.elidedString() +
              " from frame with URL " + m_document->url().elidedString() +
              ". Domains, protocols and ports must match.\n";
  }

  frame()->document()->addConsoleMessage(ConsoleMessage::create(
      SecurityMessageSource, ErrorMessageLevel, message));
}

ResourceRequestBlockedReason FrameFetchContext::canRequest(
    Resource::Type type,
    const ResourceRequest& resourceRequest,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reportingPolicy,
    FetchRequest::OriginRestriction originRestriction) const {
  ResourceRequestBlockedReason blockedReason =
      canRequestInternal(type, resourceRequest, url, options, reportingPolicy,
                         originRestriction, resourceRequest.redirectStatus());
  if (blockedReason != ResourceRequestBlockedReason::None &&
      reportingPolicy == SecurityViolationReportingPolicy::Report) {
    probe::didBlockRequest(frame(), resourceRequest, masterDocumentLoader(),
                           options.initiatorInfo, blockedReason);
  }
  return blockedReason;
}

ResourceRequestBlockedReason FrameFetchContext::allowResponse(
    Resource::Type type,
    const ResourceRequest& resourceRequest,
    const KURL& url,
    const ResourceLoaderOptions& options) const {
  ResourceRequestBlockedReason blockedReason =
      canRequestInternal(type, resourceRequest, url, options,
                         SecurityViolationReportingPolicy::Report,
                         FetchRequest::UseDefaultOriginRestrictionForType,
                         RedirectStatus::FollowedRedirect);
  if (blockedReason != ResourceRequestBlockedReason::None) {
    probe::didBlockRequest(frame(), resourceRequest, masterDocumentLoader(),
                           options.initiatorInfo, blockedReason);
  }
  return blockedReason;
}

ResourceRequestBlockedReason FrameFetchContext::canRequestInternal(
    Resource::Type type,
    const ResourceRequest& resourceRequest,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reportingPolicy,
    FetchRequest::OriginRestriction originRestriction,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (probe::shouldBlockRequest(frame(), resourceRequest))
    return ResourceRequestBlockedReason::Inspector;

  SecurityOrigin* securityOrigin = options.securityOrigin.get();
  if (!securityOrigin && m_document)
    securityOrigin = m_document->getSecurityOrigin();

  if (originRestriction != FetchRequest::NoOriginRestriction &&
      securityOrigin && !securityOrigin->canDisplay(url)) {
    if (reportingPolicy == SecurityViolationReportingPolicy::Report)
      FrameLoader::reportLocalLoadFailed(frame(), url.elidedString());
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::requestResource URL was not "
                                 "allowed by SecurityOrigin::canDisplay";
    return ResourceRequestBlockedReason::Other;
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
    case Resource::TextTrack:
    case Resource::ImportResource:
    case Resource::Media:
    case Resource::Manifest:
    case Resource::Mock:
      // By default these types of resources can be loaded from any origin.
      // FIXME: Are we sure about Resource::Font?
      if (originRestriction == FetchRequest::RestrictToSameOrigin &&
          !securityOrigin->canRequest(url)) {
        printAccessDeniedMessage(url);
        return ResourceRequestBlockedReason::Origin;
      }
      break;
    case Resource::XSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::SVGDocument:
      if (!securityOrigin->canRequest(url)) {
        printAccessDeniedMessage(url);
        return ResourceRequestBlockedReason::Origin;
      }
      break;
  }

  // FIXME: Convert this to check the isolated world's Content Security Policy
  // once webkit.org/b/104520 is solved.
  bool shouldBypassMainWorldCSP =
      frame()->script().shouldBypassMainWorldCSP() ||
      options.contentSecurityPolicyOption == DoNotCheckContentSecurityPolicy;

  if (m_document) {
    DCHECK(m_document->contentSecurityPolicy());
    if (!shouldBypassMainWorldCSP &&
        !m_document->contentSecurityPolicy()->allowRequest(
            resourceRequest.requestContext(), url,
            options.contentSecurityPolicyNonce, options.integrityMetadata,
            options.parserDisposition, redirectStatus, reportingPolicy))
      return ResourceRequestBlockedReason::CSP;
  }

  if (type == Resource::Script || type == Resource::ImportResource) {
    DCHECK(frame());
    if (!localFrameClient()->allowScriptFromSource(
            !frame()->settings() || frame()->settings()->getScriptEnabled(),
            url)) {
      localFrameClient()->didNotAllowScript();
      // TODO(estark): Use a different ResourceRequestBlockedReason here, since
      // this check has nothing to do with CSP. https://crbug.com/600795
      return ResourceRequestBlockedReason::CSP;
    }
  } else if (type == Resource::Media || type == Resource::TextTrack) {
    DCHECK(frame());
    if (!localFrameClient()->allowMedia(url))
      return ResourceRequestBlockedReason::Other;
  }

  // SVG Images have unique security rules that prevent all subresource requests
  // except for data urls.
  if (type != Resource::MainResource &&
      frame()->chromeClient().isSVGImageChromeClient() && !url.protocolIsData())
    return ResourceRequestBlockedReason::Origin;

  // Measure the number of legacy URL schemes ('ftp://') and the number of
  // embedded-credential ('http://user:password@...') resources embedded as
  // subresources. in the hopes that we can block them at some point in the
  // future.
  if (resourceRequest.frameType() != WebURLRequest::FrameTypeTopLevel) {
    DCHECK(frame()->document());
    if (SchemeRegistry::shouldTreatURLSchemeAsLegacy(url.protocol()) &&
        !SchemeRegistry::shouldTreatURLSchemeAsLegacy(
            frame()->document()->getSecurityOrigin()->protocol())) {
      Deprecation::countDeprecation(
          frame()->document(), UseCounter::LegacyProtocolEmbeddedAsSubresource);

      // TODO(mkwst): Drop the runtime-enabled check in M59:
      // https://www.chromestatus.com/feature/5709390967472128
      if (RuntimeEnabledFeatures::blockLegacySubresourcesEnabled())
        return ResourceRequestBlockedReason::Origin;
    }
    if (!url.user().isEmpty() || !url.pass().isEmpty()) {
      Deprecation::countDeprecation(
          frame()->document(),
          UseCounter::RequestedSubresourceWithEmbeddedCredentials);
    }
  }

  // Check for mixed content. We do this second-to-last so that when folks block
  // mixed content with a CSP policy, they don't get a warning. They'll still
  // get a warning in the console about CSP blocking the load.
  if (MixedContentChecker::shouldBlockFetch(frame(), resourceRequest, url,
                                            reportingPolicy))
    return ResourceRequestBlockedReason::MixedContent;

  // Let the client have the final say into whether or not the load should
  // proceed.
  DocumentLoader* documentLoader = masterDocumentLoader();
  if (documentLoader && documentLoader->subresourceFilter() &&
      type != Resource::MainResource && type != Resource::ImportResource) {
    if (!documentLoader->subresourceFilter()->allowLoad(
            url, resourceRequest.requestContext(), reportingPolicy)) {
      return ResourceRequestBlockedReason::SubresourceFilter;
    }
  }

  return ResourceRequestBlockedReason::None;
}

bool FrameFetchContext::isControlledByServiceWorker() const {
  DCHECK(m_documentLoader || frame()->loader().documentLoader());

  // Service workers are bypassed by suborigins (see
  // https://w3c.github.io/webappsec-suborigins/). Since service worker
  // controllers are assigned based on physical origin, without knowledge of
  // whether the context is in a suborigin, it is necessary to explicitly bypass
  // service workers on a per-request basis. Additionally, it is necessary to
  // explicitly return |false| here so that it is clear that the SW will be
  // bypassed. In particular, this is important for
  // ResourceFetcher::getCacheIdentifier(), which will return the SW's cache if
  // the context's isControlledByServiceWorker() returns |true|, and thus will
  // returned cached resources from the service worker. That would have the
  // effect of not bypassing the SW.
  if (getSecurityOrigin() && getSecurityOrigin()->hasSuborigin())
    return false;

  if (m_documentLoader) {
    return m_documentLoader->frame()
        ->loader()
        .client()
        ->isControlledByServiceWorker(*m_documentLoader);
  }
  // m_documentLoader is null while loading resources from an HTML import. In
  // such cases whether the request is controlled by ServiceWorker or not is
  // determined by the document loader of the frame.
  return localFrameClient()->isControlledByServiceWorker(
      *frame()->loader().documentLoader());
}

int64_t FrameFetchContext::serviceWorkerID() const {
  DCHECK(m_documentLoader || frame()->loader().documentLoader());
  if (m_documentLoader) {
    return m_documentLoader->frame()->client()->serviceWorkerID(
        *m_documentLoader);
  }
  // m_documentLoader is null while loading resources from an HTML import.
  // In such cases a service worker ID could be retrieved from the document
  // loader of the frame.
  return localFrameClient()->serviceWorkerID(
      *frame()->loader().documentLoader());
}

bool FrameFetchContext::isMainFrame() const {
  return frame()->isMainFrame();
}

bool FrameFetchContext::defersLoading() const {
  return frame()->page()->suspended();
}

bool FrameFetchContext::isLoadComplete() const {
  return m_document && m_document->loadEventFinished();
}

bool FrameFetchContext::pageDismissalEventBeingDispatched() const {
  return m_document &&
         m_document->pageDismissalEventBeingDispatched() !=
             Document::NoDismissal;
}

bool FrameFetchContext::updateTimingInfoForIFrameNavigation(
    ResourceTimingInfo* info) {
  // <iframe>s should report the initial navigation requested by the parent
  // document, but not subsequent navigations.
  // FIXME: Resource timing is broken when the parent is a remote frame.
  if (!frame()->deprecatedLocalOwner() ||
      frame()->deprecatedLocalOwner()->loadedNonEmptyDocument())
    return false;
  frame()->deprecatedLocalOwner()->didLoadNonEmptyDocument();
  // Do not report iframe navigation that restored from history, since its
  // location may have been changed after initial navigation.
  if (masterDocumentLoader()->loadType() == FrameLoadTypeInitialHistoryLoad)
    return false;
  info->setInitiatorType(frame()->deprecatedLocalOwner()->localName());
  return true;
}

void FrameFetchContext::sendImagePing(const KURL& url) {
  PingLoader::loadImage(frame(), url);
}

void FrameFetchContext::addConsoleMessage(const String& message,
                                          LogMessageType messageType) const {
  MessageLevel level = messageType == LogWarningMessage ? WarningMessageLevel
                                                        : ErrorMessageLevel;
  if (frame()->document()) {
    frame()->document()->addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, level, message));
  }
}

SecurityOrigin* FrameFetchContext::getSecurityOrigin() const {
  return m_document ? m_document->getSecurityOrigin() : nullptr;
}

void FrameFetchContext::modifyRequestForCSP(ResourceRequest& resourceRequest) {
  // Record the latest requiredCSP value that will be used when sending this
  // request.
  frame()->loader().recordLatestRequiredCSP();
  frame()->loader().modifyRequestForCSP(resourceRequest, m_document);
}

void FrameFetchContext::addClientHintsIfNecessary(
    const ClientHintsPreferences& hintsPreferences,
    const FetchRequest::ResourceWidth& resourceWidth,
    ResourceRequest& request) {
  if (!RuntimeEnabledFeatures::clientHintsEnabled() || !m_document)
    return;

  bool shouldSendDPR = m_document->clientHintsPreferences().shouldSendDPR() ||
                       hintsPreferences.shouldSendDPR();
  bool shouldSendResourceWidth =
      m_document->clientHintsPreferences().shouldSendResourceWidth() ||
      hintsPreferences.shouldSendResourceWidth();
  bool shouldSendViewportWidth =
      m_document->clientHintsPreferences().shouldSendViewportWidth() ||
      hintsPreferences.shouldSendViewportWidth();

  if (shouldSendDPR) {
    request.addHTTPHeaderField(
        "DPR", AtomicString(String::number(m_document->devicePixelRatio())));
  }

  if (shouldSendResourceWidth) {
    if (resourceWidth.isSet) {
      float physicalWidth =
          resourceWidth.width * m_document->devicePixelRatio();
      request.addHTTPHeaderField(
          "Width", AtomicString(String::number(ceil(physicalWidth))));
    }
  }

  if (shouldSendViewportWidth && frame()->view()) {
    request.addHTTPHeaderField(
        "Viewport-Width",
        AtomicString(String::number(frame()->view()->viewportWidth())));
  }
}

void FrameFetchContext::addCSPHeaderIfNecessary(Resource::Type type,
                                                ResourceRequest& request) {
  if (!m_document)
    return;

  const ContentSecurityPolicy* csp = m_document->contentSecurityPolicy();
  if (csp->shouldSendCSPHeader(type))
    request.addHTTPHeaderField("CSP", "active");
}

void FrameFetchContext::populateResourceRequest(
    Resource::Type type,
    const ClientHintsPreferences& hintsPreferences,
    const FetchRequest::ResourceWidth& resourceWidth,
    ResourceRequest& request) {
  setFirstPartyCookieAndRequestorOrigin(request);
  modifyRequestForCSP(request);
  addClientHintsIfNecessary(hintsPreferences, resourceWidth, request);
  addCSPHeaderIfNecessary(type, request);
}

void FrameFetchContext::setFirstPartyCookieAndRequestorOrigin(
    ResourceRequest& request) {
  if (!m_document)
    return;

  if (request.firstPartyForCookies().isNull()) {
    request.setFirstPartyForCookies(
        m_document ? m_document->firstPartyForCookies()
                   : SecurityOrigin::urlWithUniqueSecurityOrigin());
  }

  // Subresource requests inherit their requestor origin from |m_document|
  // directly. Top-level and nested frame types are taken care of in
  // 'FrameLoadRequest()'. Auxiliary frame types in 'createWindow()' and
  // 'FrameLoader::load'.
  // TODO(mkwst): It would be cleaner to adjust blink::ResourceRequest to
  // initialize itself with a `nullptr` initiator so that this can be a simple
  // `isNull()` check. https://crbug.com/625969
  if (request.frameType() == WebURLRequest::FrameTypeNone &&
      request.requestorOrigin()->isUnique()) {
    request.setRequestorOrigin(m_document->isSandboxed(SandboxOrigin)
                                   ? SecurityOrigin::create(m_document->url())
                                   : m_document->getSecurityOrigin());
  }
}

MHTMLArchive* FrameFetchContext::archive() const {
  DCHECK(!isMainFrame());
  // TODO(nasko): How should this work with OOPIF?
  // The MHTMLArchive is parsed as a whole, but can be constructed from frames
  // in mutliple processes. In that case, which process should parse it and how
  // should the output be spread back across multiple processes?
  if (!frame()->tree().parent()->isLocalFrame())
    return nullptr;
  return toLocalFrame(frame()->tree().parent())
      ->loader()
      .documentLoader()
      ->fetcher()
      ->archive();
}

ResourceLoadPriority FrameFetchContext::modifyPriorityForExperiments(
    ResourceLoadPriority priority) {
  // If Settings is null, we can't verify any experiments are in force.
  if (!frame()->settings())
    return priority;

  // If enabled, drop the priority of all resources in a subframe.
  if (frame()->settings()->getLowPriorityIframes() && !frame()->isMainFrame())
    return ResourceLoadPriorityVeryLow;

  return priority;
}

RefPtr<WebTaskRunner> FrameFetchContext::loadingTaskRunner() const {
  return frame()->frameScheduler()->loadingTaskRunner();
}

void FrameFetchContext::dispatchDidReceiveResponseInternal(
    unsigned long identifier,
    const ResourceResponse& response,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    Resource* resource,
    LinkLoader::CanLoadResources resourceLoadingPolicy) {
  TRACE_EVENT1(
      "devtools.timeline", "ResourceReceiveResponse", "data",
      InspectorReceiveResponseEvent::data(identifier, frame(), response));
  MixedContentChecker::checkMixedPrivatePublic(frame(),
                                               response.remoteIPAddress());
  if (m_documentLoader &&
      m_documentLoader ==
          m_documentLoader->frame()->loader().provisionalDocumentLoader()) {
    FrameClientHintsPreferencesContext hintsContext(frame());
    m_documentLoader->clientHintsPreferences()
        .updateFromAcceptClientHintsHeader(
            response.httpHeaderField(HTTPNames::Accept_CH), &hintsContext);
    // When response is received with a provisional docloader, the resource
    // haven't committed yet, and we cannot load resources, only preconnect.
    resourceLoadingPolicy = LinkLoader::DoNotLoadResources;
  }
  LinkLoader::loadLinksFromHeader(
      response.httpHeaderField(HTTPNames::Link), response.url(),
      frame()->document(), NetworkHintsInterfaceImpl(), resourceLoadingPolicy,
      LinkLoader::LoadAll, nullptr);

  if (response.hasMajorCertificateErrors()) {
    MixedContentChecker::handleCertificateError(frame(), response, frameType,
                                                requestContext);
  }

  frame()->loader().progress().incrementProgress(identifier, response);
  localFrameClient()->dispatchDidReceiveResponse(response);
  DocumentLoader* documentLoader = masterDocumentLoader();
  probe::didReceiveResourceResponse(frame(), identifier, documentLoader,
                                    response, resource);
  // It is essential that inspector gets resource response BEFORE console.
  frame()->console().reportResourceResponseReceived(documentLoader, identifier,
                                                    response);
}

DEFINE_TRACE(FrameFetchContext) {
  visitor->trace(m_document);
  visitor->trace(m_documentLoader);
  FetchContext::trace(visitor);
}

}  // namespace blink

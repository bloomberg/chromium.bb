/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/loader/DocumentLoader.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/WeakIdentifierMap.h"
#include "core/events/Event.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/FrameFetchContext.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/SubresourceFilter.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/FontResource.h"
#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "platform/HTTPNames.h"
#include "platform/UserGestureIndicator.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/plugins/PluginData.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "wtf/Assertions.h"
#include "wtf/AutoReset.h"
#include "wtf/text/WTFString.h"

namespace blink {

static bool isArchiveMIMEType(const String& mimeType) {
  return equalIgnoringCase("multipart/related", mimeType);
}

static bool shouldInheritSecurityOriginFromOwner(const KURL& url) {
  // https://html.spec.whatwg.org/multipage/browsers.html#origin
  //
  // If a Document is the initial "about:blank" document The origin and
  // effective script origin of the Document are those it was assigned when its
  // browsing context was created.
  //
  // Note: We generalize this to all "blank" URLs and invalid URLs because we
  // treat all of these URLs as about:blank.
  return url.isEmpty() || url.protocolIsAbout();
}

DocumentLoader::DocumentLoader(LocalFrame* frame,
                               const ResourceRequest& req,
                               const SubstituteData& substituteData,
                               ClientRedirectPolicy clientRedirectPolicy)
    : m_frame(frame),
      m_fetcher(FrameFetchContext::createFetcherFromDocumentLoader(this)),
      m_originalRequest(req),
      m_substituteData(substituteData),
      m_request(req),
      m_loadType(FrameLoadTypeStandard),
      m_isClientRedirect(clientRedirectPolicy ==
                         ClientRedirectPolicy::ClientRedirect),
      m_replacesCurrentHistoryItem(false),
      m_dataReceived(false),
      m_navigationType(NavigationTypeOther),
      m_documentLoadTiming(*this),
      m_timeOfLastDataReceived(0.0),
      m_applicationCacheHost(ApplicationCacheHost::create(this)),
      m_wasBlockedAfterCSP(false),
      m_state(NotStarted),
      m_inDataReceived(false),
      m_dataBuffer(SharedBuffer::create()) {
  DCHECK(m_frame);

  // The document URL needs to be added to the head of the list as that is
  // where the redirects originated.
  if (m_isClientRedirect)
    appendRedirect(m_frame->document()->url());
}

FrameLoader& DocumentLoader::frameLoader() const {
  DCHECK(m_frame);
  return m_frame->loader();
}

LocalFrameClient& DocumentLoader::localFrameClient() const {
  DCHECK(m_frame);
  LocalFrameClient* client = m_frame->client();
  // LocalFrame clears its |m_client| only after detaching all DocumentLoaders
  // (i.e. calls detachFromFrame() which clears |m_frame|) owned by the
  // LocalFrame's FrameLoader. So, if |m_frame| is non nullptr, |client| is
  // also non nullptr.
  DCHECK(client);
  return *client;
}

DocumentLoader::~DocumentLoader() {
  DCHECK(!m_frame);
  DCHECK(!m_mainResource);
  DCHECK(!m_applicationCacheHost);
  DCHECK_EQ(m_state, SentDidFinishLoad);
}

DEFINE_TRACE(DocumentLoader) {
  visitor->trace(m_frame);
  visitor->trace(m_fetcher);
  visitor->trace(m_mainResource);
  visitor->trace(m_writer);
  visitor->trace(m_subresourceFilter);
  visitor->trace(m_documentLoadTiming);
  visitor->trace(m_applicationCacheHost);
  visitor->trace(m_contentSecurityPolicy);
  RawResourceClient::trace(visitor);
}

unsigned long DocumentLoader::mainResourceIdentifier() const {
  return m_mainResource ? m_mainResource->identifier() : 0;
}

ResourceTimingInfo* DocumentLoader::getNavigationTimingInfo() const {
  DCHECK(fetcher());
  return fetcher()->getNavigationTimingInfo();
}

const ResourceRequest& DocumentLoader::originalRequest() const {
  return m_originalRequest;
}

const ResourceRequest& DocumentLoader::getRequest() const {
  return m_request;
}

void DocumentLoader::setSubresourceFilter(
    SubresourceFilter* subresourceFilter) {
  m_subresourceFilter = subresourceFilter;
}

const KURL& DocumentLoader::url() const {
  return m_request.url();
}

Resource* DocumentLoader::startPreload(Resource::Type type,
                                       FetchRequest& request) {
  Resource* resource = nullptr;
  switch (type) {
    case Resource::Image:
      if (m_frame && m_frame->settings() &&
          m_frame->settings()->getFetchImagePlaceholders()) {
        request.setAllowImagePlaceholder();
      }
      resource = ImageResource::fetch(request, fetcher());
      break;
    case Resource::Script:
      resource = ScriptResource::fetch(request, fetcher());
      break;
    case Resource::CSSStyleSheet:
      resource = CSSStyleSheetResource::fetch(request, fetcher());
      break;
    case Resource::Font:
      resource = FontResource::fetch(request, fetcher());
      break;
    case Resource::Media:
      resource = RawResource::fetchMedia(request, fetcher());
      break;
    case Resource::TextTrack:
      resource = RawResource::fetchTextTrack(request, fetcher());
      break;
    case Resource::ImportResource:
      resource = RawResource::fetchImport(request, fetcher());
      break;
    case Resource::Raw:
      resource = RawResource::fetch(request, fetcher());
      break;
    default:
      NOTREACHED();
  }

  // CSP layout tests verify that preloads are subject to access checks by
  // seeing if they are in the `preload started` list. Therefore do not add
  // them to the list if the load is immediately denied.
  if (resource && !resource->resourceError().isAccessCheck())
    fetcher()->preloadStarted(resource);
  return resource;
}

void DocumentLoader::setServiceWorkerNetworkProvider(
    std::unique_ptr<WebServiceWorkerNetworkProvider> provider) {
  m_serviceWorkerNetworkProvider = std::move(provider);
}

void DocumentLoader::setSourceLocation(
    std::unique_ptr<SourceLocation> sourceLocation) {
  m_sourceLocation = std::move(sourceLocation);
}

std::unique_ptr<SourceLocation> DocumentLoader::copySourceLocation() const {
  return m_sourceLocation ? m_sourceLocation->clone() : nullptr;
}

void DocumentLoader::dispatchLinkHeaderPreloads(
    ViewportDescriptionWrapper* viewport,
    LinkLoader::MediaPreloadPolicy mediaPolicy) {
  LinkLoader::loadLinksFromHeader(
      response().httpHeaderField(HTTPNames::Link), response().url(),
      m_frame->document(), NetworkHintsInterfaceImpl(),
      LinkLoader::OnlyLoadResources, mediaPolicy, viewport);
}

void DocumentLoader::didChangePerformanceTiming() {
  if (m_frame && m_state >= Committed) {
    localFrameClient().didChangePerformanceTiming();
  }
}

void DocumentLoader::didObserveLoadingBehavior(
    WebLoadingBehaviorFlag behavior) {
  if (m_frame) {
    DCHECK_GE(m_state, Committed);
    localFrameClient().didObserveLoadingBehavior(behavior);
  }
}

void DocumentLoader::updateForSameDocumentNavigation(
    const KURL& newURL,
    SameDocumentNavigationSource sameDocumentNavigationSource) {
  KURL oldURL = m_request.url();
  m_originalRequest.setURL(newURL);
  m_request.setURL(newURL);
  if (sameDocumentNavigationSource == SameDocumentNavigationHistoryApi) {
    m_request.setHTTPMethod(HTTPNames::GET);
    m_request.setHTTPBody(nullptr);
  }
  clearRedirectChain();
  if (m_isClientRedirect)
    appendRedirect(oldURL);
  appendRedirect(newURL);
}

const KURL& DocumentLoader::urlForHistory() const {
  return unreachableURL().isEmpty() ? url() : unreachableURL();
}

void DocumentLoader::commitIfReady() {
  if (m_state < Committed) {
    m_state = Committed;
    frameLoader().commitProvisionalLoad();
  }
}

void DocumentLoader::notifyFinished(Resource* resource) {
  DCHECK_EQ(m_mainResource, resource);
  DCHECK(m_mainResource);

  if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
    finishedLoading(m_mainResource->loadFinishTime());
    return;
  }

  if (m_applicationCacheHost)
    m_applicationCacheHost->failedLoadingMainResource();

  if (m_mainResource->resourceError().wasBlockedByResponse()) {
    probe::canceledAfterReceivedResourceResponse(
        m_frame, this, mainResourceIdentifier(), resource->response(),
        m_mainResource.get());
  }

  frameLoader().loadFailed(this, m_mainResource->resourceError());
  clearMainResourceHandle();
}

void DocumentLoader::finishedLoading(double finishTime) {
  DCHECK(m_frame->loader().stateMachine()->creatingInitialEmptyDocument() ||
         !m_frame->page()->suspended() ||
         MainThreadDebugger::instance()->isPaused());

  double responseEndTime = finishTime;
  if (!responseEndTime)
    responseEndTime = m_timeOfLastDataReceived;
  if (!responseEndTime)
    responseEndTime = monotonicallyIncreasingTime();
  timing().setResponseEnd(responseEndTime);

  commitIfReady();
  if (!m_frame)
    return;

  if (!maybeCreateArchive()) {
    // If this is an empty document, it will not have actually been created yet.
    // Commit dummy data so that DocumentWriter::begin() gets called and creates
    // the Document.
    if (!m_writer)
      commitData(0, 0);
  }

  if (!m_frame)
    return;

  m_applicationCacheHost->finishedLoadingMainResource();
  endWriting();
  clearMainResourceHandle();

  // Shows the deprecation message and measures the impact of the new security
  // restriction which disallows responding to navigation requests with
  // redirected responses in the service worker.
  // TODO(horo): Remove this when we actually introduce the restriction in
  // RespondWithObserver.
  if (m_response.wasFetchedViaServiceWorker() &&
      m_response.urlListViaServiceWorker().size() > 1) {
    Deprecation::countDeprecation(
        m_frame,
        UseCounter::
            ServiceWorkerRespondToNavigationRequestWithRedirectedResponse);
  }
}

bool DocumentLoader::redirectReceived(
    Resource* resource,
    const ResourceRequest& request,
    const ResourceResponse& redirectResponse) {
  DCHECK(m_frame);
  DCHECK_EQ(resource, m_mainResource);
  DCHECK(!redirectResponse.isNull());
  m_request = request;

  // If the redirecting url is not allowed to display content from the target
  // origin, then block the redirect.
  const KURL& requestURL = m_request.url();
  RefPtr<SecurityOrigin> redirectingOrigin =
      SecurityOrigin::create(redirectResponse.url());
  if (!redirectingOrigin->canDisplay(requestURL)) {
    FrameLoader::reportLocalLoadFailed(m_frame, requestURL.getString());
    m_fetcher->stopFetching();
    return false;
  }
  if (frameLoader().shouldContinueForNavigationPolicy(
          m_request, SubstituteData(), this, CheckContentSecurityPolicy,
          m_navigationType, NavigationPolicyCurrentTab, m_loadType,
          isClientRedirect(), nullptr) != NavigationPolicyCurrentTab) {
    m_fetcher->stopFetching();
    return false;
  }

  DCHECK(timing().fetchStart());
  appendRedirect(requestURL);
  timing().addRedirect(redirectResponse.url(), requestURL);

  // If a redirection happens during a back/forward navigation, don't restore
  // any state from the old HistoryItem. There is a provisional history item for
  // back/forward navigation only. In the other case, clearing it is a no-op.
  frameLoader().clearProvisionalHistoryItem();

  localFrameClient().dispatchDidReceiveServerRedirectForProvisionalLoad();

  return true;
}

static bool canShowMIMEType(const String& mimeType, LocalFrame* frame) {
  if (MIMETypeRegistry::isSupportedMIMEType(mimeType))
    return true;
  PluginData* pluginData = frame->pluginData();
  return !mimeType.isEmpty() && pluginData &&
         pluginData->supportsMimeType(mimeType);
}

bool DocumentLoader::shouldContinueForResponse() const {
  if (m_substituteData.isValid())
    return true;

  int statusCode = m_response.httpStatusCode();
  if (statusCode == 204 || statusCode == 205) {
    // The server does not want us to replace the page contents.
    return false;
  }

  if (getContentDispositionType(m_response.httpHeaderField(
          HTTPNames::Content_Disposition)) == ContentDispositionAttachment) {
    // The server wants us to download instead of replacing the page contents.
    // Downloading is handled by the embedder, but we still get the initial
    // response so that we can ignore it and clean up properly.
    return false;
  }

  if (!canShowMIMEType(m_response.mimeType(), m_frame))
    return false;
  return true;
}

void DocumentLoader::cancelLoadAfterCSPDenied(
    const ResourceResponse& response) {
  probe::canceledAfterReceivedResourceResponse(
      m_frame, this, mainResourceIdentifier(), response, m_mainResource.get());

  setWasBlockedAfterCSP();

  // Pretend that this was an empty HTTP 200 response.  Don't reuse the original
  // URL for the empty page (https://crbug.com/622385).
  //
  // TODO(mkwst):  Remove this once XFO moves to the browser.
  // https://crbug.com/555418.
  clearMainResourceHandle();
  m_contentSecurityPolicy.clear();
  KURL blockedURL = SecurityOrigin::urlWithUniqueSecurityOrigin();
  m_originalRequest.setURL(blockedURL);
  m_request.setURL(blockedURL);
  m_redirectChain.pop_back();
  appendRedirect(blockedURL);
  m_response = ResourceResponse(blockedURL, "text/html", 0, nullAtom);
  finishedLoading(monotonicallyIncreasingTime());

  return;
}

void DocumentLoader::responseReceived(
    Resource* resource,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK_EQ(m_mainResource, resource);
  DCHECK(!handle);
  DCHECK(m_frame);

  m_applicationCacheHost->didReceiveResponseForMainResource(response);

  // The memory cache doesn't understand the application cache or its caching
  // rules. So if a main resource is served from the application cache, ensure
  // we don't save the result for future use. All responses loaded from appcache
  // will have a non-zero appCacheID().
  if (response.appCacheID())
    memoryCache()->remove(m_mainResource.get());

  m_contentSecurityPolicy = ContentSecurityPolicy::create();
  m_contentSecurityPolicy->setOverrideURLForSelf(response.url());
  m_contentSecurityPolicy->didReceiveHeaders(
      ContentSecurityPolicyResponseHeaders(response));
  if (!m_contentSecurityPolicy->allowAncestors(m_frame, response.url())) {
    cancelLoadAfterCSPDenied(response);
    return;
  }

  if (RuntimeEnabledFeatures::embedderCSPEnforcementEnabled() &&
      !frameLoader().requiredCSP().isEmpty()) {
    SecurityOrigin* parentSecurityOrigin =
        m_frame->tree().parent()->securityContext()->getSecurityOrigin();
    if (ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
            response, parentSecurityOrigin)) {
      m_contentSecurityPolicy->addPolicyFromHeaderValue(
          frameLoader().requiredCSP(), ContentSecurityPolicyHeaderTypeEnforce,
          ContentSecurityPolicyHeaderSourceHTTP);
    } else {
      ContentSecurityPolicy* embeddingCSP = ContentSecurityPolicy::create();
      embeddingCSP->addPolicyFromHeaderValue(
          frameLoader().requiredCSP(), ContentSecurityPolicyHeaderTypeEnforce,
          ContentSecurityPolicyHeaderSourceHTTP);
      if (!embeddingCSP->subsumes(*m_contentSecurityPolicy)) {
        String message = "Refused to display '" +
                         response.url().elidedString() +
                         "' because it has not opted-into the following policy "
                         "required by its embedder: '" +
                         frameLoader().requiredCSP() + "'.";
        ConsoleMessage* consoleMessage = ConsoleMessage::createForRequest(
            SecurityMessageSource, ErrorMessageLevel, message, response.url(),
            mainResourceIdentifier());
        m_frame->document()->addConsoleMessage(consoleMessage);
        cancelLoadAfterCSPDenied(response);
        return;
      }
    }
  }

  DCHECK(!m_frame->page()->suspended());

  if (response.didServiceWorkerNavigationPreload())
    UseCounter::count(m_frame, UseCounter::ServiceWorkerNavigationPreload);
  m_response = response;

  if (isArchiveMIMEType(m_response.mimeType()) &&
      m_mainResource->getDataBufferingPolicy() != BufferData)
    m_mainResource->setDataBufferingPolicy(BufferData);

  if (!shouldContinueForResponse()) {
    probe::continueWithPolicyIgnore(m_frame, this, m_mainResource->identifier(),
                                    m_response, m_mainResource.get());
    m_fetcher->stopFetching();
    return;
  }

  if (m_frame->owner() && m_response.isHTTP() &&
      !FetchUtils::isOkStatus(m_response.httpStatusCode()))
    m_frame->owner()->renderFallbackContent();
}

void DocumentLoader::ensureWriter(const AtomicString& mimeType,
                                  const KURL& overridingURL) {
  if (m_writer)
    return;

  const AtomicString& encoding = response().textEncodingName();

  // Prepare a DocumentInit before clearing the frame, because it may need to
  // inherit an aliased security context.
  Document* owner = nullptr;
  // TODO(dcheng): This differs from the behavior of both IE and Firefox: the
  // origin is inherited from the document that loaded the URL.
  if (shouldInheritSecurityOriginFromOwner(url())) {
    Frame* ownerFrame = m_frame->tree().parent();
    if (!ownerFrame)
      ownerFrame = m_frame->loader().opener();
    if (ownerFrame && ownerFrame->isLocalFrame())
      owner = toLocalFrame(ownerFrame)->document();
  }
  DocumentInit init(owner, url(), m_frame);
  init.withNewRegistrationContext();
  m_frame->loader().clear();
  DCHECK(m_frame->page());

  ParserSynchronizationPolicy parsingPolicy = AllowAsynchronousParsing;
  if ((m_substituteData.isValid() && m_substituteData.forceSynchronousLoad()) ||
      !Document::threadedParsingEnabledForTesting())
    parsingPolicy = ForceSynchronousParsing;

  installNewDocument(init, mimeType, encoding,
                     InstallNewDocumentReason::kNavigation, parsingPolicy,
                     overridingURL);
  m_writer->setDocumentWasLoadedAsPartOfNavigation();
  m_frame->document()->maybeHandleHttpRefresh(
      m_response.httpHeaderField(HTTPNames::Refresh),
      Document::HttpRefreshFromHeader);
}

void DocumentLoader::commitData(const char* bytes, size_t length) {
  DCHECK_EQ(m_state, Committed);
  ensureWriter(m_response.mimeType());

  // This can happen if document.close() is called by an event handler while
  // there's still pending incoming data.
  if (m_frame && !m_frame->document()->parsing())
    return;

  if (length)
    m_dataReceived = true;

  m_writer->addData(bytes, length);
}

void DocumentLoader::dataReceived(Resource* resource,
                                  const char* data,
                                  size_t length) {
  DCHECK(data);
  DCHECK(length);
  DCHECK_EQ(resource, m_mainResource);
  DCHECK(!m_response.isNull());
  DCHECK(!m_frame->page()->suspended());

  if (m_inDataReceived) {
    // If this function is reentered, defer processing of the additional data to
    // the top-level invocation. Reentrant calls can occur because of web
    // platform (mis-)features that require running a nested message loop:
    // - alert(), confirm(), prompt()
    // - Detach of plugin elements.
    // - Synchronous XMLHTTPRequest
    m_dataBuffer->append(data, length);
    return;
  }

  AutoReset<bool> reentrancyProtector(&m_inDataReceived, true);
  processData(data, length);

  // Process data received in reentrant invocations. Note that the invocations
  // of processData() may queue more data in reentrant invocations, so iterate
  // until it's empty.
  const char* segment;
  size_t pos = 0;
  while (size_t length = m_dataBuffer->getSomeData(segment, pos)) {
    processData(segment, length);
    pos += length;
  }
  // All data has been consumed, so flush the buffer.
  m_dataBuffer->clear();
}

void DocumentLoader::processData(const char* data, size_t length) {
  m_applicationCacheHost->mainResourceDataReceived(data, length);
  m_timeOfLastDataReceived = monotonicallyIncreasingTime();

  if (isArchiveMIMEType(response().mimeType()))
    return;
  commitIfReady();
  if (!m_frame)
    return;
  commitData(data, length);

  // If we are sending data to MediaDocument, we should stop here and cancel the
  // request.
  if (m_frame && m_frame->document()->isMediaDocument())
    m_fetcher->stopFetching();
}

void DocumentLoader::clearRedirectChain() {
  m_redirectChain.clear();
}

void DocumentLoader::appendRedirect(const KURL& url) {
  m_redirectChain.push_back(url);
}

void DocumentLoader::detachFromFrame() {
  DCHECK(m_frame);

  // It never makes sense to have a document loader that is detached from its
  // frame have any loads active, so go ahead and kill all the loads.
  m_fetcher->stopFetching();

  if (m_frame && !sentDidFinishLoad())
    frameLoader().loadFailed(this, ResourceError::cancelledError(url()));

  // If that load cancellation triggered another detach, leave.
  // (fast/frames/detach-frame-nested-no-crash.html is an example of this.)
  if (!m_frame)
    return;

  m_fetcher->clearContext();
  m_applicationCacheHost->detachFromDocumentLoader();
  m_applicationCacheHost.clear();
  m_serviceWorkerNetworkProvider = nullptr;
  WeakIdentifierMap<DocumentLoader>::notifyObjectDestroyed(this);
  clearMainResourceHandle();
  m_frame = nullptr;
}

void DocumentLoader::clearMainResourceHandle() {
  if (!m_mainResource)
    return;
  m_mainResource->removeClient(this);
  m_mainResource = nullptr;
}

bool DocumentLoader::maybeCreateArchive() {
  // Give the archive machinery a crack at this document. If the MIME type is
  // not an archive type, it will return 0.
  if (!isArchiveMIMEType(m_response.mimeType()))
    return false;

  DCHECK(m_mainResource);
  ArchiveResource* mainResource =
      m_fetcher->createArchive(m_mainResource.get());
  if (!mainResource)
    return false;
  // The origin is the MHTML file, we need to set the base URL to the document
  // encoded in the MHTML so relative URLs are resolved properly.
  ensureWriter(mainResource->mimeType(), mainResource->url());

  // The Document has now been created.
  m_frame->document()->enforceSandboxFlags(SandboxAll);

  commitData(mainResource->data()->data(), mainResource->data()->size());
  return true;
}

const AtomicString& DocumentLoader::responseMIMEType() const {
  return m_response.mimeType();
}

const KURL& DocumentLoader::unreachableURL() const {
  return m_substituteData.failingURL();
}

bool DocumentLoader::maybeLoadEmpty() {
  bool shouldLoadEmpty = !m_substituteData.isValid() &&
                         (m_request.url().isEmpty() ||
                          SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(
                              m_request.url().protocol()));
  if (!shouldLoadEmpty)
    return false;

  if (m_request.url().isEmpty() &&
      !frameLoader().stateMachine()->creatingInitialEmptyDocument())
    m_request.setURL(blankURL());
  m_response = ResourceResponse(m_request.url(), "text/html", 0, nullAtom);
  finishedLoading(monotonicallyIncreasingTime());
  return true;
}

void DocumentLoader::startLoadingMainResource() {
  timing().markNavigationStart();
  DCHECK(!m_mainResource);
  DCHECK_EQ(m_state, NotStarted);
  m_state = Provisional;

  if (maybeLoadEmpty())
    return;

  DCHECK(timing().navigationStart());

  // PlzNavigate:
  // The fetch has already started in the browser. Don't mark it again.
  if (!m_frame->settings()->getBrowserSideNavigationEnabled()) {
    DCHECK(!timing().fetchStart());
    timing().markFetchStart();
  }

  DEFINE_STATIC_LOCAL(
      ResourceLoaderOptions, mainResourceLoadOptions,
      (DoNotBufferData, AllowStoredCredentials, ClientRequestedCredentials,
       CheckContentSecurityPolicy, DocumentContext));
  FetchRequest fetchRequest(m_request, FetchInitiatorTypeNames::document,
                            mainResourceLoadOptions);
  m_mainResource =
      RawResource::fetchMainResource(fetchRequest, fetcher(), m_substituteData);

  // PlzNavigate:
  // The final access checks are still performed here, potentially rejecting
  // the "provisional" load, but the browser side already expects the renderer
  // to be able to unconditionally commit.
  if (!m_mainResource ||
      (m_frame->settings()->getBrowserSideNavigationEnabled() &&
       m_mainResource->errorOccurred())) {
    m_request = ResourceRequest(blankURL());
    maybeLoadEmpty();
    return;
  }
  // A bunch of headers are set when the underlying resource load begins, and
  // m_request needs to include those. Even when using a cached resource, we may
  // make some modification to the request, e.g. adding the referer header.
  m_request = m_mainResource->isLoading() ? m_mainResource->resourceRequest()
                                          : fetchRequest.resourceRequest();
  m_mainResource->addClient(this);
}

void DocumentLoader::endWriting() {
  m_writer->end();
  m_writer.clear();
}

void DocumentLoader::didInstallNewDocument(Document* document) {
  document->setReadyState(Document::Loading);
  document->initContentSecurityPolicy(m_contentSecurityPolicy.release());

  frameLoader().didInstallNewDocument();

  String suboriginHeader = m_response.httpHeaderField(HTTPNames::Suborigin);
  if (!suboriginHeader.isNull()) {
    Vector<String> messages;
    Suborigin suborigin;
    if (parseSuboriginHeader(suboriginHeader, &suborigin, messages))
      document->enforceSuborigin(suborigin);

    for (auto& message : messages) {
      document->addConsoleMessage(
          ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel,
                                 "Error with Suborigin header: " + message));
    }
  }

  document->clientHintsPreferences().updateFrom(m_clientHintsPreferences);

  // TODO(japhet): There's no reason to wait until commit to set these bits.
  Settings* settings = document->settings();
  m_fetcher->setImagesEnabled(settings->getImagesEnabled());
  m_fetcher->setAutoLoadImages(settings->getLoadsImagesAutomatically());

  const AtomicString& dnsPrefetchControl =
      m_response.httpHeaderField(HTTPNames::X_DNS_Prefetch_Control);
  if (!dnsPrefetchControl.isEmpty())
    document->parseDNSPrefetchControlHeader(dnsPrefetchControl);

  String headerContentLanguage =
      m_response.httpHeaderField(HTTPNames::Content_Language);
  if (!headerContentLanguage.isEmpty()) {
    size_t commaIndex = headerContentLanguage.find(',');
    // kNotFound == -1 == don't truncate
    headerContentLanguage.truncate(commaIndex);
    headerContentLanguage =
        headerContentLanguage.stripWhiteSpace(isHTMLSpace<UChar>);
    if (!headerContentLanguage.isEmpty())
      document->setContentLanguage(AtomicString(headerContentLanguage));
  }

  OriginTrialContext::addTokensFromHeader(
      document, m_response.httpHeaderField(HTTPNames::Origin_Trial));
  String referrerPolicyHeader =
      m_response.httpHeaderField(HTTPNames::Referrer_Policy);
  if (!referrerPolicyHeader.isNull()) {
    UseCounter::count(*document, UseCounter::ReferrerPolicyHeader);
    document->parseAndSetReferrerPolicy(referrerPolicyHeader);
  }

  localFrameClient().didCreateNewDocument();
}

void DocumentLoader::didCommitNavigation() {
  if (frameLoader().stateMachine()->creatingInitialEmptyDocument())
    return;
  frameLoader().receivedFirstData();

  // didObserveLoadingBehavior() must be called after dispatchDidCommitLoad() is
  // called for the metrics tracking logic to handle it properly.
  if (m_serviceWorkerNetworkProvider &&
      m_serviceWorkerNetworkProvider->isControlledByServiceWorker()) {
    localFrameClient().didObserveLoadingBehavior(
        WebLoadingBehaviorServiceWorkerControlled);
  }

  // Links with media values need more information (like viewport information).
  // This happens after the first chunk is parsed in HTMLDocumentParser.
  dispatchLinkHeaderPreloads(nullptr, LinkLoader::OnlyLoadNonMedia);

  TRACE_EVENT1("devtools.timeline", "CommitLoad", "data",
               InspectorCommitLoadEvent::data(m_frame));
  probe::didCommitLoad(m_frame, this);
  m_frame->page()->didCommitLoad(m_frame);
}

void setFeaturePolicy(Document* document, const String& featurePolicyHeader) {
  if (!RuntimeEnabledFeatures::featurePolicyEnabled())
    return;
  LocalFrame* frame = document->frame();
  WebFeaturePolicy* parentFeaturePolicy =
      frame->isMainFrame()
          ? nullptr
          : frame->tree().parent()->securityContext()->getFeaturePolicy();
  Vector<String> messages;
  const WebParsedFeaturePolicy& parsedHeader = parseFeaturePolicy(
      featurePolicyHeader, frame->securityContext()->getSecurityOrigin(),
      &messages);
  WebParsedFeaturePolicy containerPolicy;
  if (frame->owner()) {
    containerPolicy = getContainerPolicyFromAllowedFeatures(
        frame->owner()->allowedFeatures(),
        frame->securityContext()->getSecurityOrigin());
  }
  frame->securityContext()->initializeFeaturePolicy(
      parsedHeader, containerPolicy, parentFeaturePolicy);

  for (auto& message : messages) {
    document->addConsoleMessage(
        ConsoleMessage::create(OtherMessageSource, ErrorMessageLevel,
                               "Error with Feature-Policy header: " + message));
  }
  if (!parsedHeader.isEmpty())
    frame->client()->didSetFeaturePolicyHeader(parsedHeader);
}

void DocumentLoader::installNewDocument(
    const DocumentInit& init,
    const AtomicString& mimeType,
    const AtomicString& encoding,
    InstallNewDocumentReason reason,
    ParserSynchronizationPolicy parsingPolicy,
    const KURL& overridingURL) {
  DCHECK_EQ(init.frame(), m_frame);
  DCHECK(!m_frame->document() || !m_frame->document()->isActive());
  DCHECK_EQ(m_frame->tree().childCount(), 0u);

  if (!init.shouldReuseDefaultView())
    m_frame->setDOMWindow(LocalDOMWindow::create(*m_frame));

  Document* document = m_frame->domWindow()->installNewDocument(mimeType, init);
  m_frame->page()->chromeClient().installSupplements(*m_frame);
  if (!overridingURL.isEmpty())
    document->setBaseURLOverride(overridingURL);
  didInstallNewDocument(document);

  // This must be called before DocumentWriter is created, otherwise HTML parser
  // will use stale values from HTMLParserOption.
  if (reason == InstallNewDocumentReason::kNavigation)
    didCommitNavigation();

  m_writer =
      DocumentWriter::create(document, parsingPolicy, mimeType, encoding);

  // FeaturePolicy is reset in the browser process on commit, so this needs to
  // be initialized and replicated to the browser process after commit messages
  // are sent in didCommitNavigation().
  setFeaturePolicy(document,
                   m_response.httpHeaderField(HTTPNames::Feature_Policy));
  frameLoader().dispatchDidClearDocumentOfWindowObject();
}

const AtomicString& DocumentLoader::mimeType() const {
  if (m_writer)
    return m_writer->mimeType();
  return m_response.mimeType();
}

// This is only called by
// FrameLoader::replaceDocumentWhileExecutingJavaScriptURL()
void DocumentLoader::replaceDocumentWhileExecutingJavaScriptURL(
    const DocumentInit& init,
    const String& source) {
  installNewDocument(init, mimeType(),
                     m_writer ? m_writer->encoding() : emptyAtom,
                     InstallNewDocumentReason::kJavascriptURL,
                     ForceSynchronousParsing, KURL());
  if (!source.isNull())
    m_writer->appendReplacingData(source);
  endWriting();
}

DEFINE_WEAK_IDENTIFIER_MAP(DocumentLoader);

}  // namespace blink

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

#include "config.h"
#include "core/loader/DocumentLoader.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/Event.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/history/HistoryItem.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentWriter.h"
#include "core/loader/FormState.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ResourceLoader.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/archive/ArchiveResourceCollection.h"
#include "core/loader/archive/MHTMLArchive.h"
#include "core/loader/cache/CachedResourceLoader.h"
#include "core/loader/cache/MemoryCache.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/SecurityPolicy.h"
#include "core/page/Settings.h"
#include "core/platform/Logging.h"
#include "core/platform/SchemeRegistry.h"
#include <wtf/Assertions.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

static void cancelAll(const ResourceLoaderSet& loaders)
{
    Vector<RefPtr<ResourceLoader> > loadersCopy;
    copyToVector(loaders, loadersCopy);
    size_t size = loadersCopy.size();
    for (size_t i = 0; i < size; ++i)
        loadersCopy[i]->cancel();
}

static void setAllDefersLoading(const ResourceLoaderSet& loaders, bool defers)
{
    Vector<RefPtr<ResourceLoader> > loadersCopy;
    copyToVector(loaders, loadersCopy);
    size_t size = loadersCopy.size();
    for (size_t i = 0; i < size; ++i)
        loadersCopy[i]->setDefersLoading(defers);
}

DocumentLoader::DocumentLoader(const ResourceRequest& req, const SubstituteData& substituteData)
    : m_deferMainResourceDataLoad(true)
    , m_frame(0)
    , m_cachedResourceLoader(CachedResourceLoader::create(this))
    , m_writer(m_frame)
    , m_originalRequest(req)
    , m_substituteData(substituteData)
    , m_originalRequestCopy(req)
    , m_request(req)
    , m_originalSubstituteDataWasValid(substituteData.isValid())
    , m_committed(false)
    , m_isStopping(false)
    , m_gotFirstByte(false)
    , m_isClientRedirect(false)
    , m_isLoadingMultipartContent(false)
    , m_wasOnloadHandled(false)
    , m_substituteResourceDeliveryTimer(this, &DocumentLoader::substituteResourceDeliveryTimerFired)
    , m_loadingMainResource(false)
    , m_timeOfLastDataReceived(0.0)
    , m_identifierForLoadWithoutResourceLoader(0)
    , m_dataLoadTimer(this, &DocumentLoader::handleSubstituteDataLoadNow)
    , m_applicationCacheHost(adoptPtr(new ApplicationCacheHost(this)))
{
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

ResourceLoader* DocumentLoader::mainResourceLoader() const
{
    return m_mainResource ? m_mainResource->loader() : 0;
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame || frameLoader()->activeDocumentLoader() != this || !isLoading());
    m_cachedResourceLoader->clearDocumentLoader();
    
    if (m_mainResource) {
        m_mainResource->removeClient(this);
        m_mainResource = 0;
    }
}

PassRefPtr<SharedBuffer> DocumentLoader::mainResourceData() const
{
    if (m_substituteData.isValid())
        return m_substituteData.content()->copy();
    if (m_mainResource)
        return m_mainResource->resourceBuffer();
    return 0;
}

Document* DocumentLoader::document() const
{
    if (m_frame && m_frame->loader()->documentLoader() == this)
        return m_frame->document();
    return 0;
}

const ResourceRequest& DocumentLoader::originalRequest() const
{
    return m_originalRequest;
}

const ResourceRequest& DocumentLoader::originalRequestCopy() const
{
    return m_originalRequestCopy;
}

const ResourceRequest& DocumentLoader::request() const
{
    return m_request;
}

ResourceRequest& DocumentLoader::request()
{
    return m_request;
}

const KURL& DocumentLoader::url() const
{
    return request().url();
}

void DocumentLoader::replaceRequestURLForSameDocumentNavigation(const KURL& url)
{
    m_originalRequestCopy.setURL(url);
    m_request.setURL(url);
}

void DocumentLoader::setRequest(const ResourceRequest& req)
{
    // Replacing an unreachable URL with alternate content looks like a server-side
    // redirect at this point, but we can replace a committed dataSource.
    bool handlingUnreachableURL = false;

    handlingUnreachableURL = m_substituteData.isValid() && !m_substituteData.failingURL().isEmpty();

    if (handlingUnreachableURL)
        m_committed = false;

    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!m_committed);

    m_request = req;
}

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    m_mainDocumentError = error;    
    frameLoader()->client()->setMainDocumentError(this, error);
}

void DocumentLoader::mainReceivedError(const ResourceError& error)
{
    ASSERT(!error.isNull());
    if (m_identifierForLoadWithoutResourceLoader) {
        ASSERT(!mainResourceLoader());
        frameLoader()->client()->dispatchDidFailLoading(this, m_identifierForLoadWithoutResourceLoader, error);
    }

    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());

    m_applicationCacheHost->failedLoadingMainResource();

    if (!frameLoader())
        return;
    setMainDocumentError(error);
    clearMainResourceLoader();
    frameLoader()->receivedMainResourceError(error);
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    // In some rare cases, calling FrameLoader::stopLoading could cause isLoading() to return false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = isLoading();
    
    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        Document* doc = m_frame->document();
        
        if (loading || doc->parsing())
            m_frame->loader()->stopLoading(UnloadEventPolicyNone);
    }

    // Always cancel multipart loaders
    cancelAll(m_multipartResourceLoaders);
    
    clearArchiveResources();

    if (!loading) {
        // If something above restarted loading we might run into mysterious crashes like 
        // https://bugs.webkit.org/show_bug.cgi?id=62764 and <rdar://problem/9328684>
        ASSERT(!isLoading());
        return;
    }

    // We might run in to infinite recursion if we're stopping loading as the result of 
    // detaching from the frame, so break out of that recursion here.
    // See <rdar://problem/9673866> for more details.
    if (m_isStopping)
        return;

    m_isStopping = true;

    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    
    if (isLoadingMainResource())
        // Stop the main resource loader and let it send the cancelled message.
        cancelMainResourceLoad(frameLoader->cancelledError(m_request));
    else if (!m_resourceLoaders.isEmpty())
        // The main resource loader already finished loading. Set the cancelled error on the 
        // document and let the resourceLoaders send individual cancelled messages below.
        setMainDocumentError(frameLoader->cancelledError(m_request));
    else
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        mainReceivedError(frameLoader->cancelledError(m_request));
    
    stopLoadingSubresources();
    
    m_isStopping = false;
}

void DocumentLoader::commitIfReady()
{
    if (!m_committed) {
        m_committed = true;
        frameLoader()->commitProvisionalLoad();
        m_writer.setMIMEType(m_response.mimeType());
    }
}

bool DocumentLoader::isLoading() const
{
    if (document() && document()->hasActiveParser())
        return true;

    return isLoadingMainResource() || !m_resourceLoaders.isEmpty();
}

void DocumentLoader::notifyFinished(CachedResource* resource)
{
    ASSERT_UNUSED(resource, m_mainResource == resource);
    ASSERT(m_mainResource);
    if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
        finishedLoading(m_mainResource->loadFinishTime());
        return;
    }

    mainReceivedError(m_mainResource->resourceError());
}

void DocumentLoader::finishedLoading(double finishTime)
{
    ASSERT(!m_frame->page()->defersLoading() || InspectorInstrumentation::isDebuggerPaused(m_frame));

    RefPtr<DocumentLoader> protect(this);

    if (m_identifierForLoadWithoutResourceLoader) {
        frameLoader()->notifier()->dispatchDidFinishLoading(this, m_identifierForLoadWithoutResourceLoader, finishTime);
        m_identifierForLoadWithoutResourceLoader = 0;
    }

    maybeFinishLoadingMultipartContent();

    double responseEndTime = finishTime;
    if (!responseEndTime)
        responseEndTime = m_timeOfLastDataReceived;
    if (!responseEndTime)
        responseEndTime = monotonicallyIncreasingTime();
    timing()->setResponseEnd(responseEndTime);

    commitIfReady();
    if (!frameLoader())
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_gotFirstByte)
            commitData(0, 0);
        frameLoader()->client()->finishedLoading(this);
    }

    m_writer.end();
    if (!m_mainDocumentError.isNull())
        return;
    clearMainResourceLoader();
    if (!frameLoader()->stateMachine()->creatingInitialEmptyDocument())
        frameLoader()->checkLoadComplete();

    // If the document specified an application cache manifest, it violates the author's intent if we store it in the memory cache
    // and deny the appcache the chance to intercept it in the future, so remove from the memory cache.
    if (m_frame) {
        if (m_mainResource && m_frame->document()->hasManifest())
            memoryCache()->remove(m_mainResource.get());
    }
    m_applicationCacheHost->finishedLoadingMainResource();
}

bool DocumentLoader::isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (newRequest.httpMethod() == "POST")
        return true;

    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && m_originalRequest.httpMethod() == "POST")
        return true;

    return false;
}

void DocumentLoader::handleSubstituteDataLoadNow(DocumentLoaderTimer*)
{
    KURL url = m_substituteData.responseURL();
    if (url.isEmpty())
        url = m_request.url();
    ResourceResponse response(url, m_substituteData.mimeType(), m_substituteData.content()->size(), m_substituteData.textEncoding(), "");
    responseReceived(0, response);
}

void DocumentLoader::startDataLoadTimer()
{
    m_dataLoadTimer.startOneShot(0);
}

void DocumentLoader::handleSubstituteDataLoadSoon()
{
    if (m_deferMainResourceDataLoad)
        startDataLoadTimer();
    else
        handleSubstituteDataLoadNow(0);
}

bool DocumentLoader::shouldContinueForNavigationPolicy(const ResourceRequest& request)
{
    NavigationAction action = triggeringAction();
    if (action.isEmpty()) {
        action = NavigationAction(request, NavigationTypeOther);
        setTriggeringAction(action);
    }

    // Don't ask if we are loading an empty URL.
    if (!request.isNull() && request.url().isEmpty())
        return true;

    // We are always willing to show alternate content for unreachable URLs;
    // treat it like a reload so it maintains the right state for b/f list.
    if (m_substituteData.isValid() && !m_substituteData.failingURL().isEmpty()) {
        if (isBackForwardLoadType(frameLoader()->loadType()))
            frameLoader()->setLoadType(FrameLoadTypeReload);
        return true;
    }

    // If we're loading content into a subframe, check against the parent's Content Security Policy
    // and kill the load if that check fails.
    if (m_frame->ownerElement() && !m_frame->ownerElement()->document()->contentSecurityPolicy()->allowChildFrameFromSource(request.url()))
        return false;

    PolicyAction policy = frameLoader()->client()->decidePolicyForNavigationAction(action, request);
    if (policy == PolicyDownload) {
        ResourceRequest mutableRequest(request);
        frameLoader()->setOriginalURLForDownloadRequest(mutableRequest);
        frameLoader()->client()->startDownload(mutableRequest);
    }
    return policy == PolicyUse;
}

void DocumentLoader::redirectReceived(CachedResource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(resource, resource == m_mainResource);
    willSendRequest(request, redirectResponse);
}

void DocumentLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());

    if (!frameLoader()->checkIfFormActionAllowedByCSP(newRequest.url())) {
        cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
        return;
    }

    ASSERT(timing()->fetchStart());
    if (!redirectResponse.isNull()) {
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        RefPtr<SecurityOrigin> redirectingOrigin = SecurityOrigin::create(redirectResponse.url());
        if (!redirectingOrigin->canDisplay(newRequest.url())) {
            FrameLoader::reportLocalLoadFailed(m_frame, newRequest.url().string());
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return;
        }
        timing()->addRedirect(redirectResponse.url(), newRequest.url());
    }

    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frameLoader()->isLoadingMainFrame())
        newRequest.setFirstPartyForCookies(newRequest.url());

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ReloadIgnoringCacheData);

    Frame* top = m_frame->tree()->top();
    if (top != m_frame) {
        if (!frameLoader()->mixedContentChecker()->canDisplayInsecureContent(top->document()->securityOrigin(), newRequest.url())) {
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return;
        }
    }

    setRequest(newRequest);

    if (redirectResponse.isNull())
        return;

    if (!shouldContinueForNavigationPolicy(newRequest))
        stopLoadingForPolicyChange();
}

bool DocumentLoader::shouldContinueForResponse() const
{
    if (m_substituteData.isValid())
        return true;

    int statusCode = m_response.httpStatusCode();
    if (statusCode == 204 || statusCode == 205) {
        // The server does not want us to replace the page contents.
        return false;
    }

    if (contentDispositionType(m_response.httpHeaderField("Content-Disposition")) == ContentDispositionAttachment) {
        // The server wants us to download instead of replacing the page contents.
        // Downloading is handled by the embedder, but we still get the initial
        // response so that we can ignore it and clean up properly.
        return false;
    }

    if (!frameLoader()->client()->canShowMIMEType(m_response.mimeType()))
        return false;

    // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks.
    if (equalIgnoringCase("multipart/related", m_response.mimeType()) && !SchemeRegistry::shouldTreatURLSchemeAsLocal(m_request.url().protocol()))
        return false;

    return true;
}

void DocumentLoader::responseReceived(CachedResource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, m_mainResource == resource);
    RefPtr<DocumentLoader> protect(this);

    m_applicationCacheHost->didReceiveResponseForMainResource(response);

    // The memory cache doesn't understand the application cache or its caching rules. So if a main resource is served
    // from the application cache, ensure we don't save the result for future use. All responses loaded
    // from appcache will have a non-zero appCacheID().
    if (response.appCacheID())
        memoryCache()->remove(m_mainResource.get());

    DEFINE_STATIC_LOCAL(AtomicString, xFrameOptionHeader, ("x-frame-options", AtomicString::ConstructFromLiteral));
    HTTPHeaderMap::const_iterator it = response.httpHeaderFields().find(xFrameOptionHeader);
    if (it != response.httpHeaderFields().end()) {
        String content = it->value;
        ASSERT(m_mainResource);
        unsigned long identifier = m_identifierForLoadWithoutResourceLoader ? m_identifierForLoadWithoutResourceLoader : m_mainResource->identifier();
        ASSERT(identifier);
        if (frameLoader()->shouldInterruptLoadForXFrameOptions(content, response.url(), identifier)) {
            InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_frame, this, identifier, response);
            String message = "Refused to display '" + response.url().elidedString() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.";
            frame()->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, identifier);
            frame()->document()->enforceSandboxFlags(SandboxOrigin);
            if (HTMLFrameOwnerElement* ownerElement = frame()->ownerElement())
                ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, false, false));
            cancelMainResourceLoad(frameLoader()->cancelledError(m_request));
            return;
        }
    }

    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());

    if (m_isLoadingMultipartContent) {
        setupForReplace();
        m_mainResource->clear();
    } else if (response.isMultipart()) {
        UseCounter::count(m_frame->document(), UseCounter::MultipartMainResource);
        m_isLoadingMultipartContent = true;
    }

    m_response = response;

    if (m_identifierForLoadWithoutResourceLoader)
        frameLoader()->notifier()->dispatchDidReceiveResponse(this, m_identifierForLoadWithoutResourceLoader, m_response, 0);

    if (!shouldContinueForResponse()) {
        InspectorInstrumentation::continueWithPolicyIgnore(m_frame, this, mainResourceLoader()->identifier(), m_response);
        stopLoadingForPolicyChange();
        return;
    }

    if (m_response.isHTTP()) {
        int status = m_response.httpStatusCode();
        if (status < 200 || status >= 300) {
            bool hostedByObject = frameLoader()->isHostedByObjectElement();

            frameLoader()->handleFallbackContent();
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                cancelMainResourceLoad(frameLoader()->cancelledError(m_request));
        }
    }

    if (!isStopping() && m_substituteData.isValid()) {
        if (m_substituteData.content()->size())
            dataReceived(0, m_substituteData.content()->data(), m_substituteData.content()->size());
        if (isLoadingMainResource())
            finishedLoading(0);
    }
}

static bool isArchiveMIMEType(const String& mimeType)
{
    return mimeType == "multipart/related";
}

void DocumentLoader::commitLoad(const char* data, int length)
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    commitIfReady();
    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    if (!frameLoader)
        return;
    if (isArchiveMIMEType(response().mimeType()))
        return;
    frameLoader->client()->committedLoad(this, data, length);
}

ResourceError DocumentLoader::interruptedForPolicyChangeError() const
{
    return frameLoader()->client()->interruptedForPolicyChangeError(request());
}

void DocumentLoader::stopLoadingForPolicyChange()
{
    ResourceError error = interruptedForPolicyChangeError();
    error.setIsCancellation(true);
    cancelMainResourceLoad(error);
}

void DocumentLoader::commitData(const char* bytes, size_t length)
{
    if (!m_gotFirstByte) {
        m_gotFirstByte = true;
        m_writer.begin(documentURL(), false);
        m_writer.setDocumentWasLoadedAsPartOfNavigation();

        if (SecurityPolicy::allowSubstituteDataAccessToLocal() && m_originalSubstituteDataWasValid) {
            // If this document was loaded with substituteData, then the document can
            // load local resources. See https://bugs.webkit.org/show_bug.cgi?id=16756
            // and https://bugs.webkit.org/show_bug.cgi?id=19760 for further
            // discussion.
            m_frame->document()->securityOrigin()->grantLoadLocalResources();
        }

        if (frameLoader()->stateMachine()->creatingInitialEmptyDocument())
            return;
        if (frameLoader()->stateMachine()->isDisplayingInitialEmptyDocument())
            frameLoader()->stateMachine()->advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
        
        // The origin is the MHTML file, we need to set the base URL to the document encoded in the MHTML so
        // relative URLs are resolved properly.
        if (m_archive)
            m_frame->document()->setBaseURLOverride(m_archive->mainResource()->url());

        // Call receivedFirstData() exactly once per load. We should only reach this point multiple times
        // for multipart loads, and FrameLoader::isReplacing() will be true after the first time.
        if (!isMultipartReplacingLoad())
            frameLoader()->receivedFirstData();

        bool userChosen = true;
        String encoding = overrideEncoding();
        if (encoding.isNull()) {
            userChosen = false;
            encoding = response().textEncodingName();
        }
        m_writer.setEncoding(encoding, userChosen);
    }
    ASSERT(m_frame->document()->parsing());
    m_writer.addData(bytes, length);
}

void DocumentLoader::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Loader);
    info.addMember(m_frame, "frame");
    info.addMember(m_cachedResourceLoader, "cachedResourceLoader");
    info.addMember(m_mainResource, "mainResource");
    info.addMember(m_resourceLoaders, "resourceLoaders");
    info.addMember(m_multipartResourceLoaders, "multipartResourceLoaders");
    info.addMember(m_substituteData, "substituteData");
    info.addMember(m_pageTitle.string(), "pageTitle.string()");
    info.addMember(m_overrideEncoding, "overrideEncoding");
    info.addMember(m_originalRequest, "originalRequest");
    info.addMember(m_originalRequestCopy, "originalRequestCopy");
    info.addMember(m_request, "request");
    info.addMember(m_response, "response");
    info.addMember(m_pendingSubstituteResources, "pendingSubstituteResources");
    info.addMember(m_substituteResourceDeliveryTimer, "substituteResourceDeliveryTimer");
    info.addMember(m_archiveResourceCollection, "archiveResourceCollection");
    info.addMember(m_archive, "archive");
    info.addMember(m_resourcesClientKnowsAbout, "resourcesClientKnowsAbout");
    info.addMember(m_resourcesLoadedFromMemoryCacheForClientNotification, "resourcesLoadedFromMemoryCacheForClientNotification");
    info.addMember(m_applicationCacheHost, "applicationCacheHost");
}

void DocumentLoader::dataReceived(CachedResource* resource, const char* data, int length)
{
    ASSERT(data);
    ASSERT(length);
    ASSERT_UNUSED(resource, resource == m_mainResource);
    ASSERT(!m_response.isNull());
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());

    if (m_identifierForLoadWithoutResourceLoader)
        frameLoader()->notifier()->dispatchDidReceiveData(this, m_identifierForLoadWithoutResourceLoader, data, length, -1);

    m_applicationCacheHost->mainResourceDataReceived(data, length);
    m_timeOfLastDataReceived = monotonicallyIncreasingTime();

    if (!isMultipartReplacingLoad())
        commitLoad(data, length);
}

void DocumentLoader::setupForReplace()
{
    if (!mainResourceData())
        return;
    
    maybeFinishLoadingMultipartContent();
    maybeCreateArchive();
    m_writer.end();
    frameLoader()->setReplacing();
    m_gotFirstByte = false;
    
    stopLoadingSubresources();
    clearArchiveResources();
}

void DocumentLoader::checkLoadComplete()
{
    if (!m_frame || isLoading())
        return;
    // FIXME: This ASSERT is always triggered.
    // See https://bugs.webkit.org/show_bug.cgi?id=110937
    // ASSERT(this == frameLoader()->activeDocumentLoader())
    m_frame->document()->domWindow()->finishedLoading();
}

void DocumentLoader::setFrame(Frame* frame)
{
    if (m_frame == frame)
        return;
    ASSERT(frame && !m_frame);
    m_frame = frame;
    m_writer.setFrame(frame);
}

void DocumentLoader::detachFromFrame()
{
    ASSERT(m_frame);
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so go ahead and kill all the loads.
    stopLoading();

    m_applicationCacheHost->setDOMApplicationCache(0);
    InspectorInstrumentation::loaderDetachedFromFrame(m_frame, this);
    m_frame = 0;
}

void DocumentLoader::clearMainResourceLoader()
{
    m_loadingMainResource = false;
    if (this == frameLoader()->activeDocumentLoader())
        checkLoadComplete();
}

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != FrameStateComplete) {
        Document* doc = m_frame->document();
        if ((isLoadingMainResource() || !m_frame->document()->loadEventFinished()) && isLoading())
            return true;
        if (m_cachedResourceLoader->requestCount())
            return true;
        if (doc->processingLoadEvent())
            return true;
        if (doc->hasActiveParser())
            return true;
    }
    return frameLoader()->subframeIsLoading();
}

bool DocumentLoader::maybeCreateArchive()
{
    // Give the archive machinery a crack at this document. If the MIME type is not an archive type, it will return 0.
    if (!isArchiveMIMEType(m_response.mimeType()))
        return false;

    m_archive = MHTMLArchive::create(m_response.url(), mainResourceData().get());
    ASSERT(m_archive);
    
    addAllArchiveResources(m_archive.get());
    ArchiveResource* mainResource = m_archive->mainResource();
    m_writer.setMIMEType(mainResource->mimeType());
    
    ASSERT(m_frame->document());
    commitData(mainResource->data()->data(), mainResource->data()->size());
    return true;
}

void DocumentLoader::addAllArchiveResources(MHTMLArchive* archive)
{
    ASSERT(archive);
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = adoptPtr(new ArchiveResourceCollection);
    m_archiveResourceCollection->addAllResources(archive);
}

void DocumentLoader::prepareSubframeArchiveLoadIfNeeded()
{
    if (!m_frame->tree()->parent())
        return;

    ArchiveResourceCollection* parentCollection = m_frame->tree()->parent()->loader()->documentLoader()->m_archiveResourceCollection.get();
    if (!parentCollection)
        return;

    m_archive = parentCollection->popSubframeArchive(m_frame->tree()->uniqueName(), m_request.url());

    if (!m_archive)
        return;
    addAllArchiveResources(m_archive.get());

    ArchiveResource* mainResource = m_archive->mainResource();
    m_substituteData = SubstituteData(mainResource->data(), mainResource->mimeType(), mainResource->textEncoding(), KURL());
}

void DocumentLoader::clearArchiveResources()
{
    m_archiveResourceCollection.clear();
    m_substituteResourceDeliveryTimer.stop();
}

ArchiveResource* DocumentLoader::archiveResourceForURL(const KURL& url) const
{
    if (!m_archiveResourceCollection)
        return 0;
        
    ArchiveResource* resource = m_archiveResourceCollection->archiveResourceForURL(url);

    return resource && !resource->shouldIgnoreWhenUnarchiving() ? resource : 0;
}

void DocumentLoader::deliverSubstituteResourcesAfterDelay()
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame && m_frame->page());
    if (m_frame->page()->defersLoading())
        return;
    if (!m_substituteResourceDeliveryTimer.isActive())
        m_substituteResourceDeliveryTimer.startOneShot(0);
}

void DocumentLoader::substituteResourceDeliveryTimerFired(Timer<DocumentLoader>*)
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame && m_frame->page());
    if (m_frame->page()->defersLoading())
        return;

    SubstituteResourceMap copy;
    copy.swap(m_pendingSubstituteResources);

    SubstituteResourceMap::const_iterator end = copy.end();
    for (SubstituteResourceMap::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<ResourceLoader> loader = it->key;
        SubstituteResource* resource = it->value.get();
        
        if (resource) {
            SharedBuffer* data = resource->data();
        
            loader->didReceiveResponse(0, resource->response());

            // Calling ResourceLoader::didReceiveResponse can end up cancelling the load,
            // so we need to check if the loader has reached its terminal state.
            if (loader->reachedTerminalState())
                return;

            loader->didReceiveData(0, data->data(), data->size(), data->size());

            // Calling ResourceLoader::didReceiveData can end up cancelling the load,
            // so we need to check if the loader has reached its terminal state.
            if (loader->reachedTerminalState())
                return;

            loader->didFinishLoading(0, 0);
        } else {
            // A null resource means that we should fail the load.
            // FIXME: Maybe we should use another error here - something like "not in cache".
            loader->didFail(0, loader->cannotShowURLError());
        }
    }
}

#ifndef NDEBUG
bool DocumentLoader::isSubstituteLoadPending(ResourceLoader* loader) const
{
    return m_pendingSubstituteResources.contains(loader);
}
#endif

void DocumentLoader::cancelPendingSubstituteLoad(ResourceLoader* loader)
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    m_pendingSubstituteResources.remove(loader);
    if (m_pendingSubstituteResources.isEmpty())
        m_substituteResourceDeliveryTimer.stop();
}

bool DocumentLoader::scheduleArchiveLoad(ResourceLoader* loader, const ResourceRequest& request)
{
    if (ArchiveResource* resource = archiveResourceForURL(request.url())) {
        m_pendingSubstituteResources.set(loader, resource);
        deliverSubstituteResourcesAfterDelay();
        return true;
    }
    return m_archive;
}

void DocumentLoader::setTitle(const StringWithDirection& title)
{
    if (m_pageTitle == title)
        return;

    m_pageTitle = title;
    frameLoader()->didChangeTitle(this);
}

KURL DocumentLoader::urlForHistory() const
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    if (m_substituteData.isValid())
        return unreachableURL();

    return m_originalRequestCopy.url();
}

const KURL& DocumentLoader::originalURL() const
{
    return m_originalRequestCopy.url();
}

const KURL& DocumentLoader::requestURL() const
{
    return request().url();
}

KURL DocumentLoader::documentURL() const
{
    KURL url = substituteData().responseURL();
    if (url.isEmpty())
        url = requestURL();
    if (url.isEmpty())
        url = m_response.url();
    return url;
}

const String& DocumentLoader::responseMIMEType() const
{
    return m_response.mimeType();
}

const KURL& DocumentLoader::unreachableURL() const
{
    return m_substituteData.failingURL();
}

void DocumentLoader::setDefersLoading(bool defers)
{
    // Multiple frames may be loading the same main resource simultaneously. If deferral state changes,
    // each frame's DocumentLoader will try to send a setDefersLoading() to the same underlying ResourceLoader. Ensure only
    // the "owning" DocumentLoader does so, as setDefersLoading() is not resilient to setting the same value repeatedly.
    if (mainResourceLoader() && mainResourceLoader()->documentLoader() == this)
        mainResourceLoader()->setDefersLoading(defers);

    setAllDefersLoading(m_resourceLoaders, defers);
    if (!defers)
        deliverSubstituteResourcesAfterDelay();
}

void DocumentLoader::setMainResourceDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    if (m_mainResource)
        m_mainResource->setDataBufferingPolicy(dataBufferingPolicy);
}

void DocumentLoader::stopLoadingSubresources()
{
    cancelAll(m_resourceLoaders);
}

void DocumentLoader::addResourceLoader(ResourceLoader* loader)
{
    // The main resource's underlying ResourceLoader will ask to be added here.
    // It is much simpler to handle special casing of main resource loads if we don't
    // let it be added. In the main resource load case, mainResourceLoader()
    // will still be null at this point, but m_gotFirstByte should be false here if and only
    // if we are just starting the main resource load.
    if (!m_gotFirstByte)
        return;
    ASSERT(!m_resourceLoaders.contains(loader));
    ASSERT(!mainResourceLoader() || mainResourceLoader() != loader);
    m_resourceLoaders.add(loader);
}

void DocumentLoader::removeResourceLoader(ResourceLoader* loader)
{
    if (!m_resourceLoaders.contains(loader))
        return;
    m_resourceLoaders.remove(loader);
    checkLoadComplete();
    if (Frame* frame = m_frame)
        frame->loader()->checkLoadComplete();
}

bool DocumentLoader::isMultipartReplacingLoad() const
{
    return isLoadingMultipartContent() && frameLoader()->isReplacing();
}

bool DocumentLoader::maybeLoadEmpty()
{
    bool shouldLoadEmpty = !m_substituteData.isValid() && (m_request.url().isEmpty() || SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(m_request.url().protocol()));
    if (!shouldLoadEmpty)
        return false;

    if (m_request.url().isEmpty() && !frameLoader()->stateMachine()->creatingInitialEmptyDocument())
        m_request.setURL(blankURL());
    String mimeType = shouldLoadEmpty ? "text/html" : frameLoader()->client()->generatedMIMETypeForURLScheme(m_request.url().protocol());
    m_response = ResourceResponse(m_request.url(), mimeType, 0, String(), String());
    finishedLoading(monotonicallyIncreasingTime());
    return true;
}

void DocumentLoader::startLoadingMainResource()
{
    m_mainDocumentError = ResourceError();
    timing()->markNavigationStart();
    ASSERT(!m_mainResource);
    ASSERT(!m_loadingMainResource);
    m_loadingMainResource = true;

    if (maybeLoadEmpty())
        return;

    ASSERT(timing()->navigationStart());
    ASSERT(!timing()->fetchStart());
    timing()->markFetchStart();
    willSendRequest(m_request, ResourceResponse());

    // willSendRequest() may lead to our Frame being detached or cancelling the load via nulling the ResourceRequest.
    if (!m_frame || m_request.isNull())
        return;

    m_applicationCacheHost->willStartLoadingMainResource(m_request);
    prepareSubframeArchiveLoadIfNeeded();

    if (m_substituteData.isValid()) {
        m_identifierForLoadWithoutResourceLoader = createUniqueIdentifier();
        frameLoader()->notifier()->dispatchWillSendRequest(this, m_identifierForLoadWithoutResourceLoader, m_request, ResourceResponse());
        handleSubstituteDataLoadSoon();
        return;
    }

    ResourceRequest request(m_request);
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, mainResourceLoadOptions,
        (SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForCrossOriginCredentials, SkipSecurityCheck));
    CachedResourceRequest cachedResourceRequest(request, mainResourceLoadOptions);
    m_mainResource = m_cachedResourceLoader->requestMainResource(cachedResourceRequest);
    if (!m_mainResource) {
        setRequest(ResourceRequest());
        // If the load was aborted by clearing m_request, it's possible the ApplicationCacheHost
        // is now in a state where starting an empty load will be inconsistent. Replace it with
        // a new ApplicationCacheHost.
        m_applicationCacheHost = adoptPtr(new ApplicationCacheHost(this));
        maybeLoadEmpty();
        return;
    }

    if (!mainResourceLoader()) {
        m_identifierForLoadWithoutResourceLoader = createUniqueIdentifier();
        frameLoader()->notifier()->dispatchWillSendRequest(this, m_identifierForLoadWithoutResourceLoader, request, ResourceResponse());
    }
    m_mainResource->addClient(this);

    // A bunch of headers are set when the underlying ResourceLoader is created, and m_request needs to include those.
    if (mainResourceLoader())
        request = mainResourceLoader()->originalRequest();
    // If there was a fragment identifier on m_request, the cache will have stripped it. m_request should include
    // the fragment identifier, so add that back in.
    if (equalIgnoringFragmentIdentifier(m_request.url(), request.url()))
        request.setURL(m_request.url());
    setRequest(request);
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& resourceError)
{
    RefPtr<DocumentLoader> protect(this);
    ResourceError error = resourceError.isNull() ? frameLoader()->cancelledError(m_request) : resourceError;

    m_dataLoadTimer.stop();
    if (mainResourceLoader())
        mainResourceLoader()->cancel(error);

    mainReceivedError(error);
}

void DocumentLoader::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    m_multipartResourceLoaders.add(loader);
    m_resourceLoaders.remove(loader);
    checkLoadComplete();
    if (Frame* frame = m_frame)
        frame->loader()->checkLoadComplete();    
}

void DocumentLoader::maybeFinishLoadingMultipartContent()
{
    if (!isMultipartReplacingLoad())
        return;

    frameLoader()->setupForReplace();
    m_committed = false;
    RefPtr<SharedBuffer> resourceData = mainResourceData();
    commitLoad(resourceData->data(), resourceData->size());
}

void DocumentLoader::handledOnloadEvents()
{
    m_wasOnloadHandled = true;
    applicationCacheHost()->stopDeferringEvents();
}

} // namespace WebCore

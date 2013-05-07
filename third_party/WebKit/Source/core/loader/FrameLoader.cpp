/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
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
#include "core/loader/FrameLoader.h"

#include "HTMLNames.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/BeforeUnloadEvent.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/dom/PageTransitionEvent.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/editing/Editor.h"
#include "core/history/BackForwardController.h"
#include "core/history/HistoryItem.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/PluginDocument.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameNetworkingContext.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/loader/cache/CachedResourceLoader.h"
#include "core/loader/cache/MemoryCache.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Console.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMWindow.h"
#include "core/page/EditorClient.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/SecurityOrigin.h"
#include "core/page/SecurityPolicy.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/Logging.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/SchemeRegistry.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/network/HTTPParsers.h"
#include "core/platform/network/ResourceHandle.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/text/SegmentedString.h"
#include "core/plugins/PluginData.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "modules/webdatabase/DatabaseManager.h"
#include <wtf/CurrentTime.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#include "core/svg/SVGDocument.h"
#include "core/svg/SVGLocatable.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGViewElement.h"
#include "core/svg/SVGViewSpec.h"
#endif


namespace WebCore {

using namespace HTMLNames;

#if ENABLE(SVG)
using namespace SVGNames;
#endif

static const char defaultAcceptHeader[] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

bool isBackForwardLoadType(FrameLoadType type)
{
    switch (type) {
        case FrameLoadTypeStandard:
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeRedirectWithLockedBackForwardList:
        case FrameLoadTypeReplace:
            return false;
        case FrameLoadTypeBack:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// This is not in the FrameLoader class to emphasize that it does not depend on
// private FrameLoader data, and to avoid increasing the number of public functions
// with access to private data.  Since only this .cpp file needs it, making it
// non-member lets us exclude it from the header file, thus keeping core/loader/FrameLoader.h's
// API simpler.
//
static bool isDocumentSandboxed(Frame* frame, SandboxFlags mask)
{
    return frame->document() && frame->document()->isSandboxed(mask);
}

class FrameLoader::FrameProgressTracker {
public:
    static PassOwnPtr<FrameProgressTracker> create(Frame* frame) { return adoptPtr(new FrameProgressTracker(frame)); }
    ~FrameProgressTracker()
    {
        ASSERT(!m_inProgress || m_frame->page());
        if (m_inProgress)
            m_frame->page()->progress()->progressCompleted(m_frame);
    }

    void progressStarted()
    {
        ASSERT(m_frame->page());
        if (!m_inProgress)
            m_frame->page()->progress()->progressStarted(m_frame);
        m_inProgress = true;
    }

    void progressCompleted()
    {
        ASSERT(m_inProgress);
        ASSERT(m_frame->page());
        m_inProgress = false;
        m_frame->page()->progress()->progressCompleted(m_frame);
    }

private:
    FrameProgressTracker(Frame* frame)
        : m_frame(frame)
        , m_inProgress(false)
    {
    }

    Frame* m_frame;
    bool m_inProgress;
};

FrameLoader::FrameLoader(Frame* frame, FrameLoaderClient* client)
    : m_frame(frame)
    , m_client(client)
    , m_history(frame)
    , m_notifer(frame)
    , m_subframeLoader(frame)
    , m_icon(frame)
    , m_mixedContentChecker(frame)
    , m_state(FrameStateProvisional)
    , m_loadType(FrameLoadTypeStandard)
    , m_delegateIsHandlingProvisionalLoadError(false)
    , m_quickRedirectComing(false)
    , m_inStopAllLoaders(false)
    , m_isExecutingJavaScriptFormAction(false)
    , m_didCallImplicitClose(true)
    , m_wasUnloadEventEmitted(false)
    , m_pageDismissalEventBeingDispatched(NoDismissal)
    , m_isComplete(false)
    , m_needsClear(false)
    , m_checkTimer(this, &FrameLoader::checkTimerFired)
    , m_shouldCallCheckCompleted(false)
    , m_shouldCallCheckLoadComplete(false)
    , m_opener(0)
    , m_didAccessInitialDocument(false)
    , m_didAccessInitialDocumentTimer(this, &FrameLoader::didAccessInitialDocumentTimerFired)
    , m_suppressOpenerInNewFrame(false)
    , m_forcedSandboxFlags(SandboxNone)
{
}

FrameLoader::~FrameLoader()
{
    setOpener(0);

    HashSet<Frame*>::iterator end = m_openedFrames.end();
    for (HashSet<Frame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->loader()->m_opener = 0;

    m_client->frameLoaderDestroyed();

    if (m_networkingContext)
        m_networkingContext->invalidate();
}

void FrameLoader::init()
{
    // This somewhat odd set of steps gives the frame an initial empty document.
    setPolicyDocumentLoader(m_client->createDocumentLoader(ResourceRequest(KURL(ParsedURLString, emptyString())), SubstituteData()).get());
    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_provisionalDocumentLoader->startLoadingMainResource();
    m_frame->document()->cancelParsing();
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);

    m_networkingContext = m_client->createNetworkingContext();
    m_progressTracker = FrameProgressTracker::create(m_frame);
}

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_documentLoader)
        m_documentLoader->setDefersLoading(defers);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->setDefersLoading(defers);
    if (m_policyDocumentLoader)
        m_policyDocumentLoader->setDefersLoading(defers);
    history()->setDefersLoading(defers);

    if (!defers) {
        m_frame->navigationScheduler()->startTimer();
        startCheckCompleteTimer();
    }
}

void FrameLoader::changeLocation(SecurityOrigin* securityOrigin, const KURL& url, const String& referrer, bool lockBackForwardList, bool refresh)
{
    urlSelected(FrameLoadRequest(securityOrigin, ResourceRequest(url, referrer, refresh ? ReloadIgnoringCacheData : UseProtocolCachePolicy), "_self"),
        0, lockBackForwardList, MaybeSendReferrer, ReplaceDocumentIfJavaScriptURL);
}

void FrameLoader::urlSelected(const KURL& url, const String& passedTarget, PassRefPtr<Event> triggeringEvent, bool lockBackForwardList, ShouldSendReferrer shouldSendReferrer)
{
    urlSelected(FrameLoadRequest(m_frame->document()->securityOrigin(), ResourceRequest(url), passedTarget),
        triggeringEvent, lockBackForwardList, shouldSendReferrer, DoNotReplaceDocumentIfJavaScriptURL);
}

// The shouldReplaceDocumentIfJavaScriptURL parameter will go away when the FIXME to eliminate the
// corresponding parameter from ScriptController::executeIfJavaScriptURL() is addressed.
void FrameLoader::urlSelected(const FrameLoadRequest& passedRequest, PassRefPtr<Event> triggeringEvent, bool lockBackForwardList, ShouldSendReferrer shouldSendReferrer, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL)
{
    ASSERT(!m_suppressOpenerInNewFrame);

    RefPtr<Frame> protect(m_frame);
    FrameLoadRequest frameRequest(passedRequest);

    if (m_frame->script()->executeIfJavaScriptURL(frameRequest.resourceRequest().url(), shouldReplaceDocumentIfJavaScriptURL))
        return;

    if (frameRequest.frameName().isEmpty())
        frameRequest.setFrameName(m_frame->document()->baseTarget());

    if (shouldSendReferrer == NeverSendReferrer)
        m_suppressOpenerInNewFrame = true;
    addHTTPOriginIfNeeded(frameRequest.resourceRequest(), outgoingOrigin());

    loadFrameRequest(frameRequest, lockBackForwardList, triggeringEvent, 0, shouldSendReferrer);

    m_suppressOpenerInNewFrame = false;
}

void FrameLoader::submitForm(PassRefPtr<FormSubmission> submission)
{
    ASSERT(submission->method() == FormSubmission::PostMethod || submission->method() == FormSubmission::GetMethod);

    // FIXME: Find a good spot for these.
    ASSERT(submission->data());
    ASSERT(submission->state());
    ASSERT(!submission->state()->sourceDocument()->frame() || submission->state()->sourceDocument()->frame() == m_frame);
    
    if (!m_frame->page())
        return;
    
    if (submission->action().isEmpty())
        return;

    if (isDocumentSandboxed(m_frame, SandboxForms)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        m_frame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked form submission to '" + submission->action().elidedString() + "' because the form's frame is sandboxed and the 'allow-forms' permission is not set.");
        return;
    }

    if (protocolIsJavaScript(submission->action())) {
        if (!m_frame->document()->contentSecurityPolicy()->allowFormAction(KURL(submission->action())))
            return;
        m_isExecutingJavaScriptFormAction = true;
        m_frame->script()->executeIfJavaScriptURL(submission->action(), DoNotReplaceDocumentIfJavaScriptURL);
        m_isExecutingJavaScriptFormAction = false;
        return;
    }

    Frame* targetFrame = findFrameForNavigation(submission->target(), submission->state()->sourceDocument());
    if (!targetFrame) {
        if (!DOMWindow::allowPopUp(m_frame) && !ScriptController::processingUserGesture())
            return;

        targetFrame = m_frame;
    } else
        submission->clearTarget();

    if (!targetFrame->page())
        return;

    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.

    // We do not want to submit more than one form from the same page, nor do we want to submit a single
    // form more than once. This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.

    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.

    if (m_frame->tree()->isDescendantOf(targetFrame)) {
        if (m_submittedFormURL == submission->requestURL())
            return;
        m_submittedFormURL = submission->requestURL();
    }

    submission->setReferrer(outgoingReferrer());
    submission->setOrigin(outgoingOrigin());

    targetFrame->navigationScheduler()->scheduleFormSubmission(submission);
}

void FrameLoader::stopLoading(UnloadEventPolicy unloadEventPolicy)
{
    if (m_frame->document() && m_frame->document()->parser())
        m_frame->document()->parser()->stopParsing();

    if (unloadEventPolicy != UnloadEventPolicyNone) {
        if (m_frame->document()) {
            if (m_didCallImplicitClose && !m_wasUnloadEventEmitted) {
                Node* currentFocusedNode = m_frame->document()->focusedNode();
                if (currentFocusedNode && currentFocusedNode->toInputElement())
                    currentFocusedNode->toInputElement()->endEditing();
                if (m_pageDismissalEventBeingDispatched == NoDismissal) {
                    if (unloadEventPolicy == UnloadEventPolicyUnloadAndPageHide) {
                        m_pageDismissalEventBeingDispatched = PageHideDismissal;
                        m_frame->document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), m_frame->document());
                    }
                    RefPtr<Event> unloadEvent(Event::create(eventNames().unloadEvent, false, false));
                    // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed
                    // while dispatching the event, so protect it to prevent writing the end
                    // time into freed memory.
                    RefPtr<DocumentLoader> documentLoader = m_provisionalDocumentLoader;
                    m_pageDismissalEventBeingDispatched = UnloadDismissal;
                    if (documentLoader && !documentLoader->timing()->unloadEventStart() && !documentLoader->timing()->unloadEventEnd()) {
                        DocumentLoadTiming* timing = documentLoader->timing();
                        ASSERT(timing->navigationStart());
                        timing->markUnloadEventStart();
                        m_frame->document()->domWindow()->dispatchEvent(unloadEvent, m_frame->document());
                        timing->markUnloadEventEnd();
                    } else
                        m_frame->document()->domWindow()->dispatchEvent(unloadEvent, m_frame->document());
                }
                m_pageDismissalEventBeingDispatched = NoDismissal;
                if (m_frame->document())
                    m_frame->document()->updateStyleIfNeeded();
                m_wasUnloadEventEmitted = true;
            }
        }

        // Dispatching the unload event could have made m_frame->document() null.
        if (m_frame->document()) {
            // Don't remove event listeners from a transitional empty document (see bug 28716 for more information).
            bool keepEventListeners = m_stateMachine.isDisplayingInitialEmptyDocument() && m_provisionalDocumentLoader
                && m_frame->document()->isSecureTransitionTo(m_provisionalDocumentLoader->url());

            if (!keepEventListeners)
                m_frame->document()->removeAllEventListeners();
        }
    }

    m_isComplete = true; // to avoid calling completed() in finishedParsing()
    m_didCallImplicitClose = true; // don't want that one either

    if (m_frame->document() && m_frame->document()->parsing()) {
        finishedParsing();
        m_frame->document()->setParsing(false);
    }

    if (Document* doc = m_frame->document()) {
        // FIXME: HTML5 doesn't tell us to set the state to complete when aborting, but we do anyway to match legacy behavior.
        // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10537
        doc->setReadyState(Document::Complete);

        // FIXME: Should the DatabaseManager watch for something like ActiveDOMObject::stop() rather than being special-cased here?
        DatabaseManager::manager().stopDatabases(doc, 0);
    }

    // FIXME: This will cancel redirection timer, which really needs to be restarted when restoring the frame from b/f cache.
    m_frame->navigationScheduler()->cancel();
}

void FrameLoader::stop()
{
    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it will be deleted by checkCompleted().
    RefPtr<Frame> protector(m_frame);

    if (DocumentParser* parser = m_frame->document()->parser()) {
        parser->stopParsing();
        parser->finish();
    }
}

bool FrameLoader::closeURL()
{
    history()->saveDocumentState();
    
    // Should only send the pagehide event here if the current document exists.
    Document* currentDocument = m_frame->document();
    stopLoading(currentDocument ? UnloadEventPolicyUnloadAndPageHide : UnloadEventPolicyUnloadOnly);
    
    m_frame->editor()->clearUndoRedoOperations();
    return true;
}

bool FrameLoader::didOpenURL()
{
    if (m_frame->navigationScheduler()->redirectScheduledDuringLoad()) {
        // A redirect was scheduled before the document was created.
        // This can happen when one frame changes another frame's location.
        return false;
    }

    m_frame->navigationScheduler()->cancel();
    m_frame->editor()->clearLastEditCommand();

    m_isComplete = false;
    m_didCallImplicitClose = false;

    // If we are still in the process of initializing an empty document then
    // its frame is not in a consistent state for rendering, so avoid setJSStatusBarText
    // since it may cause clients to attempt to render the frame.
    if (!m_stateMachine.creatingInitialEmptyDocument()) {
        DOMWindow* window = m_frame->document()->domWindow();
        window->setStatus(String());
        window->setDefaultStatus(String());
    }

    started();

    return true;
}

void FrameLoader::didExplicitOpen()
{
    m_isComplete = false;
    m_didCallImplicitClose = false;

    // Calling document.open counts as committing the first real document load.
    if (!m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    
    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call. 
    // Canceling redirection here works for all cases because document.open 
    // implicitly precedes document.write.
    m_frame->navigationScheduler()->cancel();
}


void FrameLoader::cancelAndClear()
{
    m_frame->navigationScheduler()->cancel();

    if (!m_isComplete)
        closeURL();

    clear(false);
}

void FrameLoader::clear(bool clearWindowProperties, bool clearScriptObjects, bool clearFrameView)
{
    m_frame->editor()->clear();

    if (!m_needsClear)
        return;
    m_needsClear = false;
    
    m_frame->document()->cancelParsing();
    m_frame->document()->stopActiveDOMObjects();
    if (m_frame->document()->attached()) {
        m_frame->document()->prepareForDestruction();
        m_frame->document()->removeFocusedNodeOfSubtree(m_frame->document());
    }

    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties) {
        InspectorInstrumentation::frameWindowDiscarded(m_frame, m_frame->document()->domWindow());
        m_frame->document()->domWindow()->reset();
        m_frame->script()->clearWindowShell();
    }

    m_frame->selection()->prepareForDestruction();
    m_frame->eventHandler()->clear();
    if (clearFrameView && m_frame->view())
        m_frame->view()->clear();

    // Do not drop the document before the ScriptController and view are cleared
    // as some destructors might still try to access the document.
    m_frame->setDocument(0);

    m_subframeLoader.clear();

    if (clearScriptObjects)
        m_frame->script()->clearScriptObjects();

    m_frame->script()->enableEval();

    m_frame->navigationScheduler()->clear();

    m_checkTimer.stop();
    m_shouldCallCheckCompleted = false;
    m_shouldCallCheckLoadComplete = false;
}

void FrameLoader::receivedFirstData()
{
    dispatchDidCommitLoad();
    dispatchDidClearWindowObjectsInAllWorlds();

    if (m_documentLoader) {
        StringWithDirection ptitle = m_documentLoader->title();
        // If we have a title let the WebView know about it.
        if (!ptitle.isNull())
            m_client->dispatchDidReceiveTitle(ptitle);
    }

    if (!m_documentLoader)
        return;
    if (m_frame->document()->isViewSource())
        return;

    double delay;
    String url;
    if (!parseHTTPRefresh(m_documentLoader->response().httpHeaderField("Refresh"), false, delay, url))
        return;
    if (url.isEmpty())
        url = m_frame->document()->url().string();
    else
        url = m_frame->document()->completeURL(url).string();

    m_frame->navigationScheduler()->scheduleRedirect(delay, url);
}

void FrameLoader::setOutgoingReferrer(const KURL& url)
{
    m_outgoingReferrer = url.strippedForUseAsReferrer();
}

void FrameLoader::didBeginDocument(bool dispatch)
{
    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_frame->document()->setReadyState(Document::Loading);

    if (history()->currentItem())
        m_frame->document()->statePopped(history()->currentItem()->stateObject());

    if (dispatch)
        dispatchDidClearWindowObjectsInAllWorlds();

    updateFirstPartyForCookies();
    m_frame->document()->initContentSecurityPolicy();

    Settings* settings = m_frame->document()->settings();
    if (settings) {
        m_frame->document()->cachedResourceLoader()->setImagesEnabled(settings->areImagesEnabled());
        m_frame->document()->cachedResourceLoader()->setAutoLoadImages(settings->loadsImagesAutomatically());
    }

    if (m_documentLoader) {
        String dnsPrefetchControl = m_documentLoader->response().httpHeaderField("X-DNS-Prefetch-Control");
        if (!dnsPrefetchControl.isEmpty())
            m_frame->document()->parseDNSPrefetchControlHeader(dnsPrefetchControl);

        String policyValue = m_documentLoader->response().httpHeaderField("Content-Security-Policy");
        if (!policyValue.isEmpty())
            m_frame->document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::Enforce);

        policyValue = m_documentLoader->response().httpHeaderField("Content-Security-Policy-Report-Only");
        if (!policyValue.isEmpty())
            m_frame->document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::Report);

        policyValue = m_documentLoader->response().httpHeaderField("X-WebKit-CSP");
        if (!policyValue.isEmpty())
            m_frame->document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::PrefixedEnforce);

        policyValue = m_documentLoader->response().httpHeaderField("X-WebKit-CSP-Report-Only");
        if (!policyValue.isEmpty())
            m_frame->document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::PrefixedReport);

        String headerContentLanguage = m_documentLoader->response().httpHeaderField("Content-Language");
        if (!headerContentLanguage.isEmpty()) {
            size_t commaIndex = headerContentLanguage.find(',');
            headerContentLanguage.truncate(commaIndex); // notFound == -1 == don't truncate
            headerContentLanguage = headerContentLanguage.stripWhiteSpace(isHTMLSpace);
            if (!headerContentLanguage.isEmpty())
                m_frame->document()->setContentLanguage(headerContentLanguage);
        }
    }

    history()->restoreDocumentState();
}

void FrameLoader::finishedParsing()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    // Null-checking the FrameView indicates whether or not we're in the destructor.
    RefPtr<Frame> protector = m_frame->view() ? m_frame : 0;

    m_client->dispatchDidFinishDocumentLoad();

    checkCompleted();

    if (!m_frame->view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame->view()->restoreScrollbar();
    scrollToFragmentWithParentBoundary(m_frame->document()->url());
}

void FrameLoader::loadDone()
{
    checkCompleted();
}

bool FrameLoader::allChildrenAreComplete() const
{
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!child->loader()->m_isComplete)
            return false;
    }
    return true;
}

bool FrameLoader::allAncestorsAreComplete() const
{
    for (Frame* ancestor = m_frame; ancestor; ancestor = ancestor->tree()->parent()) {
        if (!ancestor->loader()->m_isComplete)
            return false;
    }
    return true;
}

void FrameLoader::checkCompleted()
{
    RefPtr<Frame> protect(m_frame);
    m_shouldCallCheckCompleted = false;

    if (m_frame->view())
        m_frame->view()->handleLoadCompleted();

    // Have we completed before?
    if (m_isComplete)
        return;

    // Are we still parsing?
    if (m_frame->document()->parsing())
        return;

    // Still waiting for images/scripts?
    if (m_frame->document()->cachedResourceLoader()->requestCount())
        return;

    // Still waiting for elements that don't go through a FrameLoader?
    if (m_frame->document()->isDelayingLoadEvent())
        return;

    // Any frame that hasn't completed yet?
    if (!allChildrenAreComplete())
        return;

    // OK, completed.
    m_isComplete = true;
    m_requestedHistoryItem = 0;
    m_frame->document()->setReadyState(Document::Complete);

    checkCallImplicitClose(); // if we didn't do it before

    m_frame->navigationScheduler()->startTimer();

    completed();
    if (m_frame->page())
        checkLoadComplete();

    if (m_frame->view())
        m_frame->view()->handleLoadCompleted();
}

void FrameLoader::checkTimerFired(Timer<FrameLoader>*)
{
    RefPtr<Frame> protect(m_frame);

    if (Page* page = m_frame->page()) {
        if (page->defersLoading())
            return;
    }
    if (m_shouldCallCheckCompleted)
        checkCompleted();
    if (m_shouldCallCheckLoadComplete)
        checkLoadComplete();
}

void FrameLoader::startCheckCompleteTimer()
{
    if (!(m_shouldCallCheckCompleted || m_shouldCallCheckLoadComplete))
        return;
    if (m_checkTimer.isActive())
        return;
    m_checkTimer.startOneShot(0);
}

void FrameLoader::scheduleCheckCompleted()
{
    m_shouldCallCheckCompleted = true;
    startCheckCompleteTimer();
}

void FrameLoader::scheduleCheckLoadComplete()
{
    m_shouldCallCheckLoadComplete = true;
    startCheckCompleteTimer();
}

void FrameLoader::checkCallImplicitClose()
{
    if (m_didCallImplicitClose || m_frame->document()->parsing() || m_frame->document()->isDelayingLoadEvent())
        return;

    if (!allChildrenAreComplete())
        return; // still got a frame running -> too early

    m_didCallImplicitClose = true;
    m_wasUnloadEventEmitted = false;
    m_frame->document()->implicitClose();
}

void FrameLoader::loadURLIntoChildFrame(const KURL& url, const String& referer, Frame* childFrame)
{
    ASSERT(childFrame);

    HistoryItem* parentItem = history()->currentItem();
    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    if (parentItem && parentItem->children().size() && isBackForwardLoadType(loadType()) 
        && !m_frame->document()->loadEventFinished()) {
        HistoryItem* childItem = parentItem->childItemWithTarget(childFrame->tree()->uniqueName());
        if (childItem) {
            childFrame->loader()->loadDifferentDocumentItem(childItem, loadType(), MayAttemptCacheOnlyLoadForFormSubmissionItem);
            return;
        }
    }

    childFrame->loader()->loadURL(url, referer, "_self", FrameLoadTypeRedirectWithLockedBackForwardList, 0, 0);
}

ObjectContentType FrameLoader::defaultObjectContentType(const KURL& url, const String& mimeTypeIn, bool shouldPreferPlugInsForImages)
{
    String mimeType = mimeTypeIn;

    if (mimeType.isEmpty())
        mimeType = mimeTypeFromURL(url);

    if (mimeType.isEmpty())
        return ObjectContentFrame; // Go ahead and hope that we can display the content.

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return WebCore::ObjectContentImage;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return WebCore::ObjectContentFrame;

    return WebCore::ObjectContentNone;
}

String FrameLoader::outgoingReferrer() const
{
    // See http://www.whatwg.org/specs/web-apps/current-work/#fetching-resources
    // for why we walk the parent chain for srcdoc documents.
    Frame* frame = m_frame;
    while (frame->document()->isSrcdocDocument()) {
        frame = frame->tree()->parent();
        // Srcdoc documents cannot be top-level documents, by definition,
        // because they need to be contained in iframes with the srcdoc.
        ASSERT(frame);
    }
    return frame->loader()->m_outgoingReferrer;
}

String FrameLoader::outgoingOrigin() const
{
    return m_frame->document()->securityOrigin()->toString();
}

bool FrameLoader::checkIfFormActionAllowedByCSP(const KURL& url) const
{
    if (m_submittedFormURL.isEmpty())
        return true;

    return m_frame->document()->contentSecurityPolicy()->allowFormAction(url);
}

Frame* FrameLoader::opener()
{
    return m_opener;
}

void FrameLoader::setOpener(Frame* opener)
{
    if (m_opener && !opener)
        m_client->didDisownOpener();

    if (m_opener)
        m_opener->loader()->m_openedFrames.remove(m_frame);
    if (opener)
        opener->loader()->m_openedFrames.add(m_frame);
    m_opener = opener;

    if (m_frame->document())
        m_frame->document()->initSecurityContext();
}

// FIXME: This does not belong in FrameLoader!
void FrameLoader::handleFallbackContent()
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElement*>(owner)->renderFallbackContent();
}

void FrameLoader::provisionalLoadStarted()
{
    m_frame->navigationScheduler()->cancel();
    m_quickRedirectComing = false;
}

void FrameLoader::resetMultipleFormSubmissionProtection()
{
    m_submittedFormURL = KURL();
}

void FrameLoader::updateFirstPartyForCookies()
{
    if (m_frame->tree()->parent())
        setFirstPartyForCookies(m_frame->tree()->parent()->document()->firstPartyForCookies());
    else
        setFirstPartyForCookies(m_frame->document()->url());
}

void FrameLoader::setFirstPartyForCookies(const KURL& url)
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame))
        frame->document()->setFirstPartyForCookies(url);
}

// This does the same kind of work that didOpenURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void FrameLoader::loadInSameDocument(const KURL& url, PassRefPtr<SerializedScriptValue> stateObject, bool isNewNavigation)
{
    // If we have a state object, we cannot also be a new navigation.
    ASSERT(!stateObject || (stateObject && !isNewNavigation));

    // Update the data source's request with the new URL to fake the URL change
    KURL oldURL = m_frame->document()->url();
    m_frame->document()->setURL(url);
    setOutgoingReferrer(url);
    documentLoader()->replaceRequestURLForSameDocumentNavigation(url);
    if (isNewNavigation && !shouldTreatURLAsSameAsCurrent(url) && !stateObject) {
        // NB: must happen after replaceRequestURLForSameDocumentNavigation(), since we add 
        // based on the current request. Must also happen before we openURL and displace the 
        // scroll position, since adding the BF item will save away scroll state.
        
        // NB2: If we were loading a long, slow doc, and the user fragment navigated before
        // it was done, currItem is now set the that slow doc, and prevItem is whatever was
        // before it.  Adding the b/f item will bump the slow doc down to prevItem, even
        // though its load is not yet done.  I think this all works out OK, for one because
        // we have already saved away the scroll and doc state for the long slow load,
        // but it's not an obvious case.

        history()->updateBackForwardListForFragmentScroll();
    }
    
    bool hashChange = equalIgnoringFragmentIdentifier(url, oldURL) && url.fragmentIdentifier() != oldURL.fragmentIdentifier();
    
    history()->updateForSameDocumentNavigation();

    // If we were in the autoscroll/panScroll mode we want to stop it before following the link to the anchor
    if (hashChange)
        m_frame->eventHandler()->stopAutoscrollTimer();
    
    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    started();

    // We need to scroll to the fragment whether or not a hash change occurred, since
    // the user might have scrolled since the previous navigation.
    scrollToFragmentWithParentBoundary(url);
    
    m_isComplete = false;
    checkCompleted();

    if (isNewNavigation) {
        // This will clear previousItem from the rest of the frame tree that didn't
        // doing any loading. We need to make a pass on this now, since for fragment
        // navigation we'll not go through a real load and reach Completed state.
        checkLoadComplete();
    }

    m_client->dispatchDidNavigateWithinPage();

    m_frame->document()->statePopped(stateObject ? stateObject : SerializedScriptValue::nullValue());
    
    if (hashChange) {
        m_frame->document()->enqueueHashchangeEvent(oldURL, url);
        m_client->dispatchDidChangeLocationWithinPage();
    }
    
    // FrameLoaderClient::didFinishLoad() tells the internal load delegate the load finished with no error
    m_client->didFinishLoad();
}

bool FrameLoader::isComplete() const
{
    return m_isComplete;
}

void FrameLoader::completed()
{
    RefPtr<Frame> protect(m_frame);

    for (Frame* descendant = m_frame->tree()->traverseNext(m_frame); descendant; descendant = descendant->tree()->traverseNext(m_frame))
        descendant->navigationScheduler()->startTimer();

    if (Frame* parent = m_frame->tree()->parent())
        parent->loader()->checkCompleted();

    if (m_frame->view())
        m_frame->view()->maintainScrollPositionAtAnchor(0);
}

void FrameLoader::started()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->parent())
        frame->loader()->m_isComplete = false;
}

void FrameLoader::prepareForHistoryNavigation()
{
    // If there is no currentItem, but we still want to engage in 
    // history navigation we need to manufacture one, and update
    // the state machine of this frame to impersonate having
    // loaded it.
    RefPtr<HistoryItem> currentItem = history()->currentItem();
    if (!currentItem) {
        currentItem = HistoryItem::create();
        history()->setCurrentItem(currentItem.get());
        frame()->page()->backForward()->setCurrentItem(currentItem.get());

        ASSERT(stateMachine()->isDisplayingInitialEmptyDocument());
        stateMachine()->advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    }
}

void FrameLoader::prepareForLoadStart()
{
    m_progressTracker->progressStarted();
    m_client->dispatchDidStartProvisionalLoad();

    // Notify accessibility.
    if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache()) {
        AXObjectCache::AXLoadingEvent loadingEvent = loadType() == FrameLoadTypeReload ? AXObjectCache::AXLoadingReloaded : AXObjectCache::AXLoadingStarted;
        cache->frameLoadingEventNotification(m_frame, loadingEvent);
    }
}

void FrameLoader::setupForReplace()
{
    setState(FrameStateProvisional);
    m_provisionalDocumentLoader = m_documentLoader;
    m_documentLoader = 0;
    detachChildren();
}

void FrameLoader::loadFrameRequest(const FrameLoadRequest& request, bool lockBackForwardList,
    PassRefPtr<Event> event, PassRefPtr<FormState> formState, ShouldSendReferrer shouldSendReferrer)
{    
    // Protect frame from getting blown away inside dispatchBeforeLoadEvent in loadWithDocumentLoader.
    RefPtr<Frame> protect(m_frame);

    KURL url = request.resourceRequest().url();

    ASSERT(m_frame->document());
    if (!request.requester()->canDisplay(url)) {
        reportLocalLoadFailed(m_frame, url.elidedString());
        return;
    }

    String argsReferrer = request.resourceRequest().httpReferrer();
    if (argsReferrer.isEmpty())
        argsReferrer = outgoingReferrer();

    String referrer = SecurityPolicy::generateReferrerHeader(m_frame->document()->referrerPolicy(), url, argsReferrer);
    if (shouldSendReferrer == NeverSendReferrer)
        referrer = String();
    
    FrameLoadType loadType;
    if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
        loadType = FrameLoadTypeReload;
    else if (lockBackForwardList)
        loadType = FrameLoadTypeRedirectWithLockedBackForwardList;
    else
        loadType = FrameLoadTypeStandard;

    if (request.resourceRequest().httpMethod() == "POST")
        loadPostRequest(request.resourceRequest(), referrer, request.frameName(), loadType, event, formState.get());
    else
        loadURL(request.resourceRequest().url(), referrer, request.frameName(), loadType, event, formState.get());

    // FIXME: It's possible this targetFrame will not be the same frame that was targeted by the actual
    // load if frame names have changed.
    Frame* sourceFrame = formState ? formState->sourceDocument()->frame() : m_frame;
    if (!sourceFrame)
        sourceFrame = m_frame;
    Frame* targetFrame = sourceFrame->loader()->findFrameForNavigation(request.frameName());
    if (targetFrame && targetFrame != sourceFrame) {
        if (Page* page = targetFrame->page())
            page->chrome()->focus();
    }
}

void FrameLoader::loadURL(const KURL& newURL, const String& referrer, const String& frameName, FrameLoadType newLoadType,
    PassRefPtr<Event> event, PassRefPtr<FormState> prpFormState)
{
    if (m_inStopAllLoaders)
        return;

    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;
    
    ResourceRequest request(newURL);
    if (!referrer.isEmpty()) {
        request.setHTTPReferrer(referrer);
        RefPtr<SecurityOrigin> referrerOrigin = SecurityOrigin::createFromString(referrer);
        addHTTPOriginIfNeeded(request, referrerOrigin->toString());
    }
    if (newLoadType == FrameLoadTypeReload || newLoadType == FrameLoadTypeReloadFromOrigin)
        request.setCachePolicy(ReloadIgnoringCacheData);

    ASSERT(newLoadType != FrameLoadTypeSame);

    // The search for a target frame is done earlier in the case of form submission.
    Frame* targetFrame = isFormSubmission ? 0 : findFrameForNavigation(frameName);
    if (targetFrame && targetFrame != m_frame) {
        targetFrame->loader()->loadURL(newURL, referrer, "_self", newLoadType, event, formState.release());
        return;
    }

    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    NavigationAction action(request, newLoadType, isFormSubmission, event);

    if (!targetFrame && !frameName.isEmpty()) {
        checkNewWindowPolicyAndContinue(formState.release(), frameName, action);
        return;
    }

    bool sameURL = shouldTreatURLAsSameAsCurrent(newURL);
    loadWithNavigationAction(request, action, newLoadType, formState.release());
    // Example of this case are sites that reload the same URL with a different cookie
    // driving the generated content, or a master frame with links that drive a target
    // frame, where the user has clicked on the same link repeatedly.
    if (sameURL && newLoadType != FrameLoadTypeReload && newLoadType != FrameLoadTypeReloadFromOrigin)
        m_loadType = FrameLoadTypeSame;
}

SubstituteData FrameLoader::defaultSubstituteDataForURL(const KURL& url)
{
    if (!shouldTreatURLAsSrcdocDocument(url))
        return SubstituteData();
    String srcdoc = m_frame->ownerElement()->fastGetAttribute(srcdocAttr);
    ASSERT(!srcdoc.isNull());
    CString encodedSrcdoc = srcdoc.utf8();
    return SubstituteData(SharedBuffer::create(encodedSrcdoc.data(), encodedSrcdoc.length()), "text/html", "UTF-8", KURL());
}

void FrameLoader::load(const FrameLoadRequest& passedRequest)
{
    FrameLoadRequest request(passedRequest);

    if (m_inStopAllLoaders)
        return;

    if (!request.frameName().isEmpty()) {
        Frame* frame = findFrameForNavigation(request.frameName());
        if (frame && frame->loader() != this) {
            frame->loader()->load(request);
            return;
        }
    }

    if (!request.hasSubstituteData())
        request.setSubstituteData(defaultSubstituteDataForURL(request.resourceRequest().url()));

    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request.resourceRequest(), request.substituteData());
    load(loader.get());
}

void FrameLoader::loadWithNavigationAction(const ResourceRequest& request, const NavigationAction& action, FrameLoadType type, PassRefPtr<FormState> formState, const String& overrideEncoding)
{
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request, defaultSubstituteDataForURL(request.url()));
    loader->setTriggeringAction(action);

    if (!overrideEncoding.isEmpty())
        loader->setOverrideEncoding(overrideEncoding);
    else if (m_documentLoader)
        loader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    if (m_quickRedirectComing) {
        loader->setIsClientRedirect(true);
        m_quickRedirectComing = false;
    }

    loadWithDocumentLoader(loader.get(), type, formState);
}

void FrameLoader::load(DocumentLoader* newDocumentLoader)
{
    ResourceRequest& r = newDocumentLoader->request();
    FrameLoadType type;

    if (shouldTreatURLAsSameAsCurrent(newDocumentLoader->originalRequest().url())) {
        r.setCachePolicy(ReloadIgnoringCacheData);
        type = FrameLoadTypeSame;
    } else if (shouldTreatURLAsSameAsCurrent(newDocumentLoader->unreachableURL()) && m_loadType == FrameLoadTypeReload)
        type = FrameLoadTypeReload;
    else
        type = FrameLoadTypeStandard;

    if (m_documentLoader)
        newDocumentLoader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the history list, we treat it as a reload so the history list 
    // is appropriately maintained.
    //
    // FIXME: This seems like a dangerous overloading of the meaning of "FrameLoadTypeReload" ...
    // shouldn't a more explicit type of reload be defined, that means roughly 
    // "load without affecting history" ? 
    if (shouldReloadToHandleUnreachableURL(newDocumentLoader)) {
        // shouldReloadToHandleUnreachableURL() returns true only when the original load type is back-forward.
        // In this case we should save the document state now. Otherwise the state can be lost because load type is
        // changed and updateForBackForwardNavigation() will not be called when loading is committed.
        history()->saveDocumentAndScrollState();

        ASSERT(type == FrameLoadTypeStandard);
        type = FrameLoadTypeReload;
    }

    loadWithDocumentLoader(newDocumentLoader, type, 0);
}

void FrameLoader::loadWithDocumentLoader(DocumentLoader* loader, FrameLoadType type, PassRefPtr<FormState> prpFormState)
{
    // Retain because dispatchBeforeLoadEvent may release the last reference to it.
    RefPtr<Frame> protect(m_frame);

    ASSERT(m_client->hasWebView());

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT(m_frame->view());

    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    if (m_frame->document())
        m_previousURL = m_frame->document()->url();

    m_loadType = type;
    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;

    const KURL& newURL = loader->request().url();
    const String& httpMethod = loader->request().httpMethod();

    if (shouldPerformFragmentNavigation(isFormSubmission, httpMethod, type, newURL))
        checkNavigationPolicyAndContinueFragmentScroll(NavigationAction(loader->request(), type, isFormSubmission));
    else {
        if (Frame* parent = m_frame->tree()->parent())
            loader->setOverrideEncoding(parent->loader()->documentLoader()->overrideEncoding());

        setPolicyDocumentLoader(loader);
        if (loader->triggeringAction().isEmpty())
            loader->setTriggeringAction(NavigationAction(loader->request(), type, isFormSubmission));

        checkNavigationPolicyAndContinueLoad(formState);
    }
}

void FrameLoader::reportLocalLoadFailed(Frame* frame, const String& url)
{
    ASSERT(!url.isEmpty());
    if (!frame)
        return;

    frame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Not allowed to load local resource: " + url);
}

const ResourceRequest& FrameLoader::initialRequest() const
{
    return activeDocumentLoader()->originalRequest();
}

bool FrameLoader::willLoadMediaElementURL(KURL& url)
{
    ResourceRequest request(url);

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    notifier()->sendRemainingDelegateMessages(m_documentLoader.get(), identifier, ResourceResponse(url, String(), -1, String(), String()), 0, -1, -1, error);

    url = request.url();

    return error.isNull();
}

bool FrameLoader::shouldReloadToHandleUnreachableURL(DocumentLoader* docLoader)
{
    KURL unreachableURL = docLoader->unreachableURL();

    if (unreachableURL.isEmpty())
        return false;

    if (!isBackForwardLoadType(m_loadType))
        return false;

    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    DocumentLoader* compareDocumentLoader = 0;
    if (m_delegateIsHandlingProvisionalLoadError)
        compareDocumentLoader = m_provisionalDocumentLoader.get();

    return compareDocumentLoader && unreachableURL == compareDocumentLoader->request().url();
}

void FrameLoader::reload(bool endToEndReload, const KURL& overrideURL, const String& overrideEncoding)
{
    if (!m_documentLoader)
        return;

    frame()->loader()->history()->saveDocumentAndScrollState();
    ResourceRequest request = m_documentLoader->request();
    if (!overrideURL.isEmpty())
        request.setURL(overrideURL);
    else if (!m_documentLoader->unreachableURL().isEmpty())
        request.setURL(m_documentLoader->unreachableURL());

    bool isFormSubmission = request.httpMethod() == "POST";
    if (overrideEncoding.isEmpty())
        request.setCachePolicy(ReloadIgnoringCacheData);
    else if (isFormSubmission)
        request.setCachePolicy(ReturnCacheDataDontLoad);
    else
        request.setCachePolicy(ReturnCacheDataElseLoad);

    FrameLoadType type = endToEndReload ? FrameLoadTypeReloadFromOrigin : FrameLoadTypeReload;
    NavigationAction action(request, type, isFormSubmission);
    loadWithNavigationAction(request, action, type, 0, overrideEncoding);
}

void FrameLoader::stopAllLoaders(ClearProvisionalItemPolicy clearProvisionalItemPolicy)
{
    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;
    
    // Calling stopLoading() on the provisional document loader can blow away
    // the frame from underneath.
    RefPtr<Frame> protect(m_frame);

    m_inStopAllLoaders = true;

    // If no new load is in progress, we should clear the provisional item from history
    // before we call stopLoading.
    if (clearProvisionalItemPolicy == ShouldClearProvisionalItem)
        history()->setProvisionalItem(0);

    for (RefPtr<Frame> child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->stopAllLoaders(clearProvisionalItemPolicy);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();

    setProvisionalDocumentLoader(0);

    m_checkTimer.stop();

    m_inStopAllLoaders = false;    
}

void FrameLoader::stopForUserCancel(bool deferCheckLoadComplete)
{
    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);
    stopAllLoaders();
    
    if (deferCheckLoadComplete)
        scheduleCheckLoadComplete();
    else if (m_frame->page())
        checkLoadComplete();
}

DocumentLoader* FrameLoader::activeDocumentLoader() const
{
    if (m_state == FrameStateProvisional)
        return m_provisionalDocumentLoader.get();
    return m_documentLoader.get();
}

void FrameLoader::didAccessInitialDocument()
{
    // We only need to notify the client once, and only for the main frame.
    if (isLoadingMainFrame() && !m_didAccessInitialDocument) {
        m_didAccessInitialDocument = true;
        // Notify asynchronously, since this is called within a JavaScript security check.
        m_didAccessInitialDocumentTimer.startOneShot(0);
    }
}

void FrameLoader::didAccessInitialDocumentTimerFired(Timer<FrameLoader>*)
{
    m_client->didAccessInitialDocument();
}

bool FrameLoader::isLoading() const
{
    DocumentLoader* docLoader = activeDocumentLoader();
    if (!docLoader)
        return false;
    return docLoader->isLoading();
}

bool FrameLoader::frameHasLoaded() const
{
    return m_stateMachine.committedFirstRealDocumentLoad() || (m_provisionalDocumentLoader && !m_stateMachine.creatingInitialEmptyDocument()); 
}

void FrameLoader::setDocumentLoader(DocumentLoader* loader)
{
    if (!loader && !m_documentLoader)
        return;
    
    ASSERT(loader != m_documentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    detachChildren();

    // detachChildren() can trigger this frame's unload event, and therefore
    // script can run and do just about anything. For example, an unload event that calls
    // document.write("") on its parent frame can lead to a recursive detachChildren()
    // invocation for this frame. In that case, we can end up at this point with a
    // loader that hasn't been deleted but has been detached from its frame. Such a
    // DocumentLoader has been sufficiently detached that we'll end up in an inconsistent
    // state if we try to use it.
    if (loader && !loader->frame())
        return;

    if (m_documentLoader)
        m_documentLoader->detachFromFrame();

    m_documentLoader = loader;
}

void FrameLoader::setPolicyDocumentLoader(DocumentLoader* loader)
{
    if (m_policyDocumentLoader == loader)
        return;

    ASSERT(m_frame);
    if (loader)
        loader->setFrame(m_frame);
    if (m_policyDocumentLoader
            && m_policyDocumentLoader != m_provisionalDocumentLoader
            && m_policyDocumentLoader != m_documentLoader)
        m_policyDocumentLoader->detachFromFrame();

    m_policyDocumentLoader = loader;
}

void FrameLoader::setProvisionalDocumentLoader(DocumentLoader* loader)
{
    ASSERT(!loader || !m_provisionalDocumentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    if (m_provisionalDocumentLoader && m_provisionalDocumentLoader != m_documentLoader)
        m_provisionalDocumentLoader->detachFromFrame();

    m_provisionalDocumentLoader = loader;
}

void FrameLoader::setState(FrameState newState)
{    
    m_state = newState;
    
    if (newState == FrameStateProvisional)
        provisionalLoadStarted();
    else if (newState == FrameStateComplete)
        frameLoadCompleted();
}

void FrameLoader::clearProvisionalLoad()
{
    setProvisionalDocumentLoader(0);
    m_progressTracker->progressCompleted();
    setState(FrameStateComplete);
}

void FrameLoader::commitProvisionalLoad()
{
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
    RefPtr<Frame> protect(m_frame);

    LOG(Loading, "WebCoreLoading %s: About to commit provisional load from previous URL '%s' to new URL '%s'", m_frame->tree()->uniqueName().string().utf8().data(),
        m_frame->document() ? m_frame->document()->url().elidedString().utf8().data() : "",
        pdl ? pdl->url().elidedString().utf8().data() : "<no provisional DocumentLoader>");

    if (m_loadType != FrameLoadTypeReplace)
        closeOldDataSources();

    transitionToCommitted();

    if (pdl && m_documentLoader) {
        // Check if the destination page is allowed to access the previous page's timing information.
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(pdl->request().url());
        m_documentLoader->timing()->setHasSameOriginAsPreviousDocument(securityOrigin->canRequest(m_previousURL));
    }

    // Call clientRedirectCancelledOrFinished() here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.
    if (pdl->isClientRedirect())
        clientRedirectCancelledOrFinished();
    
    didOpenURL();

    LOG(Loading, "WebCoreLoading %s: Finished committing provisional load to URL %s", m_frame->tree()->uniqueName().string().utf8().data(),
        m_frame->document() ? m_frame->document()->url().elidedString().utf8().data() : "");

    if (m_loadType == FrameLoadTypeStandard && m_documentLoader->isClientRedirect())
        history()->updateForClientRedirect();
}

void FrameLoader::transitionToCommitted()
{
    ASSERT(m_client->hasWebView());
    ASSERT(m_state == FrameStateProvisional);

    if (m_state != FrameStateProvisional)
        return;

    if (FrameView* view = m_frame->view()) {
        if (ScrollAnimator* scrollAnimator = view->existingScrollAnimator())
            scrollAnimator->cancelAnimations();
    }

    // The call to closeURL() invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    DocumentLoader* pdl = m_provisionalDocumentLoader.get();
    if (m_documentLoader)
        closeURL();
    if (pdl != m_provisionalDocumentLoader)
        return;

    // Nothing else can interupt this commit - set the Provisional->Committed transition in stone
    if (m_documentLoader)
        m_documentLoader->stopLoadingSubresources();

    setDocumentLoader(m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(0);

    if (pdl != m_documentLoader) {
        ASSERT(m_state == FrameStateComplete);
        return;
    }

    setState(FrameStateCommittedPage);

    if (isLoadingMainFrame())
        m_frame->page()->chrome()->client()->needTouchEvents(false);

    history()->updateForCommit();
    m_client->transitionToCommittedForNewPage();
}

void FrameLoader::clientRedirectCancelledOrFinished()
{
    m_client->dispatchDidCancelClientRedirect();
}

void FrameLoader::clientRedirected(const KURL& url, double seconds, double fireDate, bool lockBackForwardList)
{
    m_client->dispatchWillPerformClientRedirect(url, seconds, fireDate);

    // If a "quick" redirect comes in, we set a special mode so we treat the next
    // load as part of the original navigation. If we don't have a document loader, we have
    // no "original" load on which to base a redirect, so we treat the redirect as a normal load.
    // Loads triggered by JavaScript form submissions never count as quick redirects.
    m_quickRedirectComing = (lockBackForwardList || history()->currentItemShouldBeReplaced()) && m_documentLoader && !m_isExecutingJavaScriptFormAction;
}

bool FrameLoader::shouldReload(const KURL& currentURL, const KURL& destinationURL)
{
    // This function implements the rule: "Don't reload if navigating by fragment within
    // the same URL, but do reload if going to a new URL or to the same URL with no
    // fragment identifier at all."
    if (!destinationURL.hasFragmentIdentifier())
        return true;
    return !equalIgnoringFragmentIdentifier(currentURL, destinationURL);
}

void FrameLoader::closeOldDataSources()
{
    // FIXME: Is it important for this traversal to be postorder instead of preorder?
    // If so, add helpers for postorder traversal, and use them. If not, then lets not
    // use a recursive algorithm here.
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->closeOldDataSources();
    
    if (m_documentLoader)
        m_client->dispatchWillClose();
}

bool FrameLoader::isHostedByObjectElement() const
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    return owner && owner->hasTagName(objectTag);
}

bool FrameLoader::isLoadingMainFrame() const
{
    Page* page = m_frame->page();
    return page && m_frame == page->mainFrame();
}

bool FrameLoader::isReplacing() const
{
    return m_loadType == FrameLoadTypeReplace;
}

void FrameLoader::setReplacing()
{
    m_loadType = FrameLoadTypeReplace;
}

bool FrameLoader::subframeIsLoading() const
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (Frame* child = m_frame->tree()->lastChild(); child; child = child->tree()->previousSibling()) {
        FrameLoader* childLoader = child->loader();
        DocumentLoader* documentLoader = childLoader->documentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader->provisionalDocumentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader->policyDocumentLoader();
        if (documentLoader)
            return true;
    }
    return false;
}

FrameLoadType FrameLoader::loadType() const
{
    return m_loadType;
}
    
CachePolicy FrameLoader::subresourceCachePolicy() const
{
    if (m_isComplete)
        return CachePolicyVerify;

    if (m_loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    if (Frame* parentFrame = m_frame->tree()->parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader()->subresourceCachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }
    
    if (m_loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    const ResourceRequest& request(documentLoader()->request());

    if (request.cachePolicy() == ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;

    return CachePolicyVerify;
}

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client->hasWebView());

    switch (m_state) {
        case FrameStateProvisional: {
            if (m_delegateIsHandlingProvisionalLoadError)
                return;

            RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
            if (!pdl)
                return;
                
            // If we've received any errors we may be stuck in the provisional state and actually complete.
            const ResourceError& error = pdl->mainDocumentError();
            if (error.isNull())
                return;

            // Check all children first.
            RefPtr<HistoryItem> item;
            if (Page* page = m_frame->page())
                if (isBackForwardLoadType(loadType()))
                    // Reset the back forward list to the last committed history item at the top level.
                    item = page->mainFrame()->loader()->history()->currentItem();
                
            // Only reset if we aren't already going to a new provisional item.
            bool shouldReset = !history()->provisionalItem();
            if (!pdl->isLoadingInAPISense() || pdl->isStopping()) {
                m_delegateIsHandlingProvisionalLoadError = true;
                m_client->dispatchDidFailProvisionalLoad(error);
                m_delegateIsHandlingProvisionalLoadError = false;

                ASSERT(!pdl->isLoading());

                // If we're in the middle of loading multipart data, we need to restore the document loader.
                if (isReplacing() && !m_documentLoader.get())
                    setDocumentLoader(m_provisionalDocumentLoader.get());

                // Finish resetting the load state, but only if another load hasn't been started by the
                // delegate callback.
                if (pdl == m_provisionalDocumentLoader)
                    clearProvisionalLoad();
                else if (activeDocumentLoader()) {
                    KURL unreachableURL = activeDocumentLoader()->unreachableURL();
                    if (!unreachableURL.isEmpty() && unreachableURL == pdl->request().url())
                        shouldReset = false;
                }
            }
            if (shouldReset && item)
                if (Page* page = m_frame->page())
                    page->backForward()->setCurrentItem(item.get());
            return;
        }
        
        case FrameStateCommittedPage: {
            DocumentLoader* dl = m_documentLoader.get();            
            if (!dl || (dl->isLoadingInAPISense() && !dl->isStopping()))
                return;

            setState(FrameStateComplete);

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if (m_frame->page()) {
                if (isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadFromOrigin)
                    history()->restoreScrollPositionAndViewState();
            }

            if (m_stateMachine.creatingInitialEmptyDocument() || !m_stateMachine.committedFirstRealDocumentLoad())
                return;

            m_progressTracker->progressCompleted();
            if (Page* page = m_frame->page()) {
                if (m_frame == page->mainFrame())
                    page->resetRelevantPaintedObjectCounter();
            }

            const ResourceError& error = dl->mainDocumentError();

            AXObjectCache::AXLoadingEvent loadingEvent;
            if (!error.isNull()) {
                m_client->dispatchDidFailLoad(error);
                loadingEvent = AXObjectCache::AXLoadingFailed;
            } else {
                m_client->dispatchDidFinishLoad();
                loadingEvent = AXObjectCache::AXLoadingFinished;
            }

            // Notify accessibility.
            if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
                cache->frameLoadingEventNotification(m_frame, loadingEvent);

            return;
        }
        
        case FrameStateComplete:
            m_loadType = FrameLoadTypeStandard;
            frameLoadCompleted();
            return;
    }

    ASSERT_NOT_REACHED();
}

static KURL originatingURLFromBackForwardList(Page* page)
{
    // FIXME: Can this logic be replaced with m_frame->document()->firstPartyForCookies()?
    // It has the same meaning of "page a user thinks is the current one".

    KURL originalURL;
    int backCount = page->backForward()->backCount();
    for (int backIndex = 0; backIndex <= backCount; backIndex++) {
        // FIXME: At one point we had code here to check a "was user gesture" flag.
        // Do we need to restore that logic?
        HistoryItem* historyItem = page->backForward()->itemAtIndex(-backIndex);
        if (!historyItem)
            continue;

        originalURL = historyItem->originalURL(); 
        if (!originalURL.isNull()) 
            return originalURL;
    }

    return KURL();
}

void FrameLoader::setOriginalURLForDownloadRequest(ResourceRequest& request)
{
    KURL originalURL;
    
    // If there is no referrer, assume that the download was initiated directly, so current document is
    // completely unrelated to it. See <rdar://problem/5294691>.
    // FIXME: Referrer is not sent in many other cases, so we will often miss this important information.
    // Find a better way to decide whether the download was unrelated to current document.
    if (!request.httpReferrer().isNull()) {
        // find the first item in the history that was originated by the user
        originalURL = originatingURLFromBackForwardList(m_frame->page());
    }

    if (originalURL.isNull())
        originalURL = request.url();

    if (!originalURL.protocol().isEmpty() && !originalURL.host().isEmpty()) {
        unsigned port = originalURL.port();

        // Original URL is needed to show the user where a file was downloaded from. We should make a URL that won't result in downloading the file again.
        // FIXME: Using host-only URL is a very heavy-handed approach. We should attempt to provide the actual page where the download was initiated from, as a reminder to the user.
        String hostOnlyURLString;
        if (port)
            hostOnlyURLString = makeString(originalURL.protocol(), "://", originalURL.host(), ":", String::number(port));
        else
            hostOnlyURLString = makeString(originalURL.protocol(), "://", originalURL.host());

        // FIXME: Rename firstPartyForCookies back to mainDocumentURL. It was a mistake to think that it was only used for cookies.
        request.setFirstPartyForCookies(KURL(KURL(), hostOnlyURLString));
    }
}

void FrameLoader::didLayout(LayoutMilestones milestones)
{
    m_client->dispatchDidLayout(milestones);
}

void FrameLoader::didFirstLayout()
{
    if (m_frame->page() && isBackForwardLoadType(m_loadType))
        history()->restoreScrollPositionAndViewState();
}

void FrameLoader::frameLoadCompleted()
{
    // Note: Can be called multiple times.
    history()->updateForFrameLoadCompleted();
}

void FrameLoader::detachChildren()
{
    typedef Vector<RefPtr<Frame> > FrameVector;
    FrameVector childrenToDetach;
    childrenToDetach.reserveCapacity(m_frame->tree()->childCount());
    for (Frame* child = m_frame->tree()->lastChild(); child; child = child->tree()->previousSibling())
        childrenToDetach.append(child);
    FrameVector::iterator end = childrenToDetach.end();
    for (FrameVector::iterator it = childrenToDetach.begin(); it != end; it++)
        (*it)->loader()->detachFromParent();
}

void FrameLoader::closeAndRemoveChild(Frame* child)
{
    child->tree()->detachFromParent();

    child->setView(0);
    if (child->ownerElement() && child->page())
        child->page()->decrementSubframeCount();
    child->willDetachPage();
    child->detachFromPage();

    m_frame->tree()->removeChild(child);
}

// Called every time a resource is completely loaded or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT(m_client->hasWebView());
    
    m_shouldCallCheckLoadComplete = false;

    // FIXME: Always traversing the entire frame tree is a bit inefficient, but 
    // is currently needed in order to null out the previous history item for all frames.
    if (Page* page = m_frame->page()) {
        Vector<RefPtr<Frame>, 10> frames;
        for (RefPtr<Frame> frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frames.append(frame);
        // To process children before their parents, iterate the vector backwards.
        for (size_t i = frames.size(); i; --i)
            frames[i - 1]->loader()->checkLoadCompleteForThisFrame();
    }
}

int FrameLoader::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return m_frame->document()->cachedResourceLoader()->requestCount();

    int count = 0;
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame))
        count += frame->document()->cachedResourceLoader()->requestCount();
    return count;
}

String FrameLoader::userAgent(const KURL& url) const
{
    String userAgent = m_client->userAgent(url);
    InspectorInstrumentation::applyUserAgentOverride(m_frame, &userAgent);
    return userAgent;
}

void FrameLoader::handledOnloadEvents()
{
    m_client->dispatchDidHandleOnloadEvents();

    if (documentLoader())
        documentLoader()->handledOnloadEvents();
}

void FrameLoader::frameDetached()
{
    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);
    stopAllLoaders();
    m_frame->document()->stopActiveDOMObjects();
    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);

    closeURL();
    history()->saveScrollPositionAndViewStateToItem(history()->currentItem());
    detachChildren();
    // stopAllLoaders() needs to be called after detachChildren(), because detachedChildren()
    // will trigger the unload event handlers of any child frames, and those event
    // handlers might start a new subresource load in this frame.
    stopAllLoaders();

    InspectorInstrumentation::frameDetachedFromParent(m_frame);

    detachViewsAndDocumentLoader();

    m_progressTracker.clear();

    if (Frame* parent = m_frame->tree()->parent()) {
        parent->loader()->closeAndRemoveChild(m_frame);
        parent->loader()->scheduleCheckCompleted();
    } else {
        m_frame->setView(0);
        m_frame->willDetachPage();
        m_frame->detachFromPage();
    }
}

void FrameLoader::detachViewsAndDocumentLoader()
{
    setDocumentLoader(0);
    m_client->detachedFromParent();
}

void FrameLoader::addExtraFieldsToRequest(ResourceRequest& request)
{
    bool isMainResource = (request.targetType() == ResourceRequest::TargetIsMainFrame) || (request.targetType() == ResourceRequest::TargetIsSubframe);

    // Don't set the cookie policy URL if it's already been set.
    // But make sure to set it on all requests regardless of protocol, as it has significance beyond the cookie policy (<rdar://problem/6616664>).
    if (request.firstPartyForCookies().isEmpty()) {
        if (isMainResource && isLoadingMainFrame())
            request.setFirstPartyForCookies(request.url());
        else if (Document* document = m_frame->document())
            request.setFirstPartyForCookies(document->firstPartyForCookies());
    }

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    applyUserAgent(request);

    if (!isMainResource) {
        if (request.isConditional())
            request.setCachePolicy(ReloadIgnoringCacheData);
        else if (documentLoader()->isLoadingInAPISense()) {
            // If we inherit cache policy from a main resource, we use the DocumentLoader's
            // original request cache policy for two reasons:
            // 1. For POST requests, we mutate the cache policy for the main resource,
            //    but we do not want this to apply to subresources
            // 2. Delegates that modify the cache policy using willSendRequest: should
            //    not affect any other resources. Such changes need to be done
            //    per request.
            ResourceRequestCachePolicy mainDocumentOriginalCachePolicy = documentLoader()->originalRequest().cachePolicy();
            // Back-forward navigations try to load main resource from cache only to avoid re-submitting form data, and start over (with a warning dialog) if that fails.
            // This policy is set on initial request too, but should not be inherited.
            ResourceRequestCachePolicy subresourceCachePolicy = (mainDocumentOriginalCachePolicy == ReturnCacheDataDontLoad) ? ReturnCacheDataElseLoad : mainDocumentOriginalCachePolicy;
            request.setCachePolicy(subresourceCachePolicy);
        } else
            request.setCachePolicy(UseProtocolCachePolicy);

    // FIXME: Other FrameLoader functions have duplicated code for setting cache policy of main request when reloading.
    // It seems better to manage it explicitly than to hide the logic inside addExtraFieldsToRequest().
    } else if (m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadFromOrigin || request.isConditional())
        request.setCachePolicy(ReloadIgnoringCacheData);

    if (request.cachePolicy() == ReloadIgnoringCacheData) {
        if (m_loadType == FrameLoadTypeReload)
            request.setHTTPHeaderField("Cache-Control", "max-age=0");
        else if (m_loadType == FrameLoadTypeReloadFromOrigin) {
            request.setHTTPHeaderField("Cache-Control", "no-cache");
            request.setHTTPHeaderField("Pragma", "no-cache");
        }
    }
    
    if (isMainResource)
        request.setHTTPAccept(defaultAcceptHeader);

    // Make sure we send the Origin header.
    addHTTPOriginIfNeeded(request, String());
}

void FrameLoader::addHTTPOriginIfNeeded(ResourceRequest& request, const String& origin)
{
    if (!request.httpOrigin().isEmpty())
        return;  // Request already has an Origin header.

    // Don't send an Origin header for GET or HEAD to avoid privacy issues.
    // For example, if an intranet page has a hyperlink to an external web
    // site, we don't want to include the Origin of the request because it
    // will leak the internal host name. Similar privacy concerns have lead
    // to the widespread suppression of the Referer header at the network
    // layer.
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return;

    // For non-GET and non-HEAD methods, always send an Origin header so the
    // server knows we support this feature.

    if (origin.isEmpty()) {
        // If we don't know what origin header to attach, we attach the value
        // for an empty origin.
        request.setHTTPOrigin(SecurityOrigin::createUnique()->toString());
        return;
    }

    request.setHTTPOrigin(origin);
}

void FrameLoader::loadPostRequest(const ResourceRequest& inRequest, const String& referrer, const String& frameName, FrameLoadType loadType, PassRefPtr<Event> event, PassRefPtr<FormState> prpFormState)
{
    RefPtr<FormState> formState = prpFormState;

    // Previously when this method was reached, the original FrameLoadRequest had been deconstructed to build a 
    // bunch of parameters that would come in here and then be built back up to a ResourceRequest.  In case
    // any caller depends on the immutability of the original ResourceRequest, I'm rebuilding a ResourceRequest
    // from scratch as it did all along.
    const KURL& url = inRequest.url();
    RefPtr<FormData> formData = inRequest.httpBody();
    const String& contentType = inRequest.httpContentType();
    String origin = inRequest.httpOrigin();

    ResourceRequest workingResourceRequest(url);    

    if (!referrer.isEmpty())
        workingResourceRequest.setHTTPReferrer(referrer);
    workingResourceRequest.setHTTPOrigin(origin);
    workingResourceRequest.setHTTPMethod("POST");
    workingResourceRequest.setHTTPBody(formData);
    workingResourceRequest.setHTTPContentType(contentType);

    NavigationAction action(workingResourceRequest, loadType, true, event);

    if (!frameName.isEmpty()) {
        // The search for a target frame is done earlier in the case of form submission.
        if (Frame* targetFrame = formState ? 0 : findFrameForNavigation(frameName))
            targetFrame->loader()->loadWithNavigationAction(workingResourceRequest, action, loadType, formState.release());
        else
            checkNewWindowPolicyAndContinue(formState.release(), frameName, action);
    } else
        loadWithNavigationAction(workingResourceRequest, action, loadType, formState.release());
}

unsigned long FrameLoader::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    ASSERT(m_frame->document());
    String referrer = SecurityPolicy::generateReferrerHeader(m_frame->document()->referrerPolicy(), request.url(), outgoingReferrer());
    
    ResourceRequest initialRequest = request;
    initialRequest.setTimeoutInterval(10);
    
    if (!referrer.isEmpty())
        initialRequest.setHTTPReferrer(referrer);
    addHTTPOriginIfNeeded(initialRequest, outgoingOrigin());

    if (Page* page = m_frame->page())
        initialRequest.setFirstPartyForCookies(page->mainFrame()->loader()->documentLoader()->request().url());

    addExtraFieldsToRequest(initialRequest);

    unsigned long identifier = 0;    
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

    if (error.isNull()) {
        ASSERT(!newRequest.isNull());
        documentLoader()->applicationCacheHost()->willStartLoadingSynchronously(newRequest);
        ResourceHandle::loadResourceSynchronously(networkingContext(), newRequest, storedCredentials, error, response, data);
    }
    int encodedDataLength = response.resourceLoadInfo() ? static_cast<int>(response.resourceLoadInfo()->encodedDataLength) : -1;
    notifier()->sendRemainingDelegateMessages(m_documentLoader.get(), identifier, response, data.data(), data.size(), encodedDataLength, error);
    return identifier;
}

const ResourceRequest& FrameLoader::originalRequest() const
{
    return activeDocumentLoader()->originalRequestCopy();
}

void FrameLoader::receivedMainResourceError(const ResourceError& error)
{
    // Retain because the stop may release the last reference to it.
    RefPtr<Frame> protect(m_frame);

    RefPtr<DocumentLoader> loader = activeDocumentLoader();
    // FIXME: Don't want to do this if an entirely new load is going, so should check
    // that both data sources on the frame are either this or nil.
    stop();
    if (m_client->shouldFallBack(error))
        handleFallbackContent();

    if (m_state == FrameStateProvisional && m_provisionalDocumentLoader) {
        if (m_submittedFormURL == m_provisionalDocumentLoader->originalRequestCopy().url())
            m_submittedFormURL = KURL();
            
        // Call clientRedirectCancelledOrFinished here so that the frame load delegate is notified that the redirect's
        // status has changed, if there was a redirect.
        if (loader->isClientRedirect())
            clientRedirectCancelledOrFinished();
    }

    checkCompleted();
    if (m_frame->page())
        checkLoadComplete();
}

void FrameLoader::checkNavigationPolicyAndContinueFragmentScroll(const NavigationAction& action)
{
    m_documentLoader->setTriggeringAction(action);

    const ResourceRequest& request = action.resourceRequest();
    if (!m_documentLoader->shouldContinueForNavigationPolicy(request))
        return;

    // If we have a provisional request for a different document, a fragment scroll should cancel it.
    if (m_provisionalDocumentLoader && !equalIgnoringFragmentIdentifier(m_provisionalDocumentLoader->request().url(), request.url())) {
        m_provisionalDocumentLoader->stopLoading();
        setProvisionalDocumentLoader(0);
    }

    bool isRedirect = m_quickRedirectComing || m_loadType == FrameLoadTypeRedirectWithLockedBackForwardList;
    loadInSameDocument(request.url(), 0, !isRedirect);
}

bool FrameLoader::shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType loadType, const KURL& url)
{
    // We don't do this if we are submitting a form with method other than "GET", explicitly reloading,
    // currently displaying a frameset, or if the URL does not have a fragment.
    // These rules were originally based on what KHTML was doing in KHTMLPart::openURL.

    // FIXME: What about load types other than Standard and Reload?

    return (!isFormSubmission || equalIgnoringCase(httpMethod, "GET"))
        && loadType != FrameLoadTypeReload
        && loadType != FrameLoadTypeReloadFromOrigin
        && loadType != FrameLoadTypeSame
        && !shouldReload(m_frame->document()->url(), url)
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && !m_frame->document()->isFrameSet();
}

void FrameLoader::scrollToFragmentWithParentBoundary(const KURL& url)
{
    FrameView* view = m_frame->view();
    if (!view)
        return;

    // Leaking scroll position to a cross-origin ancestor would permit the so-called "framesniffing" attack.
    RefPtr<Frame> boundaryFrame(url.hasFragmentIdentifier() ? m_frame->document()->findUnsafeParentScrollPropagationBoundary() : 0);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(false);

    view->scrollToFragment(url);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(true);
}

bool FrameLoader::shouldClose()
{
    Page* page = m_frame->page();
    Chrome* chrome = page ? page->chrome() : 0;
    if (!chrome || !chrome->canRunBeforeUnloadConfirmPanel())
        return true;

    // Store all references to each subframe in advance since beforeunload's event handler may modify frame
    Vector<RefPtr<Frame> > targetFrames;
    targetFrames.append(m_frame);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->traverseNext(m_frame))
        targetFrames.append(child);

    bool shouldClose = false;
    {
        NavigationDisablerForBeforeUnload navigationDisabler;
        size_t i;

        for (i = 0; i < targetFrames.size(); i++) {
            if (!targetFrames[i]->tree()->isDescendantOf(m_frame))
                continue;
            if (!targetFrames[i]->loader()->fireBeforeUnloadEvent(chrome))
                break;
        }

        if (i == targetFrames.size())
            shouldClose = true;
    }

    if (!shouldClose)
        m_submittedFormURL = KURL();

    return shouldClose;
}

bool FrameLoader::fireBeforeUnloadEvent(Chrome* chrome)
{
    DOMWindow* domWindow = m_frame->document()->domWindow();
    if (!domWindow)
        return true;

    RefPtr<Document> document = m_frame->document();
    if (!document->body())
        return true;

    RefPtr<BeforeUnloadEvent> beforeUnloadEvent = BeforeUnloadEvent::create();
    m_pageDismissalEventBeingDispatched = BeforeUnloadDismissal;
    domWindow->dispatchEvent(beforeUnloadEvent.get(), domWindow->document());
    m_pageDismissalEventBeingDispatched = NoDismissal;

    if (!beforeUnloadEvent->defaultPrevented())
        document->defaultEventHandler(beforeUnloadEvent.get());
    if (beforeUnloadEvent->result().isNull())
        return true;

    String text = document->displayStringModifiedByEncoding(beforeUnloadEvent->result());
    return chrome->runBeforeUnloadConfirmPanel(text, m_frame);
}

void FrameLoader::checkNavigationPolicyAndContinueLoad(PassRefPtr<FormState> formState)
{
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, policyDataSource should still be set.
    ASSERT(m_policyDocumentLoader || !m_provisionalDocumentLoader->unreachableURL().isEmpty());

    // stopAllLoaders can detach the Frame, so protect it.
    RefPtr<Frame> protect(m_frame);

    bool isTargetItem = history()->provisionalItem() ? history()->provisionalItem()->isTargetItem() : false;

    bool shouldContinue = false;
    if (m_stateMachine.committedFirstRealDocumentLoad() || !m_frame->ownerElement()
        || m_frame->ownerElement()->dispatchBeforeLoadEvent(m_policyDocumentLoader->request().url().string())) {
        // We skip dispatching the beforeload event if we've already
        // committed a real document load because the event would leak
        // subsequent activity by the frame which the parent frame isn't
        // supposed to learn. For example, if the child frame navigated to
        // a new URL, the parent frame shouldn't learn the URL.
        shouldContinue = m_policyDocumentLoader->shouldContinueForNavigationPolicy(m_policyDocumentLoader->request());
    }

    // Two reasons we can't continue:
    //    1) Navigation policy delegate said we can't so request is nil. A primary case of this 
    //       is the user responding Cancel to the form repost nag sheet.
    //    2) User responded Cancel to an alert popped up by the before unload event handler.
    bool canContinue = shouldContinue && shouldClose();

    if (!canContinue) {
        // If we were waiting for a client redirect, but the policy delegate decided to ignore it, then we
        // need to report that the client redirect was cancelled.
        if (m_policyDocumentLoader->isClientRedirect())
            clientRedirectCancelledOrFinished();

        setPolicyDocumentLoader(0);

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back.  For sanity, 
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || isLoadingMainFrame()) && isBackForwardLoadType(m_loadType)) {
            if (Page* page = m_frame->page()) {
                Frame* mainFrame = page->mainFrame();
                if (HistoryItem* resetItem = mainFrame->loader()->history()->currentItem())
                    page->backForward()->setCurrentItem(resetItem);
            }
        }
        return;
    }

    // A new navigation is in progress, so don't clear the history's provisional item.
    stopAllLoaders(ShouldNotClearProvisionalItem);
    
    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load. 
    if (!m_frame->page())
        return;

    if (Page* page = m_frame->page()) {
        if (page->mainFrame() == m_frame)
            m_frame->page()->inspectorController()->resume();
    }

    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    setState(FrameStateProvisional);

    setPolicyDocumentLoader(0);

    if (formState)
        m_client->dispatchWillSubmitForm(formState);

    prepareForLoadStart();

    // The load might be cancelled inside of prepareForLoadStart(), nulling out the m_provisionalDocumentLoader,
    // so we need to null check it again.
    if (!m_provisionalDocumentLoader)
        return;

    DocumentLoader* activeDocLoader = activeDocumentLoader();
    if (activeDocLoader && activeDocLoader->isLoadingMainResource())
        return;

    m_provisionalDocumentLoader->startLoadingMainResource();
}

void FrameLoader::checkNewWindowPolicyAndContinue(PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& action)
{
    if (m_frame->document() && m_frame->document()->isSandboxed(SandboxPopups))
        return;

    if (!DOMWindow::allowPopUp(m_frame))
        return;

    PolicyAction policy = m_client->policyForNewWindowAction(action, frameName);
    ASSERT(policy != PolicyIgnore);
    if (policy == PolicyDownload) {
        m_client->startDownload(action.resourceRequest());
        return;
    }

    RefPtr<Frame> frame = m_frame;
    RefPtr<Frame> mainFrame = m_client->dispatchCreatePage(action);
    if (!mainFrame)
        return;

    if (frameName != "_blank")
        mainFrame->tree()->setName(frameName);

    mainFrame->page()->setOpenedByDOM();
    mainFrame->loader()->m_client->dispatchShow();
    if (!m_suppressOpenerInNewFrame) {
        mainFrame->loader()->setOpener(frame.get());
        mainFrame->document()->setReferrerPolicy(frame->document()->referrerPolicy());
    }

    // FIXME: We can't just send our NavigationAction to the new FrameLoader's loadWithNavigationAction(), we need to
    // create a new one with a default NavigationType and no triggering event. We should figure out why.
    mainFrame->loader()->loadWithNavigationAction(action.resourceRequest(), NavigationAction(action.resourceRequest()), FrameLoadTypeStandard, formState);
}

void FrameLoader::requestFromDelegate(ResourceRequest& request, unsigned long& identifier, ResourceError& error)
{
    ASSERT(!request.isNull());

    identifier = 0;
    if (Page* page = m_frame->page())
        identifier = createUniqueIdentifier();

    ResourceRequest newRequest(request);
    notifier()->dispatchWillSendRequest(m_documentLoader.get(), identifier, newRequest, ResourceResponse());

    if (newRequest.isNull())
        error = cancelledError(request);
    else
        error = ResourceError();

    request = newRequest;
}

void FrameLoader::loadedResourceFromMemoryCache(CachedResource* resource)
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (!resource->shouldSendResourceLoadCallbacks() || m_documentLoader->haveToldClientAboutLoad(resource->url()))
        return;

    // Main resource delegate messages are synthesized in MainResourceLoader, so we must not send them here.
    if (resource->type() == CachedResource::MainResource)
        return;

    if (!page->areMemoryCacheClientCallsEnabled()) {
        InspectorInstrumentation::didLoadResourceFromMemoryCache(page, m_documentLoader.get(), resource);
        m_documentLoader->recordMemoryCacheLoadForFutureClientNotification(resource->url());
        m_documentLoader->didTellClientAboutLoad(resource->url());
        return;
    }

    ResourceRequest request(resource->url());
    m_client->dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, resource->response(), resource->encodedSize());

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    InspectorInstrumentation::markResourceAsCached(page, identifier);
    notifier()->sendRemainingDelegateMessages(m_documentLoader.get(), identifier, resource->response(), 0, resource->encodedSize(), 0, error);
}

void FrameLoader::applyUserAgent(ResourceRequest& request)
{
    String userAgent = this->userAgent(request.url());
    ASSERT(!userAgent.isNull());
    request.setHTTPUserAgent(userAgent);
}

bool FrameLoader::shouldInterruptLoadForXFrameOptions(const String& content, const KURL& url, unsigned long requestIdentifier)
{
    UseCounter::count(m_frame->document(), UseCounter::XFrameOptions);

    Frame* topFrame = m_frame->tree()->top();
    if (m_frame == topFrame)
        return false;

    XFrameOptionsDisposition disposition = parseXFrameOptionsHeader(content);

    switch (disposition) {
    case XFrameOptionsSameOrigin: {
        UseCounter::count(m_frame->document(), UseCounter::XFrameOptionsSameOrigin);
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
        if (!origin->isSameSchemeHostPort(topFrame->document()->securityOrigin()))
            return true;
        for (Frame* frame = m_frame->tree()->parent(); frame; frame = frame->tree()->parent()) {
            if (!origin->isSameSchemeHostPort(frame->document()->securityOrigin())) {
                UseCounter::count(m_frame->document(), UseCounter::XFrameOptionsSameOriginWithBadAncestorChain);
                break;
            }
        }
        return false;
    }
    case XFrameOptionsDeny:
        return true;
    case XFrameOptionsAllowAll:
        return false;
    case XFrameOptionsConflict:
        m_frame->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Multiple 'X-Frame-Options' headers with conflicting values ('" + content + "') encountered when loading '" + url.elidedString() + "'. Falling back to 'DENY'.", requestIdentifier);
        return true;
    case XFrameOptionsInvalid:
        m_frame->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Invalid 'X-Frame-Options' header encountered when loading '" + url.elidedString() + "': '" + content + "' is not a recognized directive. The header will be ignored.", requestIdentifier);
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const KURL& url) const
{
    if (!history()->currentItem())
        return false;
    return url == history()->currentItem()->url() || url == history()->currentItem()->originalURL();
}

bool FrameLoader::shouldTreatURLAsSrcdocDocument(const KURL& url) const
{
    if (!equalIgnoringCase(url.string(), "about:srcdoc"))
        return false;
    HTMLFrameOwnerElement* ownerElement = m_frame->ownerElement();
    if (!ownerElement)
        return false;
    if (!ownerElement->hasTagName(iframeTag))
        return false;
    return ownerElement->fastHasAttribute(srcdocAttr);
}

Frame* FrameLoader::findFrameForNavigation(const AtomicString& name, Document* activeDocument)
{
    Frame* frame = m_frame->tree()->find(name);

    // From http://www.whatwg.org/specs/web-apps/current-work/#seamlessLinks:
    //
    // If the source browsing context is the same as the browsing context
    // being navigated, and this browsing context has its seamless browsing
    // context flag set, and the browsing context being navigated was not
    // chosen using an explicit self-navigation override, then find the
    // nearest ancestor browsing context that does not have its seamless
    // browsing context flag set, and continue these steps as if that
    // browsing context was the one that was going to be navigated instead.
    if (frame == m_frame && name != "_self" && m_frame->document()->shouldDisplaySeamlesslyWithParent()) {
        for (Frame* ancestor = m_frame; ancestor; ancestor = ancestor->tree()->parent()) {
            if (!ancestor->document()->shouldDisplaySeamlesslyWithParent()) {
                frame = ancestor;
                break;
            }
        }
        ASSERT(frame != m_frame);
    }

    if (activeDocument) {
        if (!activeDocument->canNavigate(frame))
            return 0;
    } else {
        // FIXME: Eventually all callers should supply the actual activeDocument
        // so we can call canNavigate with the right document.
        if (!m_frame->document()->canNavigate(frame))
            return 0;
    }

    return frame;
}

void FrameLoader::loadSameDocumentItem(HistoryItem* item)
{
    ASSERT(item->documentSequenceNumber() == history()->currentItem()->documentSequenceNumber());

    // Save user view state to the current history item here since we don't do a normal load.
    // FIXME: Does form state need to be saved here too?
    history()->saveScrollPositionAndViewStateToItem(history()->currentItem());
    if (FrameView* view = m_frame->view())
        view->setWasScrolledByUser(false);

    history()->setCurrentItem(item);
        
    // loadInSameDocument() actually changes the URL and notifies load delegates of a "fake" load
    loadInSameDocument(item->url(), item->stateObject(), false);

    // Restore user view state from the current history item here since we don't do a normal load.
    history()->restoreScrollPositionAndViewState();
}

// FIXME: This function should really be split into a couple pieces, some of
// which should be methods of HistoryController and some of which should be
// methods of FrameLoader.
void FrameLoader::loadDifferentDocumentItem(HistoryItem* item, FrameLoadType loadType, FormSubmissionCacheLoadPolicy cacheLoadPolicy)
{
    // Remember this item so we can traverse any child items as child frames load
    history()->setProvisionalItem(item);

    KURL itemURL = item->url();
    KURL itemOriginalURL = item->originalURL();
    KURL currentURL;
    if (documentLoader())
        currentURL = documentLoader()->url();
    RefPtr<FormData> formData = item->formData();

    ResourceRequest request(itemURL);

    if (!item->referrer().isNull())
        request.setHTTPReferrer(item->referrer());
    
    // If this was a repost that failed the page cache, we might try to repost the form.
    NavigationAction action;
    if (formData) {
        request.setHTTPMethod("POST");
        request.setHTTPBody(formData);
        request.setHTTPContentType(item->formContentType());
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::createFromString(item->referrer());
        addHTTPOriginIfNeeded(request, securityOrigin->toString());
        
        // FIXME: Slight hack to test if the NSURL cache contains the page we're going to.
        // We want to know this before talking to the policy delegate, since it affects whether 
        // we show the DoYouReallyWantToRepost nag.
        //
        // This trick has a small bug (3123893) where we might find a cache hit, but then
        // have the item vanish when we try to use it in the ensuing nav.  This should be
        // extremely rare, but in that case the user will get an error on the navigation.
        
        if (cacheLoadPolicy == MayAttemptCacheOnlyLoadForFormSubmissionItem) {
            request.setCachePolicy(ReturnCacheDataDontLoad);
            action = NavigationAction(request, loadType, false);
        } else {
            request.setCachePolicy(ReturnCacheDataElseLoad);
            action = NavigationAction(request, NavigationTypeFormResubmitted);
        }
    } else {
        switch (loadType) {
            case FrameLoadTypeReload:
            case FrameLoadTypeReloadFromOrigin:
                request.setCachePolicy(ReloadIgnoringCacheData);
                break;
            case FrameLoadTypeBack:
            case FrameLoadTypeForward:
            case FrameLoadTypeIndexedBackForward:
                // If the first load within a frame is a navigation within a back/forward list that was attached 
                // without any of the items being loaded then we should use the default caching policy (<rdar://problem/8131355>).
                if (m_stateMachine.committedFirstRealDocumentLoad())
                    request.setCachePolicy(ReturnCacheDataElseLoad);
                break;
            case FrameLoadTypeStandard:
            case FrameLoadTypeRedirectWithLockedBackForwardList:
                break;
            case FrameLoadTypeSame:
            default:
                ASSERT_NOT_REACHED();
        }

        ResourceRequest requestForOriginalURL(request);
        requestForOriginalURL.setURL(itemOriginalURL);
        action = NavigationAction(requestForOriginalURL, loadType, false);
    }

    loadWithNavigationAction(request, action, loadType, 0);
}

// Loads content into this frame, as specified by history item
void FrameLoader::loadItem(HistoryItem* item, FrameLoadType loadType)
{
    m_requestedHistoryItem = item;
    HistoryItem* currentItem = history()->currentItem();
    bool sameDocumentNavigation = currentItem && item->shouldDoSameDocumentNavigationTo(currentItem);

    if (sameDocumentNavigation)
        loadSameDocumentItem(item);
    else
        loadDifferentDocumentItem(item, loadType, MayAttemptCacheOnlyLoadForFormSubmissionItem);
}

ResourceError FrameLoader::cancelledError(const ResourceRequest& request) const
{
    ResourceError error = m_client->cancelledError(request);
    error.setIsCancellation(true);
    return error;
}

void FrameLoader::setTitle(const StringWithDirection& title)
{
    documentLoader()->setTitle(title);
}

String FrameLoader::referrer() const
{
    return m_documentLoader ? m_documentLoader->request().httpReferrer() : "";
}

void FrameLoader::dispatchDocumentElementAvailable()
{
    m_client->documentElementAvailable();
}

void FrameLoader::dispatchDidClearWindowObjectsInAllWorlds()
{
    if (!m_frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;

    Vector<RefPtr<DOMWrapperWorld> > worlds;
    DOMWrapperWorld::getAllWorlds(worlds);
    for (size_t i = 0; i < worlds.size(); ++i)
        dispatchDidClearWindowObjectInWorld(worlds[i].get());
}

void FrameLoader::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (!m_frame->script()->canExecuteScripts(NotAboutToExecuteScript) || !m_frame->script()->existingWindowShell(world))
        return;

    m_client->dispatchDidClearWindowObjectInWorld(world);

    if (Page* page = m_frame->page())
        page->inspectorController()->didClearWindowObjectInWorld(m_frame, world);

    InspectorInstrumentation::didClearWindowObjectInWorld(m_frame, world);
}

SandboxFlags FrameLoader::effectiveSandboxFlags() const
{
    SandboxFlags flags = m_forcedSandboxFlags;
    if (Frame* parentFrame = m_frame->tree()->parent())
        flags |= parentFrame->document()->sandboxFlags();
    if (HTMLFrameOwnerElement* ownerElement = m_frame->ownerElement())
        flags |= ownerElement->sandboxFlags();
    return flags;
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    if (loader == m_documentLoader) {
        // Must update the entries in the back-forward list too.
        history()->setCurrentItemTitle(loader->title());
        m_client->dispatchDidReceiveTitle(loader->title());
    }
}

void FrameLoader::didChangeIcons(IconType type)
{
    m_client->dispatchDidChangeIcons(type);
}

void FrameLoader::dispatchDidCommitLoad()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    m_client->dispatchDidCommitLoad();

    InspectorInstrumentation::didCommitLoad(m_frame, m_documentLoader.get());

    if (m_frame->page()->mainFrame() == m_frame)
        m_frame->page()->useCounter()->didCommitLoad();

}

void FrameLoader::tellClientAboutPastMemoryCacheLoads()
{
    ASSERT(m_frame->page());
    ASSERT(m_frame->page()->areMemoryCacheClientCallsEnabled());

    if (!m_documentLoader)
        return;

    Vector<String> pastLoads;
    m_documentLoader->takeMemoryCacheLoadsForClientNotification(pastLoads);

    size_t size = pastLoads.size();
    for (size_t i = 0; i < size; ++i) {
        CachedResource* resource = memoryCache()->resourceForURL(KURL(ParsedURLString, pastLoads[i]));

        // FIXME: These loads, loaded from cache, but now gone from the cache by the time
        // Page::setMemoryCacheClientCallsEnabled(true) is called, will not be seen by the client.
        // Consider if there's some efficient way of remembering enough to deliver this client call.
        // We have the URL, but not the rest of the response or the length.
        if (!resource)
            continue;

        ResourceRequest request(resource->url());
        m_client->dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, resource->response(), resource->encodedSize());
    }
}

NetworkingContext* FrameLoader::networkingContext() const
{
    return m_networkingContext.get();
}

void FrameLoader::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Loader);
    info.addMember(m_frame, "frame");
    info.ignoreMember(m_client);
    info.addMember(m_progressTracker, "progressTracker");
    info.addMember(m_documentLoader, "documentLoader");
    info.addMember(m_provisionalDocumentLoader, "provisionalDocumentLoader");
    info.addMember(m_policyDocumentLoader, "policyDocumentLoader");
    info.addMember(m_submittedFormURL, "submittedFormURL");
    info.addMember(m_checkTimer, "checkTimer");
    info.addMember(m_opener, "opener");
    info.addMember(m_openedFrames, "openedFrames");
    info.addMember(m_outgoingReferrer, "outgoingReferrer");
    info.addMember(m_networkingContext, "networkingContext");
    info.addMember(m_previousURL, "previousURL");
    info.addMember(m_requestedHistoryItem, "requestedHistoryItem");
}

Frame* createWindow(Frame* openerFrame, Frame* lookupFrame, const FrameLoadRequest& request, const WindowFeatures& features, bool& created)
{
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank") {
        if (Frame* frame = lookupFrame->loader()->findFrameForNavigation(request.frameName(), openerFrame->document())) {
            if (request.frameName() != "_self") {
                if (Page* page = frame->page())
                    page->chrome()->focus();
            }
            created = false;
            return frame;
        }
    }

    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (isDocumentSandboxed(openerFrame, SandboxPopups)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        openerFrame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked opening '" + request.resourceRequest().url().elidedString() + "' in a new window because the request was made in a sandboxed frame whose 'allow-popups' permission is not set.");
        return 0;
    }

    // FIXME: Setting the referrer should be the caller's responsibility.
    FrameLoadRequest requestWithReferrer = request;
    String referrer = SecurityPolicy::generateReferrerHeader(openerFrame->document()->referrerPolicy(), request.resourceRequest().url(), openerFrame->loader()->outgoingReferrer());
    if (!referrer.isEmpty())
        requestWithReferrer.resourceRequest().setHTTPReferrer(referrer);
    FrameLoader::addHTTPOriginIfNeeded(requestWithReferrer.resourceRequest(), openerFrame->loader()->outgoingOrigin());

    if (openerFrame->settings() && !openerFrame->settings()->supportsMultipleWindows()) {
        created = false;
        return openerFrame;
    }

    Page* oldPage = openerFrame->page();
    if (!oldPage)
        return 0;

    NavigationAction action(requestWithReferrer.resourceRequest());
    Page* page = oldPage->chrome()->createWindow(openerFrame, requestWithReferrer, features, action);
    if (!page)
        return 0;

    Frame* frame = page->mainFrame();

    frame->loader()->forceSandboxFlags(openerFrame->document()->sandboxFlags());

    if (request.frameName() != "_blank")
        frame->tree()->setName(request.frameName());

    page->chrome()->setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    page->chrome()->setStatusbarVisible(features.statusBarVisible);
    page->chrome()->setScrollbarsVisible(features.scrollbarsVisible);
    page->chrome()->setMenubarVisible(features.menuBarVisible);
    page->chrome()->setResizable(features.resizable);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height'
    // specify the size of the viewport. We can only resize the window, so adjust
    // for the difference between the window size and the viewport size.

    FloatRect windowRect = page->chrome()->windowRect();
    FloatSize viewportSize = page->chrome()->pageRect().size();

    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    if (features.widthSet)
        windowRect.setWidth(features.width + (windowRect.width() - viewportSize.width()));
    if (features.heightSet)
        windowRect.setHeight(features.height + (windowRect.height() - viewportSize.height()));

    // Ensure non-NaN values, minimum size as well as being within valid screen area.
    FloatRect newWindowRect = DOMWindow::adjustWindowRect(page, windowRect);

    page->chrome()->setWindowRect(newWindowRect);
    page->chrome()->show();

    created = true;
    return frame;
}

} // namespace WebCore

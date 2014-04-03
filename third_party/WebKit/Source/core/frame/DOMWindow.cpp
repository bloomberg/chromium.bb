/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/frame/DOMWindow.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSRuleList.h"
#include "core/css/DOMWindowCSS.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/StyleMedia.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/RequestAnimationFrameCallback.h"
#include "core/editing/Editor.h"
#include "core/events/DOMWindowEventQueue.h"
#include "core/events/EventListener.h"
#include "core/events/HashChangeEvent.h"
#include "core/events/MessageEvent.h"
#include "core/events/PageTransitionEvent.h"
#include "core/events/PopStateEvent.h"
#include "core/frame/BarProp.h"
#include "core/frame/Console.h"
#include "core/frame/DOMPoint.h"
#include "core/frame/DOMWindowLifecycleNotifier.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/History.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/Navigator.h"
#include "core/frame/PageConsole.h"
#include "core/frame/Screen.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/SinkDocument.h"
#include "core/loader/appcache/ApplicationCache.h"
#include "core/page/BackForwardClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/CreateWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/storage/Storage.h"
#include "core/storage/StorageArea.h"
#include "core/storage/StorageNamespace.h"
#include "core/timing/Performance.h"
#include "platform/PlatformScreen.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

using std::min;
using std::max;

namespace WebCore {

class PostMessageTimer FINAL : public SuspendableTimer {
public:
    PostMessageTimer(DOMWindow& window, PassRefPtr<SerializedScriptValue> message, const String& sourceOrigin, PassRefPtrWillBeRawPtr<DOMWindow> source, PassOwnPtr<MessagePortChannelArray> channels, SecurityOrigin* targetOrigin, PassRefPtr<ScriptCallStack> stackTrace, UserGestureToken* userGestureToken)
        : SuspendableTimer(window.document())
        , m_window(&window)
        , m_message(message)
        , m_origin(sourceOrigin)
        , m_source(source)
        , m_channels(channels)
        , m_targetOrigin(targetOrigin)
        , m_stackTrace(stackTrace)
        , m_userGestureToken(userGestureToken)
    {
    }

    PassRefPtrWillBeRawPtr<MessageEvent> event()
    {
        return MessageEvent::create(m_channels.release(), m_message, m_origin, String(), m_source.get());

    }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    ScriptCallStack* stackTrace() const { return m_stackTrace.get(); }
    UserGestureToken* userGestureToken() const { return m_userGestureToken.get(); }

private:
    virtual void fired() OVERRIDE
    {
        m_window->postMessageTimerFired(adoptPtr(this));
        // This object is deleted now.
    }

    RefPtrWillBePersistent<DOMWindow> m_window;
    RefPtr<SerializedScriptValue> m_message;
    String m_origin;
    RefPtrWillBePersistent<DOMWindow> m_source;
    OwnPtr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
    RefPtr<ScriptCallStack> m_stackTrace;
    RefPtr<UserGestureToken> m_userGestureToken;
};

static void disableSuddenTermination()
{
    blink::Platform::current()->suddenTerminationChanged(false);
}

static void enableSuddenTermination()
{
    blink::Platform::current()->suddenTerminationChanged(true);
}

typedef HashCountedSet<DOMWindow*> DOMWindowSet;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithUnloadEventListeners, ());
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithBeforeUnloadEventListeners, ());
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        disableSuddenTermination();
    set.add(domWindow);
}

static void removeUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void removeAllUnloadEventListeners(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void addBeforeUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        disableSuddenTermination();
    set.add(domWindow);
}

static void removeBeforeUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(DOMWindow* window)
{
    ASSERT_ARG(window, window);
    LocalFrame* frame = window->frame();
    if (!frame)
        return false;
    return frame->isMainFrame();
}

unsigned DOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<DOMWindow*>(this));
}

// This function:
// 1) Validates the pending changes are not changing any value to NaN; in that case keep original value.
// 2) Constrains the window rect to the minimum window size and no bigger than the float rect's dimensions.
// 3) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 4) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of the screen.
FloatRect DOMWindow::adjustWindowRect(LocalFrame& frame, const FloatRect& pendingChanges)
{
    FrameHost* host = frame.host();
    ASSERT(host);

    FloatRect screen = screenAvailableRect(frame.view());
    FloatRect window = host->chrome().windowRect();

    // Make sure we're in a valid state before adjusting dimensions.
    ASSERT(std::isfinite(screen.x()));
    ASSERT(std::isfinite(screen.y()));
    ASSERT(std::isfinite(screen.width()));
    ASSERT(std::isfinite(screen.height()));
    ASSERT(std::isfinite(window.x()));
    ASSERT(std::isfinite(window.y()));
    ASSERT(std::isfinite(window.width()));
    ASSERT(std::isfinite(window.height()));

    // Update window values if new requested values are not NaN.
    if (!std::isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!std::isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!std::isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!std::isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());

    FloatSize minimumSize = host->chrome().client().minimumWindowSize();
    // Let size 0 pass through, since that indicates default size, not minimum size.
    if (window.width())
        window.setWidth(min(max(minimumSize.width(), window.width()), screen.width()));
    if (window.height())
        window.setHeight(min(max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(max(screen.x(), min(window.x(), screen.maxX() - window.width())));
    window.setY(max(screen.y(), min(window.y(), screen.maxY() - window.height())));

    return window;
}

bool DOMWindow::allowPopUp(LocalFrame& firstFrame)
{
    if (UserGestureIndicator::processingUserGesture())
        return true;

    Settings* settings = firstFrame.settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

bool DOMWindow::allowPopUp()
{
    return m_frame && allowPopUp(*m_frame);
}

bool DOMWindow::canShowModalDialog(const LocalFrame* frame)
{
    if (!frame)
        return false;
    FrameHost* host = frame->host();
    if (!host)
        return false;
    return host->chrome().canRunModal();
}

bool DOMWindow::canShowModalDialogNow(const LocalFrame* frame)
{
    if (!frame)
        return false;
    FrameHost* host = frame->host();
    if (!host)
        return false;
    return host->chrome().canRunModalNow();
}

DOMWindow::DOMWindow(LocalFrame& frame)
    : FrameDestructionObserver(&frame)
    , m_shouldPrintWhenFinishedLoading(false)
{
    ScriptWrappable::init(this);
}

void DOMWindow::clearDocument()
{
    if (!m_document)
        return;

    if (m_document->isActive()) {
        // FIXME: We don't call willRemove here. Why is that OK?
        // This detach() call is also mostly redundant. Most of the calls to
        // this function come via DocumentLoader::createWriterFor, which
        // always detaches the previous Document first. Only XSLTProcessor
        // depends on this detach() call, so it seems like there's some room
        // for cleanup.
        m_document->detach();
    }

    // FIXME: This should be part of ActiveDOM Object shutdown
    clearEventQueue();

    m_document->clearDOMWindow();
    m_document = nullptr;
}

void DOMWindow::clearEventQueue()
{
    if (!m_eventQueue)
        return;
    m_eventQueue->close();
    m_eventQueue.clear();
}

PassRefPtr<Document> DOMWindow::createDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    RefPtr<Document> document;
    if (forceXHTML) {
        // This is a hack for XSLTProcessor. See XSLTProcessor::createDocumentFromSource().
        document = Document::create(init);
    } else {
        document = DOMImplementation::createDocument(mimeType, init, init.frame() ? init.frame()->inViewSourceMode() : false);
        if (document->isPluginDocument() && document->isSandboxed(SandboxPlugins))
            document = SinkDocument::create(init);
    }

    return document.release();
}

PassRefPtr<Document> DOMWindow::installNewDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    ASSERT(init.frame() == m_frame);

    clearDocument();

    m_document = createDocument(mimeType, init, forceXHTML);
    m_eventQueue = DOMWindowEventQueue::create(m_document.get());
    m_document->attach();

    if (!m_frame)
        return m_document;

    m_frame->script().updateDocument();
    m_document->updateViewportDescription();

    if (m_frame->page() && m_frame->view()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_frame->page()->scrollingCoordinator()) {
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_frame->view(), HorizontalScrollbar);
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_frame->view(), VerticalScrollbar);
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(m_frame->view());
        }
    }

    m_frame->selection().updateSecureKeyboardEntryIfActive();

    if (m_frame->isMainFrame()) {
        m_frame->notifyChromeClientWheelEventHandlerCountChanged();
        if (m_document->hasTouchEventHandlers())
            m_frame->host()->chrome().client().needTouchEvents(true);
    }

    return m_document;
}

EventQueue* DOMWindow::eventQueue() const
{
    return m_eventQueue.get();
}

void DOMWindow::enqueueWindowEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(this);
    m_eventQueue->enqueueEvent(event);
}

void DOMWindow::enqueueDocumentEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(m_document);
    m_eventQueue->enqueueEvent(event);
}

void DOMWindow::dispatchWindowLoadEvent()
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    dispatchLoadEvent();
}

void DOMWindow::documentWasClosed()
{
    dispatchWindowLoadEvent();
    enqueuePageshowEvent(PageshowEventNotPersisted);
    if (m_pendingStateObject)
        enqueuePopstateEvent(m_pendingStateObject.release());
}

void DOMWindow::enqueuePageshowEvent(PageshowEventPersistence persisted)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs to fire asynchronously.
    dispatchEvent(PageTransitionEvent::create(EventTypeNames::pageshow, persisted), m_document.get());
}

void DOMWindow::enqueueHashchangeEvent(const String& oldURL, const String& newURL)
{
    enqueueWindowEvent(HashChangeEvent::create(oldURL, newURL));
}

void DOMWindow::enqueuePopstateEvent(PassRefPtr<SerializedScriptValue> stateObject)
{
    if (!ContextFeatures::pushStateEnabled(document()))
        return;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36202 Popstate event needs to fire asynchronously
    dispatchEvent(PopStateEvent::create(stateObject, &history()));
}

void DOMWindow::statePopped(PassRefPtr<SerializedScriptValue> stateObject)
{
    if (!frame())
        return;

    // Per step 11 of section 6.5.9 (history traversal) of the HTML5 spec, we
    // defer firing of popstate until we're in the complete state.
    if (document()->isLoadCompleted())
        enqueuePopstateEvent(stateObject);
    else
        m_pendingStateObject = stateObject;
}

DOMWindow::~DOMWindow()
{
    ASSERT(!m_screen);
    ASSERT(!m_history);
    ASSERT(!m_locationbar);
    ASSERT(!m_menubar);
    ASSERT(!m_personalbar);
    ASSERT(!m_scrollbars);
    ASSERT(!m_statusbar);
    ASSERT(!m_toolbar);
    ASSERT(!m_console);
    ASSERT(!m_navigator);
    ASSERT(!m_performance);
    ASSERT(!m_location);
    ASSERT(!m_media);
    ASSERT(!m_sessionStorage);
    ASSERT(!m_localStorage);
    ASSERT(!m_applicationCache);

    reset();

    removeAllEventListeners();

    ASSERT(m_document->isStopped());
    clearDocument();
}

const AtomicString& DOMWindow::interfaceName() const
{
    return EventTargetNames::DOMWindow;
}

ExecutionContext* DOMWindow::executionContext() const
{
    return m_document.get();
}

DOMWindow* DOMWindow::toDOMWindow()
{
    return this;
}

PassRefPtrWillBeRawPtr<MediaQueryList> DOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* DOMWindow::page()
{
    return frame() ? frame()->page() : 0;
}

void DOMWindow::frameDestroyed()
{
    FrameDestructionObserver::frameDestroyed();
    reset();
}

void DOMWindow::willDetachFrameHost()
{
    InspectorInstrumentation::frameWindowDiscarded(m_frame, this);
}

void DOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDestroyGlobalObjectInFrame();
}

void DOMWindow::willDetachDocumentFromFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDetachGlobalObjectFromFrame();
}

void DOMWindow::registerProperty(DOMWindowProperty* property)
{
    m_properties.add(property);
}

void DOMWindow::unregisterProperty(DOMWindowProperty* property)
{
    m_properties.remove(property);
}

void DOMWindow::reset()
{
    willDestroyDocumentInFrame();
    resetDOMWindowProperties();
}

void DOMWindow::resetDOMWindowProperties()
{
    m_properties.clear();

    m_screen = nullptr;
    m_history = nullptr;
    m_locationbar = nullptr;
    m_menubar = nullptr;
    m_personalbar = nullptr;
    m_scrollbars = nullptr;
    m_statusbar = nullptr;
    m_toolbar = nullptr;
    m_console = nullptr;
    m_navigator = nullptr;
    m_performance = nullptr;
    m_location = nullptr;
    m_media = nullptr;
    m_sessionStorage = nullptr;
    m_localStorage = nullptr;
    m_applicationCache = nullptr;
}

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    return m_frame && m_frame->domWindow() == this && m_frame->host();
}

int DOMWindow::orientation() const
{
    ASSERT(RuntimeEnabledFeatures::orientationEventEnabled());

    UseCounter::count(document(), UseCounter::WindowOrientation);

    if (!m_frame)
        return 0;

    return m_frame->orientation();
}

Screen& DOMWindow::screen() const
{
    if (!m_screen)
        m_screen = Screen::create(m_frame);
    return *m_screen;
}

History& DOMWindow::history() const
{
    if (!m_history)
        m_history = History::create(m_frame);
    return *m_history;
}

BarProp& DOMWindow::locationbar() const
{
    UseCounter::count(document(), UseCounter::BarPropLocationbar);
    if (!m_locationbar)
        m_locationbar = BarProp::create(m_frame, BarProp::Locationbar);
    return *m_locationbar;
}

BarProp& DOMWindow::menubar() const
{
    UseCounter::count(document(), UseCounter::BarPropMenubar);
    if (!m_menubar)
        m_menubar = BarProp::create(m_frame, BarProp::Menubar);
    return *m_menubar;
}

BarProp& DOMWindow::personalbar() const
{
    UseCounter::count(document(), UseCounter::BarPropPersonalbar);
    if (!m_personalbar)
        m_personalbar = BarProp::create(m_frame, BarProp::Personalbar);
    return *m_personalbar;
}

BarProp& DOMWindow::scrollbars() const
{
    UseCounter::count(document(), UseCounter::BarPropScrollbars);
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(m_frame, BarProp::Scrollbars);
    return *m_scrollbars;
}

BarProp& DOMWindow::statusbar() const
{
    UseCounter::count(document(), UseCounter::BarPropStatusbar);
    if (!m_statusbar)
        m_statusbar = BarProp::create(m_frame, BarProp::Statusbar);
    return *m_statusbar;
}

BarProp& DOMWindow::toolbar() const
{
    UseCounter::count(document(), UseCounter::BarPropToolbar);
    if (!m_toolbar)
        m_toolbar = BarProp::create(m_frame, BarProp::Toolbar);
    return *m_toolbar;
}

Console& DOMWindow::console() const
{
    if (!m_console)
        m_console = Console::create(m_frame);
    return *m_console;
}

PageConsole* DOMWindow::pageConsole() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    return m_frame->host() ? &m_frame->host()->console() : 0;
}

ApplicationCache* DOMWindow::applicationCache() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_applicationCache)
        m_applicationCache = ApplicationCache::create(m_frame);
    return m_applicationCache.get();
}

Navigator& DOMWindow::navigator() const
{
    if (!m_navigator)
        m_navigator = Navigator::create(m_frame);
    return *m_navigator;
}

Performance& DOMWindow::performance() const
{
    if (!m_performance)
        m_performance = Performance::create(m_frame);
    return *m_performance;
}

Location& DOMWindow::location() const
{
    if (!m_location)
        m_location = Location::create(m_frame);
    return *m_location;
}

Storage* DOMWindow::sessionStorage(ExceptionState& exceptionState) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    Document* document = this->document();
    if (!document)
        return 0;

    String accessDeniedMessage = "Access is denied for this document.";
    if (!document->securityOrigin()->canAccessLocalStorage()) {
        if (document->isSandboxed(SandboxOrigin))
            exceptionState.throwSecurityError("The document is sandboxed and lacks the 'allow-same-origin' flag.");
        else if (document->url().protocolIs("data"))
            exceptionState.throwSecurityError("Storage is disabled inside 'data:' URLs.");
        else
            exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    if (m_sessionStorage) {
        if (!m_sessionStorage->area()->canAccessStorage(m_frame)) {
            exceptionState.throwSecurityError(accessDeniedMessage);
            return 0;
        }
        return m_sessionStorage.get();
    }

    Page* page = document->page();
    if (!page)
        return 0;

    OwnPtrWillBeRawPtr<StorageArea> storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
    if (!storageArea->canAccessStorage(m_frame)) {
        exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    m_sessionStorage = Storage::create(m_frame, storageArea.release());
    return m_sessionStorage.get();
}

Storage* DOMWindow::localStorage(ExceptionState& exceptionState) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    Document* document = this->document();
    if (!document)
        return 0;

    String accessDeniedMessage = "Access is denied for this document.";
    if (!document->securityOrigin()->canAccessLocalStorage()) {
        if (document->isSandboxed(SandboxOrigin))
            exceptionState.throwSecurityError("The document is sandboxed and lacks the 'allow-same-origin' flag.");
        else if (document->url().protocolIs("data"))
            exceptionState.throwSecurityError("Storage is disabled inside 'data:' URLs.");
        else
            exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    if (m_localStorage) {
        if (!m_localStorage->area()->canAccessStorage(m_frame)) {
            exceptionState.throwSecurityError(accessDeniedMessage);
            return 0;
        }
        return m_localStorage.get();
    }

    // FIXME: Seems this check should be much higher?
    FrameHost* host = document->frameHost();
    if (!host || !host->settings().localStorageEnabled())
        return 0;

    OwnPtrWillBeRawPtr<StorageArea> storageArea = StorageNamespace::localStorageArea(document->securityOrigin());
    if (!storageArea->canAccessStorage(m_frame)) {
        exceptionState.throwSecurityError(accessDeniedMessage);
        return 0;
    }

    m_localStorage = Storage::create(m_frame, storageArea.release());
    return m_localStorage.get();
}

void DOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, const String& targetOrigin, DOMWindow* source, ExceptionState& exceptionState)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* sourceDocument = source->document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SyntaxError exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin == "/") {
        if (!sourceDocument)
            return;
        target = sourceDocument->securityOrigin();
    } else if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at a unique origin
        // because there's no way to represent a unique origin in a string.
        if (target->isUnique()) {
            exceptionState.throwDOMException(SyntaxError, "Invalid target origin '" + targetOrigin + "' in a call to 'postMessage'.");
            return;
        }
    }

    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    if (!sourceDocument)
        return;
    String sourceOrigin = sourceDocument->securityOrigin()->toString();

    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtr<ScriptCallStack> stackTrace;
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
        stackTrace = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);

    // Schedule the message.
    PostMessageTimer* timer = new PostMessageTimer(*this, message, sourceOrigin, source, channels.release(), target.get(), stackTrace.release(), UserGestureIndicator::currentToken());
    timer->startOneShot(0, FROM_HERE);
    timer->suspendIfNeeded();
}

void DOMWindow::postMessageTimerFired(PassOwnPtr<PostMessageTimer> t)
{
    OwnPtr<PostMessageTimer> timer(t);

    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtrWillBeRawPtr<MessageEvent> event = timer->event();

    // Give the embedder a chance to intercept this postMessage because this
    // DOMWindow might be a proxy for another in browsers that support
    // postMessage calls across WebKit instances.
    if (m_frame->loader().client()->willCheckAndDispatchMessageEvent(timer->targetOrigin(), event.get()))
        return;

    UserGestureIndicator gestureIndicator(timer->userGestureToken());

    event->entangleMessagePorts(document());
    dispatchMessageEventWithOriginCheck(timer->targetOrigin(), event, timer->stackTrace());
}

void DOMWindow::dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, PassRefPtrWillBeRawPtr<Event> event, PassRefPtr<ScriptCallStack> stackTrace)
{
    if (intendedTargetOrigin) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        if (!intendedTargetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
            String message = ExceptionMessages::failedToExecute("postMessage", "DOMWindow", "The target origin provided ('" + intendedTargetOrigin->toString() + "') does not match the recipient window's origin ('" + document()->securityOrigin()->toString() + "').");
            pageConsole()->addMessage(SecurityMessageSource, ErrorMessageLevel, message, stackTrace);
            return;
        }
    }

    dispatchEvent(event);
}

DOMSelection* DOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame() || !m_frame)
        return 0;

    return m_frame->document()->getSelection();
}

Element* DOMWindow::frameElement() const
{
    if (!m_frame)
        return 0;

    return m_frame->ownerElement();
}

void DOMWindow::focus(ExecutionContext* context)
{
    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    bool allowFocus = WindowFocusAllowedIndicator::windowFocusAllowed();
    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (opener() && opener() != this && activeDocument->domWindow() == opener())
            allowFocus = true;
    }

    // If we're a top level window, bring the window to the front.
    if (m_frame->isMainFrame() && allowFocus)
        host->chrome().focus();

    if (!m_frame)
        return;

    m_frame->eventHandler().focusDocumentView();
}

void DOMWindow::blur()
{
}

void DOMWindow::close(ExecutionContext* context)
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (!activeDocument)
            return;

        if (!activeDocument->canNavigate(m_frame))
            return;
    }

    Settings* settings = m_frame->settings();
    bool allowScriptsToCloseWindows = settings && settings->allowScriptsToCloseWindows();

    if (!(page->openedByDOM() || page->backForward().backForwardListCount() <= 1 || allowScriptsToCloseWindows)) {
        pageConsole()->addMessage(JSMessageSource, WarningMessageLevel, "Scripts may close only the windows that were opened by it.");
        return;
    }

    if (!m_frame->loader().shouldClose())
        return;

    page->chrome().closeWindowSoon();
}

void DOMWindow::print()
{
    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    if (m_frame->loader().provisionalDocumentLoader() || m_frame->loader().documentLoader()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    host->chrome().print(m_frame);
}

void DOMWindow::stop()
{
    if (!m_frame)
        return;
    m_frame->loader().stopAllLoaders();
}

void DOMWindow::alert(const String& message)
{
    if (!m_frame)
        return;

    m_frame->document()->updateRenderTreeIfNeeded();

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    host->chrome().runJavaScriptAlert(m_frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;

    m_frame->document()->updateRenderTreeIfNeeded();

    FrameHost* host = m_frame->host();
    if (!host)
        return false;

    return host->chrome().runJavaScriptConfirm(m_frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    m_frame->document()->updateRenderTreeIfNeeded();

    FrameHost* host = m_frame->host();
    if (!host)
        return String();

    String returnValue;
    if (host->chrome().runJavaScriptPrompt(m_frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

bool DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // |m_frame| can be destructed during |Editor::findString()| via
    // |Document::updateLayou()|, e.g. event handler removes a frame.
    RefPtr<LocalFrame> protectFrame(m_frame);

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog
    return m_frame->editor().findString(string, !backwards, caseSensitive, wrap, false);
}

bool DOMWindow::offscreenBuffering() const
{
    return true;
}

int DOMWindow::outerHeight() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().height() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().height());
}

int DOMWindow::outerWidth() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().width() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().width());
}

int DOMWindow::innerHeight() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's renderer.
    if (LocalFrame* parent = m_frame->tree().parent())
        parent->document()->updateLayoutIgnorePendingStylesheets();

    return adjustForAbsoluteZoom(view->visibleContentRect(IncludeScrollbars).height(), m_frame->pageZoomFactor());
}

int DOMWindow::innerWidth() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's renderer.
    if (LocalFrame* parent = m_frame->tree().parent())
        parent->document()->updateLayoutIgnorePendingStylesheets();

    return adjustForAbsoluteZoom(view->visibleContentRect(IncludeScrollbars).width(), m_frame->pageZoomFactor());
}

int DOMWindow::screenX() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().x() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().x());
}

int DOMWindow::screenY() const
{
    if (!m_frame)
        return 0;

    FrameHost* host = m_frame->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().y() * host->deviceScaleFactor());
    return static_cast<int>(host->chrome().windowRect().y());
}

int DOMWindow::scrollX() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return adjustForAbsoluteZoom(view->scrollX(), m_frame->pageZoomFactor());
}

int DOMWindow::scrollY() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return adjustForAbsoluteZoom(view->scrollY(), m_frame->pageZoomFactor());
}

bool DOMWindow::closed() const
{
    return !m_frame;
}

unsigned DOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return m_frame->tree().scopedChildCount();
}

const AtomicString& DOMWindow::name() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullAtom;

    return m_frame->tree().name();
}

void DOMWindow::setName(const AtomicString& name)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    m_frame->tree().setName(name);
    ASSERT(m_frame->loader().client());
    m_frame->loader().client()->didChangeName(name);
}

void DOMWindow::setStatus(const String& string)
{
    m_status = string;

    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chrome().setStatusbarText(m_frame, m_status);
}

void DOMWindow::setDefaultStatus(const String& string)
{
    m_defaultStatus = string;

    if (!m_frame)
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chrome().setStatusbarText(m_frame, m_defaultStatus);
}

DOMWindow* DOMWindow::self() const
{
    if (!m_frame)
        return 0;

    return m_frame->domWindow();
}

DOMWindow* DOMWindow::opener() const
{
    if (!m_frame)
        return 0;

    LocalFrame* opener = m_frame->loader().opener();
    if (!opener)
        return 0;

    return opener->domWindow();
}

DOMWindow* DOMWindow::parent() const
{
    if (!m_frame)
        return 0;

    LocalFrame* parent = m_frame->tree().parent();
    if (parent)
        return parent->domWindow();

    return m_frame->domWindow();
}

DOMWindow* DOMWindow::top() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return m_frame->tree().top()->domWindow();
}

Document* DOMWindow::document() const
{
    return m_document.get();
}

StyleMedia& DOMWindow::styleMedia() const
{
    if (!m_media)
        m_media = StyleMedia::create(m_frame);
    return *m_media;
}

PassRefPtr<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element* elt, const String& pseudoElt) const
{
    if (!elt)
        return nullptr;

    return CSSComputedStyleDeclaration::create(elt, false, pseudoElt);
}

PassRefPtrWillBeRawPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement) const
{
    UseCounter::count(document(), UseCounter::GetMatchedCSSRules);
    if (!element)
        return nullptr;

    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoType pseudoType = CSSSelector::parsePseudoType(AtomicString(pseudoElement.substring(colonStart)));
    if (pseudoType == CSSSelector::PseudoUnknown && !pseudoElement.isEmpty())
        return nullptr;

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);
    return m_frame->document()->ensureStyleResolver().pseudoCSSRulesForElement(element, pseudoId, rulesToInclude);
}

PassRefPtrWillBeRawPtr<DOMPoint> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const DOMPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return DOMPoint::create(pagePoint.x(), pagePoint.y());
}

PassRefPtrWillBeRawPtr<DOMPoint> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const DOMPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return DOMPoint::create(nodePoint.x(), nodePoint.y());
}

double DOMWindow::devicePixelRatio() const
{
    if (!m_frame)
        return 0.0;

    return m_frame->devicePixelRatio();
}

static bool scrollBehaviorFromScrollOptions(const Dictionary& scrollOptions, ScrollBehavior& scrollBehavior, ExceptionState& exceptionState)
{
    String scrollBehaviorString;
    if (!scrollOptions.get("behavior", scrollBehaviorString)) {
        scrollBehavior = ScrollBehaviorAuto;
        return true;
    }

    if (ScrollableArea::scrollBehaviorFromString(scrollBehaviorString, scrollBehavior))
        return true;

    exceptionState.throwTypeError("The ScrollBehavior provided is invalid.");
    return false;
}

void DOMWindow::scrollBy(int x, int y, const Dictionary& scrollOptions, ExceptionState &exceptionState) const
{
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    if (RuntimeEnabledFeatures::cssomSmoothScrollEnabled() && !scrollBehaviorFromScrollOptions(scrollOptions, scrollBehavior, exceptionState))
        return;

    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    IntSize scaledOffset(x * m_frame->pageZoomFactor(), y * m_frame->pageZoomFactor());
    // FIXME: Use scrollBehavior to decide whether to scroll smoothly or instantly.
    view->scrollBy(scaledOffset);
}

void DOMWindow::scrollTo(int x, int y, const Dictionary& scrollOptions, ExceptionState& exceptionState) const
{
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    if (RuntimeEnabledFeatures::cssomSmoothScrollEnabled() && !scrollBehaviorFromScrollOptions(scrollOptions, scrollBehavior, exceptionState))
        return;

    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    RefPtr<FrameView> view = m_frame->view();
    if (!view)
        return;

    IntPoint layoutPos(x * m_frame->pageZoomFactor(), y * m_frame->pageZoomFactor());
    // FIXME: Use scrollBehavior to decide whether to scroll smoothly or instantly.
    view->setScrollPosition(layoutPos);
}

void DOMWindow::moveBy(float x, float y) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect windowRect = host->chrome().windowRect();
    windowRect.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, windowRect));
}

void DOMWindow::moveTo(float x, float y) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect windowRect = host->chrome().windowRect();
    windowRect.setLocation(FloatPoint(x, y));
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, windowRect));
}

void DOMWindow::resizeBy(float x, float y) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect fr = host->chrome().windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, update));
}

void DOMWindow::resizeTo(float width, float height) const
{
    if (!m_frame || !m_frame->isMainFrame())
        return;

    FrameHost* host = m_frame->host();
    if (!host)
        return;

    FloatRect fr = host->chrome().windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    host->chrome().setWindowRect(adjustWindowRect(*m_frame, update));
}

int DOMWindow::requestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

int DOMWindow::webkitRequestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

void DOMWindow::cancelAnimationFrame(int id)
{
    if (Document* d = document())
        d->cancelAnimationFrame(id);
}

DOMWindowCSS& DOMWindow::css() const
{
    if (!m_css)
        m_css = DOMWindowCSS::create();
    return *m_css;
}

static void didAddStorageEventListener(DOMWindow* window)
{
    // Creating these WebCore::Storage objects informs the system that we'd like to receive
    // notifications about storage events that might be triggered in other processes. Rather
    // than subscribe to these notifications explicitly, we subscribe to them implicitly to
    // simplify the work done by the system.
    window->localStorage(IGNORE_EXCEPTION);
    window->sessionStorage(IGNORE_EXCEPTION);
}

bool DOMWindow::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
        if (isTouchEventType(eventType))
            document->didAddTouchEventHandler(document);
        else if (eventType == EventTypeNames::storage)
            didAddStorageEventListener(this);
    }

    lifecycleNotifier().notifyAddEventListener(this, eventType);

    if (eventType == EventTypeNames::unload) {
        UseCounter::count(document(), UseCounter::DocumentUnloadRegistered);
        addUnloadEventListener(this);
    } else if (eventType == EventTypeNames::beforeunload) {
        UseCounter::count(document(), UseCounter::DocumentBeforeUnloadRegistered);
        if (allowsBeforeUnloadListeners(this)) {
            // This is confusingly named. It doesn't actually add the listener. It just increments a count
            // so that we know we have listeners registered for the purposes of determining if we can
            // fast terminate the renderer process.
            addBeforeUnloadEventListener(this);
        } else {
            // Subframes return false from allowsBeforeUnloadListeners.
            UseCounter::count(document(), UseCounter::SubFrameBeforeUnloadRegistered);
        }
    }

    return true;
}

bool DOMWindow::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    if (Document* document = this->document()) {
        if (isTouchEventType(eventType))
            document->didRemoveTouchEventHandler(document);
    }

    lifecycleNotifier().notifyRemoveEventListener(this, eventType);

    if (eventType == EventTypeNames::unload) {
        removeUnloadEventListener(this);
    } else if (eventType == EventTypeNames::beforeunload && allowsBeforeUnloadListeners(this)) {
        removeBeforeUnloadEventListener(this);
    }

    return true;
}

void DOMWindow::dispatchLoadEvent()
{
    RefPtrWillBeRawPtr<Event> loadEvent(Event::create(EventTypeNames::load));
    if (m_frame && m_frame->loader().documentLoader() && !m_frame->loader().documentLoader()->timing()->loadEventStart()) {
        // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed while dispatching
        // the event, so protect it to prevent writing the end time into freed memory.
        RefPtr<DocumentLoader> documentLoader = m_frame->loader().documentLoader();
        DocumentLoadTiming* timing = documentLoader->timing();
        timing->markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing->markLoadEventEnd();
    } else
        dispatchEvent(loadEvent, document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    Element* ownerElement = m_frame ? m_frame->ownerElement() : 0;
    if (ownerElement)
        ownerElement->dispatchEvent(Event::create(EventTypeNames::load));

    InspectorInstrumentation::loadEventFired(frame());
}

bool DOMWindow::dispatchEvent(PassRefPtrWillBeRawPtr<Event> prpEvent, PassRefPtr<EventTarget> prpTarget)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtr<EventTarget> protect = this;
    RefPtrWillBeRawPtr<Event> event = prpEvent;

    event->setTarget(prpTarget ? prpTarget : this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEventOnWindow(frame(), *event, this);

    bool result = fireEventListeners(event.get());

    InspectorInstrumentation::didDispatchEventOnWindow(cookie);

    return result;
}

void DOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    lifecycleNotifier().notifyRemoveAllEventListeners(this);

    if (Document* document = this->document())
        document->didClearTouchEventHandlers(document);

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void DOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        print();
    }
}

void DOMWindow::setLocation(const String& urlString, DOMWindow* callingWindow, DOMWindow* enteredWindow, SetLocationLocking locking)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* activeDocument = callingWindow->document();
    if (!activeDocument)
        return;

    if (!activeDocument->canNavigate(m_frame))
        return;

    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return;

    KURL completedURL = firstFrame->document()->completeURL(urlString);
    if (completedURL.isNull())
        return;

    if (isInsecureScriptAccess(*callingWindow, completedURL))
        return;

    // We want a new history item if we are processing a user gesture.
    m_frame->navigationScheduler().scheduleLocationChange(activeDocument,
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, Referrer(activeDocument->outgoingReferrer(), activeDocument->referrerPolicy()),
        locking != LockHistoryBasedOnGestureState);
}

void DOMWindow::printErrorMessage(const String& message)
{
    if (message.isEmpty())
        return;

    pageConsole()->addMessage(JSMessageSource, ErrorMessageLevel, message);
}

// FIXME: Once we're throwing exceptions for cross-origin access violations, we will always sanitize the target
// frame details, so we can safely combine 'crossDomainAccessErrorMessage' with this method after considering
// exactly which details may be exposed to JavaScript.
//
// http://crbug.com/17325
String DOMWindow::sanitizedCrossDomainAccessErrorMessage(DOMWindow* callingWindow)
{
    if (!callingWindow || !callingWindow->document())
        return String();

    const KURL& callingWindowURL = callingWindow->document()->url();
    if (callingWindowURL.isNull())
        return String();

    ASSERT(!callingWindow->document()->securityOrigin()->canAccess(document()->securityOrigin()));

    SecurityOrigin* activeOrigin = callingWindow->document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a cross-origin frame.";

    // FIXME: Evaluate which details from 'crossDomainAccessErrorMessage' may safely be reported to JavaScript.

    return message;
}

String DOMWindow::crossDomainAccessErrorMessage(DOMWindow* callingWindow)
{
    if (!callingWindow || !callingWindow->document())
        return String();

    const KURL& callingWindowURL = callingWindow->document()->url();
    if (callingWindowURL.isNull())
        return String();

    ASSERT(!callingWindow->document()->securityOrigin()->canAccess(document()->securityOrigin()));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    SecurityOrigin* activeOrigin = callingWindow->document()->securityOrigin();
    SecurityOrigin* targetOrigin = document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a frame with origin \"" + targetOrigin->toString() + "\". ";

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    KURL activeURL = callingWindow->document()->url();
    KURL targetURL = document()->url();
    if (document()->isSandboxed(SandboxOrigin) || callingWindow->document()->isSandboxed(SandboxOrigin)) {
        message = "Blocked a frame at \"" + SecurityOrigin::create(activeURL)->toString() + "\" from accessing a frame at \"" + SecurityOrigin::create(targetURL)->toString() + "\". ";
        if (document()->isSandboxed(SandboxOrigin) && callingWindow->document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " Both frames are sandboxed and lack the \"allow-same-origin\" flag.";
        if (document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag.";
        return "Sandbox access violation: " + message + " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag.";
    }

    // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
    if (targetOrigin->protocol() != activeOrigin->protocol())
        return message + " The frame requesting access has a protocol of \"" + activeURL.protocol() + "\", the frame being accessed has a protocol of \"" + targetURL.protocol() + "\". Protocols must match.\n";

    // 'document.domain' errors.
    if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", the frame being accessed set it to \"" + targetOrigin->domain() + "\". Both must set \"document.domain\" to the same value to allow access.";
    if (activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access.";
    if (targetOrigin->domainWasSetInDOM())
        return message + "The frame being accessed set \"document.domain\" to \"" + targetOrigin->domain() + "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access.";

    // Default.
    return message + "Protocols, domains, and ports must match.";
}

bool DOMWindow::isInsecureScriptAccess(DOMWindow& callingWindow, const String& urlString)
{
    if (!protocolIsJavaScript(urlString))
        return false;

    // If this DOMWindow isn't currently active in the LocalFrame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect DOMWindow from
    // LocalFrame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "callingWindow == this" check?
        if (&callingWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (callingWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(&callingWindow));
    return true;
}

PassRefPtrWillBeRawPtr<DOMWindow> DOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    DOMWindow* callingWindow, DOMWindow* enteredWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    Document* activeDocument = callingWindow->document();
    if (!activeDocument)
        return nullptr;
    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return nullptr;

    if (!enteredWindow->allowPopUp()) {
        // Because FrameTree::find() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !m_frame->tree().find(frameName))
            return nullptr;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    LocalFrame* targetFrame = 0;
    if (frameName == "_top")
        targetFrame = m_frame->tree().top();
    else if (frameName == "_parent") {
        if (LocalFrame* parent = m_frame->tree().parent())
            targetFrame = parent;
        else
            targetFrame = m_frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame))
            return nullptr;

        KURL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->domWindow()->isInsecureScriptAccess(*callingWindow, completedURL))
            return targetFrame->domWindow();

        if (urlString.isEmpty())
            return targetFrame->domWindow();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        targetFrame->navigationScheduler().scheduleLocationChange(
            activeDocument,
            completedURL,
            Referrer(firstFrame->document()->outgoingReferrer(), firstFrame->document()->referrerPolicy()),
            false);
        return targetFrame->domWindow();
    }

    WindowFeatures windowFeatures(windowFeaturesString);
    LocalFrame* result = createWindow(urlString, frameName, windowFeatures, *callingWindow, *firstFrame, *m_frame);
    return result ? result->domWindow() : 0;
}

void DOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString,
    DOMWindow* callingWindow, DOMWindow* enteredWindow, PrepareDialogFunction function, void* functionContext)
{
    if (!isCurrentlyDisplayedInFrame())
        return;
    LocalFrame* activeFrame = callingWindow->frame();
    if (!activeFrame)
        return;
    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return;

    if (!canShowModalDialogNow(m_frame) || !enteredWindow->allowPopUp())
        return;

    UseCounter::countDeprecation(this, UseCounter::ShowModalDialog);

    WindowFeatures windowFeatures(dialogFeaturesString, screenAvailableRect(m_frame->view()));
    LocalFrame* dialogFrame = createWindow(urlString, emptyAtom, windowFeatures,
        *callingWindow, *firstFrame, *m_frame, function, functionContext);
    if (!dialogFrame)
        return;
    UserGestureIndicatorDisabler disabler;
    dialogFrame->host()->chrome().runModal();
}

DOMWindow* DOMWindow::anonymousIndexedGetter(uint32_t index)
{
    LocalFrame* frame = this->frame();
    if (!frame)
        return 0;

    LocalFrame* child = frame->tree().scopedChild(index);
    if (child)
        return child->domWindow();

    return 0;
}

DOMWindowLifecycleNotifier& DOMWindow::lifecycleNotifier()
{
    return static_cast<DOMWindowLifecycleNotifier&>(LifecycleContext<DOMWindow>::lifecycleNotifier());
}

PassOwnPtr<LifecycleNotifier<DOMWindow> > DOMWindow::createLifecycleNotifier()
{
    return DOMWindowLifecycleNotifier::create(this);
}

void DOMWindow::trace(Visitor* visitor)
{
    visitor->trace(m_screen);
    visitor->trace(m_history);
    visitor->trace(m_locationbar);
    visitor->trace(m_menubar);
    visitor->trace(m_personalbar);
    visitor->trace(m_scrollbars);
    visitor->trace(m_statusbar);
    visitor->trace(m_toolbar);
    visitor->trace(m_console);
    visitor->trace(m_navigator);
    visitor->trace(m_location);
    visitor->trace(m_media);
    visitor->trace(m_sessionStorage);
    visitor->trace(m_localStorage);
    visitor->trace(m_applicationCache);
    visitor->trace(m_performance);
    visitor->trace(m_css);
#if ENABLE(OILPAN)
    HeapSupplementable<DOMWindow>::trace(visitor);
#endif
}

} // namespace WebCore

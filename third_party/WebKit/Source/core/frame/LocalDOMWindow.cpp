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
#include "core/frame/LocalDOMWindow.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
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
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/History.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/Navigator.h"
#include "core/frame/Screen.h"
#include "core/frame/ScrollToOptions.h"
#include "core/frame/Settings.h"
#include "core/frame/SuspendableTimer.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/SinkDocument.h"
#include "core/loader/appcache/ApplicationCache.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/CreateWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/PlatformScreen.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/IntRect.h"
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

namespace blink {

LocalDOMWindow::WindowFrameObserver::WindowFrameObserver(LocalDOMWindow* window, LocalFrame& frame)
    : FrameDestructionObserver(&frame)
    , m_window(window)
{
}

PassOwnPtrWillBeRawPtr<LocalDOMWindow::WindowFrameObserver> LocalDOMWindow::WindowFrameObserver::create(LocalDOMWindow* window, LocalFrame& frame)
{
    return adoptPtrWillBeNoop(new WindowFrameObserver(window, frame));
}

#if !ENABLE(OILPAN)
LocalDOMWindow::WindowFrameObserver::~WindowFrameObserver()
{
}
#endif

DEFINE_TRACE(LocalDOMWindow::WindowFrameObserver)
{
    visitor->trace(m_window);
    FrameDestructionObserver::trace(visitor);
}

void LocalDOMWindow::WindowFrameObserver::willDetachFrameHost()
{
    m_window->willDetachFrameHost();
}

void LocalDOMWindow::WindowFrameObserver::contextDestroyed()
{
    m_window->frameDestroyed();
    FrameDestructionObserver::contextDestroyed();
}

class PostMessageTimer final : public NoBaseWillBeGarbageCollectedFinalized<PostMessageTimer>, public SuspendableTimer {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PostMessageTimer);
public:
    PostMessageTimer(LocalDOMWindow& window, PassRefPtr<SerializedScriptValue> message, const String& sourceOrigin, PassRefPtrWillBeRawPtr<LocalDOMWindow> source, PassOwnPtr<MessagePortChannelArray> channels, SecurityOrigin* targetOrigin, PassRefPtrWillBeRawPtr<ScriptCallStack> stackTrace, UserGestureToken* userGestureToken)
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
        m_asyncOperationId = InspectorInstrumentation::traceAsyncOperationStarting(executionContext(), "postMessage");
    }

    PassRefPtrWillBeRawPtr<MessageEvent> event()
    {
        return MessageEvent::create(m_channels.release(), m_message, m_origin, String(), m_source.get());

    }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    ScriptCallStack* stackTrace() const { return m_stackTrace.get(); }
    UserGestureToken* userGestureToken() const { return m_userGestureToken.get(); }
    LocalDOMWindow* source() const { return m_source.get(); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_window);
        visitor->trace(m_source);
        visitor->trace(m_stackTrace);
        SuspendableTimer::trace(visitor);
    }

private:
    virtual void fired() override
    {
        InspectorInstrumentationCookie cookie = InspectorInstrumentation::traceAsyncOperationCompletedCallbackStarting(executionContext(), m_asyncOperationId);
        m_window->postMessageTimerFired(this);
        // This object is deleted now.
        InspectorInstrumentation::traceAsyncCallbackCompleted(cookie);
    }

    // FIXME: Oilpan: This raw pointer is safe because the PostMessageTimer is
    // owned by the LocalDOMWindow. Ideally PostMessageTimer should be moved to
    // the heap and use Member<LocalDOMWindow>.
    RawPtrWillBeMember<LocalDOMWindow> m_window;
    RefPtr<SerializedScriptValue> m_message;
    String m_origin;
    RefPtrWillBeMember<LocalDOMWindow> m_source;
    OwnPtr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
    RefPtrWillBeMember<ScriptCallStack> m_stackTrace;
    RefPtr<UserGestureToken> m_userGestureToken;
    int m_asyncOperationId;
};

static void updateSuddenTerminationStatus(LocalDOMWindow* domWindow, bool addedListener, FrameLoaderClient::SuddenTerminationDisablerType disablerType)
{
    blink::Platform::current()->suddenTerminationChanged(!addedListener);
    if (domWindow->frame() && domWindow->frame()->loader().client())
        domWindow->frame()->loader().client()->suddenTerminationDisablerChanged(addedListener, disablerType);
}

typedef HashCountedSet<LocalDOMWindow*> DOMWindowSet;

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

static void addUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, true, FrameLoaderClient::UnloadHandler);

    set.add(domWindow);
}

static void removeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::UnloadHandler);
}

static void removeAllUnloadEventListeners(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::UnloadHandler);
}

static void addBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, true, FrameLoaderClient::BeforeUnloadHandler);

    set.add(domWindow);
}

static void removeBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::BeforeUnloadHandler);
}

static void removeAllBeforeUnloadEventListeners(LocalDOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        updateSuddenTerminationStatus(domWindow, false, FrameLoaderClient::BeforeUnloadHandler);
}

static bool allowsBeforeUnloadListeners(LocalDOMWindow* window)
{
    ASSERT_ARG(window, window);
    LocalFrame* frame = window->frame();
    if (!frame)
        return false;
    return frame->isMainFrame();
}

unsigned LocalDOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<LocalDOMWindow*>(this));
}

// This function:
// 1) Constrains the window rect to the minimum window size and no bigger than the int rect's dimensions.
// 2) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 3) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 4) Translate the window rect coordinates to be within the coordinate space of the screen.
IntRect LocalDOMWindow::adjustWindowRect(LocalFrame& frame, const IntRect& pendingChanges)
{
    FrameHost* host = frame.host();
    ASSERT(host);

    IntRect screen = screenAvailableRect(frame.view());
    IntRect window = pendingChanges;

    IntSize minimumSize = host->chrome().client().minimumWindowSize();
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

bool LocalDOMWindow::allowPopUp(LocalFrame& firstFrame)
{
    if (UserGestureIndicator::processingUserGesture())
        return true;

    Settings* settings = firstFrame.settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

bool LocalDOMWindow::allowPopUp()
{
    return frame() && allowPopUp(*frame());
}

LocalDOMWindow::LocalDOMWindow(LocalFrame& frame)
    : m_frameObserver(WindowFrameObserver::create(this, frame))
    , m_shouldPrintWhenFinishedLoading(false)
#if ENABLE(ASSERT)
    , m_hasBeenReset(false)
#endif
{
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(*this);
#endif
}

void LocalDOMWindow::clearDocument()
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

    // FIXME: This should be part of ActiveDOMObject shutdown
    clearEventQueue();

    m_document->clearDOMWindow();
    m_document = nullptr;
}

void LocalDOMWindow::clearEventQueue()
{
    if (!m_eventQueue)
        return;
    m_eventQueue->close();
    m_eventQueue.clear();
}

void LocalDOMWindow::acceptLanguagesChanged()
{
    if (m_navigator)
        m_navigator->setLanguagesChanged();

    dispatchEvent(Event::create(EventTypeNames::languagechange));
}

PassRefPtrWillBeRawPtr<Document> LocalDOMWindow::createDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    RefPtrWillBeRawPtr<Document> document = nullptr;
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

PassRefPtrWillBeRawPtr<Document> LocalDOMWindow::installNewDocument(const String& mimeType, const DocumentInit& init, bool forceXHTML)
{
    ASSERT(init.frame() == frame());

    clearDocument();

    m_document = createDocument(mimeType, init, forceXHTML);
    m_eventQueue = DOMWindowEventQueue::create(m_document.get());
    m_document->attach();

    if (!frame())
        return m_document;

    frame()->script().updateDocument();
    m_document->updateViewportDescription();

    if (frame()->page() && frame()->view()) {
        if (ScrollingCoordinator* scrollingCoordinator = frame()->page()->scrollingCoordinator()) {
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(frame()->view(), HorizontalScrollbar);
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(frame()->view(), VerticalScrollbar);
            scrollingCoordinator->scrollableAreaScrollLayerDidChange(frame()->view());
        }
    }

    frame()->selection().updateSecureKeyboardEntryIfActive();
    return m_document;
}

EventQueue* LocalDOMWindow::eventQueue() const
{
    return m_eventQueue.get();
}

void LocalDOMWindow::enqueueWindowEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(this);
    m_eventQueue->enqueueEvent(event);
}

void LocalDOMWindow::enqueueDocumentEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (!m_eventQueue)
        return;
    event->setTarget(m_document.get());
    m_eventQueue->enqueueEvent(event);
}

void LocalDOMWindow::dispatchWindowLoadEvent()
{
    ASSERT(!EventDispatchForbiddenScope::isEventDispatchForbidden());
    dispatchLoadEvent();
}

void LocalDOMWindow::documentWasClosed()
{
    dispatchWindowLoadEvent();
    enqueuePageshowEvent(PageshowEventNotPersisted);
    if (m_pendingStateObject)
        enqueuePopstateEvent(m_pendingStateObject.release());
}

void LocalDOMWindow::enqueuePageshowEvent(PageshowEventPersistence persisted)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs to fire asynchronously.
    // As per spec pageshow must be triggered asynchronously.
    // However to be compatible with other browsers blink fires pageshow synchronously.
    dispatchEvent(PageTransitionEvent::create(EventTypeNames::pageshow, persisted), m_document.get());
}

void LocalDOMWindow::enqueueHashchangeEvent(const String& oldURL, const String& newURL)
{
    enqueueWindowEvent(HashChangeEvent::create(oldURL, newURL));
}

void LocalDOMWindow::enqueuePopstateEvent(PassRefPtr<SerializedScriptValue> stateObject)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36202 Popstate event needs to fire asynchronously
    dispatchEvent(PopStateEvent::create(stateObject, history()));
}

void LocalDOMWindow::statePopped(PassRefPtr<SerializedScriptValue> stateObject)
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

LocalDOMWindow::~LocalDOMWindow()
{
#if ENABLE(OILPAN)
    // Cleared when detaching document.
    ASSERT(!m_eventQueue);
#else
    ASSERT(m_hasBeenReset);
    ASSERT(m_document->isStopped());
    clearDocument();
#endif
}

void LocalDOMWindow::dispose()
{
    // Oilpan: should the LocalDOMWindow be GCed along with its LocalFrame without the
    // frame having first notified its observers of imminent destruction, the
    // LocalDOMWindow will not have had an opportunity to remove event listeners.
    // Do that here by way of a prefinalizing action.
    //
    // (Non-Oilpan, LocalDOMWindow::reset() will always be invoked, the last opportunity
    // being via ~LocalFrame's setDOMWindow() call.)
    if (!frame())
        return;

    // (Prefinalizing actions run to completion before the Oilpan GC start to lazily sweep,
    // so keeping them short is worthwhile. Something that's worth keeping in mind when
    // working out where to best place some actions that have to be performed very late
    // on in LocalDOMWindow's lifetime.)
    removeAllEventListeners();
}

const AtomicString& LocalDOMWindow::interfaceName() const
{
    return EventTargetNames::LocalDOMWindow;
}

ExecutionContext* LocalDOMWindow::executionContext() const
{
    return m_document.get();
}

LocalDOMWindow* LocalDOMWindow::toDOMWindow()
{
    return this;
}

PassRefPtrWillBeRawPtr<MediaQueryList> LocalDOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* LocalDOMWindow::page()
{
    return frame() ? frame()->page() : nullptr;
}

void LocalDOMWindow::willDetachFrameHost()
{
    frame()->host()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);
    frame()->host()->consoleMessageStorage().frameWindowDiscarded(this);
    LocalDOMWindow::notifyContextDestroyed();
}

void LocalDOMWindow::frameDestroyed()
{
    willDestroyDocumentInFrame();
    resetLocation();
    m_properties.clear();
    removeAllEventListeners();
}

void LocalDOMWindow::willDestroyDocumentInFrame()
{
    for (const auto& domWindowProperty : m_properties)
        domWindowProperty->willDestroyGlobalObjectInFrame();
}

void LocalDOMWindow::willDetachDocumentFromFrame()
{
    for (const auto& domWindowProperty : m_properties)
        domWindowProperty->willDetachGlobalObjectFromFrame();
}

void LocalDOMWindow::registerProperty(DOMWindowProperty* property)
{
    m_properties.add(property);
}

void LocalDOMWindow::unregisterProperty(DOMWindowProperty* property)
{
    m_properties.remove(property);
}

void LocalDOMWindow::reset()
{
    frameDestroyed();

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
    m_media = nullptr;
    m_applicationCache = nullptr;
#if ENABLE(ASSERT)
    m_hasBeenReset = true;
#endif

    resetLocation();

    LocalDOMWindow::notifyContextDestroyed();
}

void LocalDOMWindow::sendOrientationChangeEvent()
{
    ASSERT(RuntimeEnabledFeatures::orientationEventEnabled());

    // Before dispatching the event, build a list of the child frames to
    // also send the event to, to mitigate side effects from event handlers
    // potentially interfering with others.
    WillBeHeapVector<RefPtrWillBeMember<Frame>> childFrames;
    for (Frame* child = frame()->tree().firstChild(); child; child = child->tree().nextSibling()) {
        childFrames.append(child);
    }

    dispatchEvent(Event::create(EventTypeNames::orientationchange));

    for (size_t i = 0; i < childFrames.size(); ++i) {
        if (!childFrames[i]->isLocalFrame())
            continue;
        toLocalFrame(childFrames[i].get())->localDOMWindow()->sendOrientationChangeEvent();
    }
}

int LocalDOMWindow::orientation() const
{
    ASSERT(RuntimeEnabledFeatures::orientationEventEnabled());

    if (!frame())
        return 0;

    int orientation = screenOrientationAngle(frame()->view());
    // For backward compatibility, we want to return a value in the range of
    // [-90; 180] instead of [0; 360[ because window.orientation used to behave
    // like that in WebKit (this is a WebKit proprietary API).
    if (orientation == 270)
        return -90;
    return orientation;
}

Screen* LocalDOMWindow::screen() const
{
    if (!m_screen)
        m_screen = Screen::create(frame());
    return m_screen.get();
}

History* LocalDOMWindow::history() const
{
    if (!m_history)
        m_history = History::create(frame());
    return m_history.get();
}

BarProp* LocalDOMWindow::locationbar() const
{
    if (!m_locationbar)
        m_locationbar = BarProp::create(frame(), BarProp::Locationbar);
    return m_locationbar.get();
}

BarProp* LocalDOMWindow::menubar() const
{
    if (!m_menubar)
        m_menubar = BarProp::create(frame(), BarProp::Menubar);
    return m_menubar.get();
}

BarProp* LocalDOMWindow::personalbar() const
{
    if (!m_personalbar)
        m_personalbar = BarProp::create(frame(), BarProp::Personalbar);
    return m_personalbar.get();
}

BarProp* LocalDOMWindow::scrollbars() const
{
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(frame(), BarProp::Scrollbars);
    return m_scrollbars.get();
}

BarProp* LocalDOMWindow::statusbar() const
{
    if (!m_statusbar)
        m_statusbar = BarProp::create(frame(), BarProp::Statusbar);
    return m_statusbar.get();
}

BarProp* LocalDOMWindow::toolbar() const
{
    if (!m_toolbar)
        m_toolbar = BarProp::create(frame(), BarProp::Toolbar);
    return m_toolbar.get();
}

Console* LocalDOMWindow::console() const
{
    if (!m_console)
        m_console = Console::create(frame());
    return m_console.get();
}

FrameConsole* LocalDOMWindow::frameConsole() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    return &frame()->console();
}

ApplicationCache* LocalDOMWindow::applicationCache() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_applicationCache)
        m_applicationCache = ApplicationCache::create(frame());
    return m_applicationCache.get();
}

Navigator* LocalDOMWindow::navigator() const
{
    if (!m_navigator)
        m_navigator = Navigator::create(frame());
    return m_navigator.get();
}

void LocalDOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, const String& targetOrigin, LocalDOMWindow* source, ExceptionState& exceptionState)
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

    if (MixedContentChecker::isMixedContent(sourceDocument->securityOrigin(), document()->url()))
        UseCounter::count(document(), UseCounter::PostMessageFromSecureToInsecure);
    else if (MixedContentChecker::isMixedContent(document()->securityOrigin(), sourceDocument->url()))
        UseCounter::count(document(), UseCounter::PostMessageFromInsecureToSecure);

    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtrWillBeRawPtr<ScriptCallStack> stackTrace = nullptr;
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
        stackTrace = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);

    // Schedule the message.
    OwnPtrWillBeRawPtr<PostMessageTimer> timer = adoptPtrWillBeNoop(new PostMessageTimer(*this, message, sourceOrigin, source, channels.release(), target.get(), stackTrace.release(), UserGestureIndicator::currentToken()));
    timer->startOneShot(0, FROM_HERE);
    timer->suspendIfNeeded();
    m_postMessageTimers.add(timer.release());
}

void LocalDOMWindow::postMessageTimerFired(PostMessageTimer* timer)
{
    if (!isCurrentlyDisplayedInFrame()) {
        m_postMessageTimers.remove(timer);
        return;
    }

    RefPtrWillBeRawPtr<MessageEvent> event = timer->event();

    // Give the embedder a chance to intercept this postMessage because this
    // LocalDOMWindow might be a proxy for another in browsers that support
    // postMessage calls across WebKit instances.
    LocalFrame* source = timer->source()->document() ? timer->source()->document()->frame() : nullptr;
    if (frame()->client()->willCheckAndDispatchMessageEvent(timer->targetOrigin(), event.get(), source)) {
        m_postMessageTimers.remove(timer);
        return;
    }

    UserGestureIndicator gestureIndicator(timer->userGestureToken());

    event->entangleMessagePorts(document());
    dispatchMessageEventWithOriginCheck(timer->targetOrigin(), event, timer->stackTrace());
    m_postMessageTimers.remove(timer);
}

void LocalDOMWindow::dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, PassRefPtrWillBeRawPtr<Event> event, PassRefPtrWillBeRawPtr<ScriptCallStack> stackTrace)
{
    if (intendedTargetOrigin) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        if (!intendedTargetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
            String message = ExceptionMessages::failedToExecute("postMessage", "DOMWindow", "The target origin provided ('" + intendedTargetOrigin->toString() + "') does not match the recipient window's origin ('" + document()->securityOrigin()->toString() + "').");
            RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message);
            consoleMessage->setCallStack(stackTrace);
            frameConsole()->addMessage(consoleMessage.release());
            return;
        }
    }

    dispatchEvent(event);
}

DOMSelection* LocalDOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    return frame()->document()->getSelection();
}

Element* LocalDOMWindow::frameElement() const
{
    if (!frame())
        return nullptr;

    // The bindings security check should ensure we're same origin...
    ASSERT(!frame()->owner() || frame()->owner()->isLocal());
    return frame()->deprecatedLocalOwner();
}

void LocalDOMWindow::focus(ExecutionContext* context)
{
    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    ASSERT(context);

    bool allowFocus = context->isWindowInteractionAllowed();
    if (allowFocus) {
        context->consumeWindowInteraction();
    } else {
        ASSERT(isMainThread());
        allowFocus = opener() && (opener() != this) && (toDocument(context)->domWindow() == opener());
    }

    // If we're a top level window, bring the window to the front.
    if (frame()->isMainFrame() && allowFocus)
        host->chrome().focus();

    frame()->eventHandler().focusDocumentView();
}

void LocalDOMWindow::blur()
{
}

void LocalDOMWindow::close(ExecutionContext* context)
{
    if (!frame() || !frame()->isMainFrame())
        return;

    Page* page = frame()->page();
    if (!page)
        return;

    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (!activeDocument)
            return;

        if (!activeDocument->frame() || !activeDocument->frame()->canNavigate(*frame()))
            return;
    }

    Settings* settings = frame()->settings();
    bool allowScriptsToCloseWindows = settings && settings->allowScriptsToCloseWindows();

    if (!page->openedByDOM() && frame()->loader().client()->backForwardLength() > 1 && !allowScriptsToCloseWindows) {
        frameConsole()->addMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Scripts may close only the windows that were opened by it."));
        return;
    }

    if (!frame()->loader().shouldClose())
        return;

    InspectorInstrumentation::willCloseWindow(context);

    page->chrome().closeWindowSoon();
}

void LocalDOMWindow::print()
{
    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    if (frame()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    host->chrome().print(frame());
}

void LocalDOMWindow::stop()
{
    if (!frame())
        return;
    frame()->loader().stopAllLoaders();
}

void LocalDOMWindow::alert(const String& message)
{
    if (!frame())
        return;

    frame()->document()->updateRenderTreeIfNeeded();

    FrameHost* host = frame()->host();
    if (!host)
        return;

    host->chrome().runJavaScriptAlert(frame(), message);
}

bool LocalDOMWindow::confirm(const String& message)
{
    if (!frame())
        return false;

    frame()->document()->updateRenderTreeIfNeeded();

    FrameHost* host = frame()->host();
    if (!host)
        return false;

    return host->chrome().runJavaScriptConfirm(frame(), message);
}

String LocalDOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!frame())
        return String();

    frame()->document()->updateRenderTreeIfNeeded();

    FrameHost* host = frame()->host();
    if (!host)
        return String();

    String returnValue;
    if (host->chrome().runJavaScriptPrompt(frame(), message, defaultValue, returnValue))
        return returnValue;

    return String();
}

bool LocalDOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // |frame()| can be destructed during |Editor::findString()| via
    // |Document::updateLayout()|, e.g. event handler removes a frame.
    RefPtrWillBeRawPtr<LocalFrame> protectFrame(frame());

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog
    return frame()->editor().findString(string, !backwards, caseSensitive, wrap, false);
}

bool LocalDOMWindow::offscreenBuffering() const
{
    return true;
}

int LocalDOMWindow::outerHeight() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().height() * host->deviceScaleFactor());
    return host->chrome().windowRect().height();
}

int LocalDOMWindow::outerWidth() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().width() * host->deviceScaleFactor());
    return host->chrome().windowRect().width();
}

int LocalDOMWindow::innerHeight() const
{
    if (!frame())
        return 0;

    FrameView* view = frame()->view();
    if (!view)
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's renderer.
    if (Frame* parent = frame()->tree().parent()) {
        if (parent && parent->isLocalFrame())
            toLocalFrame(parent)->document()->updateLayoutIgnorePendingStylesheets();
    }

    FloatSize viewportSize = host->settings().pinchVirtualViewportEnabled() && frame()->isMainFrame()
        ? host->pinchViewport().visibleRect().size()
        : view->visibleContentRect(IncludeScrollbars).size();

    return adjustForAbsoluteZoom(expandedIntSize(viewportSize).height(), frame()->pageZoomFactor());
}

int LocalDOMWindow::innerWidth() const
{
    if (!frame())
        return 0;

    FrameView* view = frame()->view();
    if (!view)
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    // FIXME: This is potentially too much work. We really only need to know the dimensions of the parent frame's renderer.
    if (Frame* parent = frame()->tree().parent()) {
        if (parent && parent->isLocalFrame())
            toLocalFrame(parent)->document()->updateLayoutIgnorePendingStylesheets();
    }

    FloatSize viewportSize = host->settings().pinchVirtualViewportEnabled() && frame()->isMainFrame()
        ? host->pinchViewport().visibleRect().size()
        : view->visibleContentRect(IncludeScrollbars).size();

    return adjustForAbsoluteZoom(expandedIntSize(viewportSize).width(), frame()->pageZoomFactor());
}

int LocalDOMWindow::screenX() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().x() * host->deviceScaleFactor());
    return host->chrome().windowRect().x();
}

int LocalDOMWindow::screenY() const
{
    if (!frame())
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    if (host->settings().reportScreenSizeInPhysicalPixelsQuirk())
        return lroundf(host->chrome().windowRect().y() * host->deviceScaleFactor());
    return host->chrome().windowRect().y();
}

double LocalDOMWindow::scrollX() const
{
    if (!frame())
        return 0;

    FrameView* view = frame()->view();
    if (!view)
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    frame()->document()->updateLayoutIgnorePendingStylesheets();

    double viewportX = view->scrollableArea()->scrollPositionDouble().x();

    if (host->settings().pinchVirtualViewportEnabled() && frame()->isMainFrame())
        viewportX += host->pinchViewport().location().x();

    return adjustScrollForAbsoluteZoom(viewportX, frame()->pageZoomFactor());
}

double LocalDOMWindow::scrollY() const
{
    if (!frame())
        return 0;

    FrameView* view = frame()->view();
    if (!view)
        return 0;

    FrameHost* host = frame()->host();
    if (!host)
        return 0;

    frame()->document()->updateLayoutIgnorePendingStylesheets();

    double viewportY = view->scrollableArea()->scrollPositionDouble().y();

    if (host->settings().pinchVirtualViewportEnabled() && frame()->isMainFrame())
        viewportY += host->pinchViewport().location().y();

    return adjustScrollForAbsoluteZoom(viewportY, frame()->pageZoomFactor());
}

const AtomicString& LocalDOMWindow::name() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullAtom;

    return frame()->tree().name();
}

void LocalDOMWindow::setName(const AtomicString& name)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    frame()->tree().setName(name);
    ASSERT(frame()->loader().client());
    frame()->loader().client()->didChangeName(name);
}

void LocalDOMWindow::setStatus(const String& string)
{
    m_status = string;

    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    ASSERT(frame()->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chrome().setStatusbarText(frame(), m_status);
}

void LocalDOMWindow::setDefaultStatus(const String& string)
{
    m_defaultStatus = string;

    if (!frame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    ASSERT(frame()->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    host->chrome().setStatusbarText(frame(), m_defaultStatus);
}

Document* LocalDOMWindow::document() const
{
    return m_document.get();
}

StyleMedia* LocalDOMWindow::styleMedia() const
{
    if (!m_media)
        m_media = StyleMedia::create(frame());
    return m_media.get();
}

PassRefPtrWillBeRawPtr<CSSStyleDeclaration> LocalDOMWindow::getComputedStyle(Element* elt, const String& pseudoElt) const
{
    ASSERT(elt);
    return CSSComputedStyleDeclaration::create(elt, false, pseudoElt);
}

PassRefPtrWillBeRawPtr<CSSRuleList> LocalDOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement) const
{
    if (!element)
        return nullptr;

    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoType pseudoType = CSSSelector::parsePseudoType(AtomicString(pseudoElement.substring(colonStart)), false);
    if (pseudoType == CSSSelector::PseudoUnknown && !pseudoElement.isEmpty())
        return nullptr;

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);
    element->document().updateRenderTreeIfNeeded();
    return frame()->document()->ensureStyleResolver().pseudoCSSRulesForElement(element, pseudoId, rulesToInclude);
}

double LocalDOMWindow::devicePixelRatio() const
{
    if (!frame())
        return 0.0;

    return frame()->devicePixelRatio();
}

// FIXME: This class shouldn't be explicitly moving the viewport around. crbug.com/371896
static void scrollViewportTo(LocalFrame* frame, DoublePoint offset, ScrollBehavior scrollBehavior)
{
    FrameView* view = frame->view();
    if (!view)
        return;

    FrameHost* host = frame->host();
    if (!host)
        return;

    view->scrollableArea()->setScrollPosition(offset, scrollBehavior);

    if (host->settings().pinchVirtualViewportEnabled() && frame->isMainFrame()) {
        PinchViewport& pinchViewport = frame->host()->pinchViewport();
        DoubleSize excessDelta = offset - DoublePoint(pinchViewport.visibleRectInDocument().location());
        pinchViewport.move(FloatPoint(excessDelta.width(), excessDelta.height()));
    }
}

void LocalDOMWindow::scrollBy(double x, double y, ScrollBehavior scrollBehavior) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = frame()->view();
    if (!view)
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    if (std::isnan(x) || std::isnan(y))
        return;

    DoublePoint currentOffset = host->settings().pinchVirtualViewportEnabled() && frame()->isMainFrame()
        ? DoublePoint(host->pinchViewport().visibleRectInDocument().location())
        : view->scrollableArea()->scrollPositionDouble();

    DoubleSize scaledOffset(x * frame()->pageZoomFactor(), y * frame()->pageZoomFactor());
    scrollViewportTo(frame(), currentOffset + scaledOffset, scrollBehavior);
}

void LocalDOMWindow::scrollBy(const ScrollToOptions& scrollToOptions) const
{
    double x = 0.0;
    double y = 0.0;
    if (scrollToOptions.hasLeft())
        x = scrollToOptions.left();
    if (scrollToOptions.hasTop())
        y = scrollToOptions.top();
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);
    scrollBy(x, y, scrollBehavior);
}

void LocalDOMWindow::scrollTo(double x, double y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    if (std::isnan(x) || std::isnan(y))
        return;

    DoublePoint layoutPos(x * frame()->pageZoomFactor(), y * frame()->pageZoomFactor());
    scrollViewportTo(frame(), layoutPos, ScrollBehaviorAuto);
}

void LocalDOMWindow::scrollTo(const ScrollToOptions& scrollToOptions) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = frame()->view();
    if (!view)
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    double scaledX = 0.0;
    double scaledY = 0.0;

    DoublePoint currentOffset = host->settings().pinchVirtualViewportEnabled() && frame()->isMainFrame()
        ? DoublePoint(host->pinchViewport().visibleRectInDocument().location())
        : view->scrollableArea()->scrollPositionDouble();
    scaledX = currentOffset.x();
    scaledY = currentOffset.y();

    if (scrollToOptions.hasLeft()) {
        if (std::isnan(scrollToOptions.left()))
            return;
        scaledX = scrollToOptions.left() * frame()->pageZoomFactor();
    }

    if (scrollToOptions.hasTop()) {
        if (std::isnan(scrollToOptions.top()))
            return;
        scaledY = scrollToOptions.top() * frame()->pageZoomFactor();
    }

    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);
    scrollViewportTo(frame(), DoublePoint(scaledX, scaledY), scrollBehavior);
}

void LocalDOMWindow::moveBy(int x, int y, bool hasX, bool hasY) const
{
    if (!hasX || !hasY)
        UseCounter::count(document(), UseCounter::WindowMoveResizeMissingArguments);

    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect windowRect = host->chrome().windowRect();
    windowRect.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chrome().setWindowRect(adjustWindowRect(*frame(), windowRect));
}

void LocalDOMWindow::moveTo(int x, int y, bool hasX, bool hasY) const
{
    if (!hasX || !hasY)
        UseCounter::count(document(), UseCounter::WindowMoveResizeMissingArguments);

    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect windowRect = host->chrome().windowRect();
    windowRect.setLocation(IntPoint(hasX ? x : windowRect.x(), hasY ? y : windowRect.y()));
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    host->chrome().setWindowRect(adjustWindowRect(*frame(), windowRect));
}

void LocalDOMWindow::resizeBy(int x, int y, bool hasX, bool hasY) const
{
    if (!hasX || !hasY)
        UseCounter::count(document(), UseCounter::WindowMoveResizeMissingArguments);

    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect fr = host->chrome().windowRect();
    IntSize dest = fr.size() + IntSize(x, y);
    IntRect update(fr.location(), dest);
    host->chrome().setWindowRect(adjustWindowRect(*frame(), update));
}

void LocalDOMWindow::resizeTo(int width, int height, bool hasWidth, bool hasHeight) const
{
    if (!hasWidth || !hasHeight)
        UseCounter::count(document(), UseCounter::WindowMoveResizeMissingArguments);

    if (!frame() || !frame()->isMainFrame())
        return;

    FrameHost* host = frame()->host();
    if (!host)
        return;

    IntRect fr = host->chrome().windowRect();
    IntSize dest = IntSize(hasWidth ? width : fr.width(), hasHeight ? height : fr.height());
    IntRect update(fr.location(), dest);
    host->chrome().setWindowRect(adjustWindowRect(*frame(), update));
}

int LocalDOMWindow::requestAnimationFrame(RequestAnimationFrameCallback* callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

int LocalDOMWindow::webkitRequestAnimationFrame(RequestAnimationFrameCallback* callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

void LocalDOMWindow::cancelAnimationFrame(int id)
{
    if (Document* d = document())
        d->cancelAnimationFrame(id);
}

DOMWindowCSS* LocalDOMWindow::css() const
{
    if (!m_css)
        m_css = DOMWindowCSS::create();
    return m_css.get();
}

bool LocalDOMWindow::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> prpListener, bool useCapture)
{
    RefPtr<EventListener> listener = prpListener;
    if (!EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    if (frame() && frame()->host())
        frame()->host()->eventHandlerRegistry().didAddEventHandler(*this, eventType);

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
    }

    notifyAddEventListener(this, eventType);

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

bool LocalDOMWindow::removeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    if (frame() && frame()->host())
        frame()->host()->eventHandlerRegistry().didRemoveEventHandler(*this, eventType);

    notifyRemoveEventListener(this, eventType);

    if (eventType == EventTypeNames::unload) {
        removeUnloadEventListener(this);
    } else if (eventType == EventTypeNames::beforeunload && allowsBeforeUnloadListeners(this)) {
        removeBeforeUnloadEventListener(this);
    }

    return true;
}

void LocalDOMWindow::dispatchLoadEvent()
{
    RefPtrWillBeRawPtr<Event> loadEvent(Event::create(EventTypeNames::load));
    if (frame() && frame()->loader().documentLoader() && !frame()->loader().documentLoader()->timing().loadEventStart()) {
        // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed while dispatching
        // the event, so protect it to prevent writing the end time into freed memory.
        RefPtr<DocumentLoader> documentLoader = frame()->loader().documentLoader();
        DocumentLoadTiming& timing = documentLoader->timing();
        timing.markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing.markLoadEventEnd();
    } else
        dispatchEvent(loadEvent, document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    FrameOwner* owner = frame() ? frame()->owner() : nullptr;
    if (owner)
        owner->dispatchLoad();

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "MarkLoad", "data", InspectorMarkLoadEvent::data(frame()));
    InspectorInstrumentation::loadEventFired(frame());
}

bool LocalDOMWindow::dispatchEvent(PassRefPtrWillBeRawPtr<Event> prpEvent, PassRefPtrWillBeRawPtr<EventTarget> prpTarget)
{
    ASSERT(!EventDispatchForbiddenScope::isEventDispatchForbidden());

    RefPtrWillBeRawPtr<EventTarget> protect(this);
    RefPtrWillBeRawPtr<Event> event = prpEvent;

    event->setTarget(prpTarget ? prpTarget : this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "EventDispatch", "data", InspectorEventDispatchEvent::data(*event));
    return fireEventListeners(event.get());
}

void LocalDOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    notifyRemoveAllEventListeners(this);
    if (frame() && frame()->host())
        frame()->host()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void LocalDOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        print();
    }
}

void LocalDOMWindow::printErrorMessage(const String& message)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    if (message.isEmpty())
        return;

    frameConsole()->addMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

// FIXME: Once we're throwing exceptions for cross-origin access violations, we will always sanitize the target
// frame details, so we can safely combine 'crossDomainAccessErrorMessage' with this method after considering
// exactly which details may be exposed to JavaScript.
//
// http://crbug.com/17325
String LocalDOMWindow::sanitizedCrossDomainAccessErrorMessage(LocalDOMWindow* callingWindow)
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

String LocalDOMWindow::crossDomainAccessErrorMessage(LocalDOMWindow* callingWindow)
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

bool LocalDOMWindow::isInsecureScriptAccess(DOMWindow& callingWindow, const String& urlString)
{
    if (!DOMWindow::isInsecureScriptAccess(callingWindow, urlString))
        return false;

    if (callingWindow.isLocalDOMWindow())
        printErrorMessage(crossDomainAccessErrorMessage(static_cast<LocalDOMWindow*>(&callingWindow)));
    return true;
}

PassRefPtrWillBeRawPtr<DOMWindow> LocalDOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    Document* activeDocument = callingWindow->document();
    if (!activeDocument)
        return nullptr;
    LocalFrame* firstFrame = enteredWindow->frame();
    if (!firstFrame)
        return nullptr;

    UseCounter::count(*activeDocument, UseCounter::DOMWindowOpen);
    if (!windowFeaturesString.isEmpty())
        UseCounter::count(*activeDocument, UseCounter::DOMWindowOpenFeatures);

    if (!enteredWindow->allowPopUp()) {
        // Because FrameTree::find() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !frame()->tree().find(frameName))
            return nullptr;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = nullptr;
    if (frameName == "_top")
        targetFrame = frame()->tree().top();
    else if (frameName == "_parent") {
        if (Frame* parent = frame()->tree().parent())
            targetFrame = parent;
        else
            targetFrame = frame();
    }

    if (targetFrame) {
        if (!activeDocument->frame() || !activeDocument->frame()->canNavigate(*targetFrame))
            return nullptr;

        KURL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->domWindow()->isInsecureScriptAccess(*callingWindow, completedURL))
            return targetFrame->domWindow();

        if (urlString.isEmpty())
            return targetFrame->domWindow();

        targetFrame->navigate(*activeDocument, completedURL, false);
        return targetFrame->domWindow();
    }

    WindowFeatures windowFeatures(windowFeaturesString);
    LocalFrame* result = createWindow(urlString, frameName, windowFeatures, *callingWindow, *firstFrame, *frame());
    return result ? result->domWindow() : nullptr;
}

DEFINE_TRACE(LocalDOMWindow)
{
#if ENABLE(OILPAN)
    visitor->trace(m_frameObserver);
    visitor->trace(m_document);
    visitor->trace(m_properties);
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
    visitor->trace(m_media);
    visitor->trace(m_applicationCache);
    visitor->trace(m_css);
    visitor->trace(m_eventQueue);
    visitor->trace(m_postMessageTimers);
    HeapSupplementable<LocalDOMWindow>::trace(visitor);
#endif
    DOMWindow::trace(visitor);
    DOMWindowLifecycleNotifier::trace(visitor);
}

LocalFrame* LocalDOMWindow::frame() const
{
    return m_frameObserver->frame();
}

v8::Handle<v8::Object> LocalDOMWindow::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT_NOT_REACHED(); // LocalDOMWindow has [Custom=ToV8].
    return v8::Handle<v8::Object>();
}

} // namespace blink

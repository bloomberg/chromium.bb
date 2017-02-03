/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef LocalDOMWindow_h
#define LocalDOMWindow_h

#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/CoreExport.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"

#include <memory>

namespace blink {

class ApplicationCache;
class BarProp;
class CSSRuleList;
class CSSStyleDeclaration;
class CustomElementRegistry;
class Document;
class DocumentInit;
class DOMSelection;
class DOMVisualViewport;
class DOMWindowEventQueue;
class Element;
class EventQueue;
class External;
class FrameConsole;
class FrameRequestCallback;
class History;
class IdleRequestCallback;
class IdleRequestOptions;
class MediaQueryList;
class MessageEvent;
class Navigator;
class PostMessageTimer;
class Screen;
class ScriptState;
class ScrollToOptions;
class SecurityOrigin;
class SerializedScriptValue;
class SourceLocation;
class StyleMedia;

enum PageshowEventPersistence {
  PageshowEventNotPersisted = 0,
  PageshowEventPersisted = 1
};

// Note: if you're thinking of returning something DOM-related by reference,
// please ping dcheng@chromium.org first. You probably don't want to do that.
class CORE_EXPORT LocalDOMWindow final : public DOMWindow,
                                         public Supplementable<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(LocalDOMWindow);
  USING_PRE_FINALIZER(LocalDOMWindow, dispose);

 public:
  class CORE_EXPORT EventListenerObserver : public GarbageCollectedMixin {
   public:
    virtual void didAddEventListener(LocalDOMWindow*, const AtomicString&) = 0;
    virtual void didRemoveEventListener(LocalDOMWindow*,
                                        const AtomicString&) = 0;
    virtual void didRemoveAllEventListeners(LocalDOMWindow*) = 0;
  };

  static Document* createDocument(const String& mimeType,
                                  const DocumentInit&,
                                  bool forceXHTML);
  static LocalDOMWindow* create(LocalFrame& frame) {
    return new LocalDOMWindow(frame);
  }

  ~LocalDOMWindow() override;

  LocalFrame* frame() const { return toLocalFrame(DOMWindow::frame()); }

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  Document* installNewDocument(const String& mimeType,
                               const DocumentInit&,
                               bool forceXHTML = false);

  // EventTarget overrides:
  ExecutionContext* getExecutionContext() const override;
  const LocalDOMWindow* toLocalDOMWindow() const override;
  LocalDOMWindow* toLocalDOMWindow() override;

  // Same-origin DOM Level 0
  Screen* screen() const;
  History* history() const;
  BarProp* locationbar() const;
  BarProp* menubar() const;
  BarProp* personalbar() const;
  BarProp* scrollbars() const;
  BarProp* statusbar() const;
  BarProp* toolbar() const;
  Navigator* navigator() const;
  Navigator* clientInformation() const { return navigator(); }

  bool offscreenBuffering() const;

  int outerHeight() const;
  int outerWidth() const;
  int innerHeight() const;
  int innerWidth() const;
  int screenX() const;
  int screenY() const;
  int screenLeft() const { return screenX(); }
  int screenTop() const { return screenY(); }
  double scrollX() const;
  double scrollY() const;
  double pageXOffset() const { return scrollX(); }
  double pageYOffset() const { return scrollY(); }

  DOMVisualViewport* visualViewport();

  const AtomicString& name() const;
  void setName(const AtomicString&);

  String status() const;
  void setStatus(const String&);
  String defaultStatus() const;
  void setDefaultStatus(const String&);

  // DOM Level 2 AbstractView Interface
  Document* document() const;

  // CSSOM View Module
  StyleMedia* styleMedia() const;

  // WebKit extensions
  double devicePixelRatio() const;

  ApplicationCache* applicationCache() const;

  // This is the interface orientation in degrees. Some examples are:
  //  0 is straight up; -90 is when the device is rotated 90 clockwise;
  //  90 is when rotated counter clockwise.
  int orientation() const;

  DOMSelection* getSelection();

  void blur() override;
  void print(ScriptState*);
  void stop();

  void alert(ScriptState*, const String& message = String());
  bool confirm(ScriptState*, const String& message);
  String prompt(ScriptState*,
                const String& message,
                const String& defaultValue);

  bool find(const String&,
            bool caseSensitive,
            bool backwards,
            bool wrap,
            bool wholeWord,
            bool searchInFrames,
            bool showDialog) const;

  // FIXME: ScrollBehaviorSmooth is currently unsupported in VisualViewport.
  // crbug.com/434497
  void scrollBy(double x, double y, ScrollBehavior = ScrollBehaviorAuto) const;
  void scrollBy(const ScrollToOptions&) const;
  void scrollTo(double x, double y) const;
  void scrollTo(const ScrollToOptions&) const;
  void scroll(double x, double y) const { scrollTo(x, y); }
  void scroll(const ScrollToOptions& scrollToOptions) const {
    scrollTo(scrollToOptions);
  }
  void moveBy(int x, int y) const;
  void moveTo(int x, int y) const;

  void resizeBy(int x, int y) const;
  void resizeTo(int width, int height) const;

  MediaQueryList* matchMedia(const String&);

  // DOM Level 2 Style Interface
  CSSStyleDeclaration* getComputedStyle(Element*,
                                        const String& pseudoElt) const;

  // WebKit extension
  CSSRuleList* getMatchedCSSRules(Element*, const String& pseudoElt) const;

  // WebKit animation extensions
  int requestAnimationFrame(FrameRequestCallback*);
  int webkitRequestAnimationFrame(FrameRequestCallback*);
  void cancelAnimationFrame(int id);

  // Idle callback extensions
  int requestIdleCallback(IdleRequestCallback*, const IdleRequestOptions&);
  void cancelIdleCallback(int id);

  // Custom elements
  CustomElementRegistry* customElements(ScriptState*) const;
  CustomElementRegistry* customElements() const;
  CustomElementRegistry* maybeCustomElements() const;

  // Obsolete APIs
  void captureEvents() {}
  void releaseEvents() {}
  External* external();

  bool isSecureContext() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(animationend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(animationiteration);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(animationstart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(transitionend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkitanimationstart,
                                         webkitAnimationStart);
  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkitanimationiteration,
                                         webkitAnimationIteration);
  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkitanimationend,
                                         webkitAnimationEnd);
  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkittransitionend,
                                         webkitTransitionEnd);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(orientationchange);

  void registerEventListenerObserver(EventListenerObserver*);

  void frameDestroyed();
  void reset();

  unsigned pendingUnloadEventListeners() const;

  bool allowPopUp();  // Call on first window, not target window.
  static bool allowPopUp(LocalFrame& firstFrame);

  Element* frameElement() const;

  DOMWindow* open(const String& urlString,
                  const AtomicString& frameName,
                  const String& windowFeaturesString,
                  LocalDOMWindow* callingWindow,
                  LocalDOMWindow* enteredWindow);

  FrameConsole* frameConsole() const;

  void printErrorMessage(const String&) const;

  void postMessageTimerFired(PostMessageTimer*);
  void removePostMessageTimer(PostMessageTimer*);
  void dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin,
                                           Event*,
                                           std::unique_ptr<SourceLocation>);

  // Events
  // EventTarget API
  void removeAllEventListeners() override;

  using EventTarget::dispatchEvent;
  DispatchEventResult dispatchEvent(Event*, EventTarget*);

  void finishedLoading();

  // Dispatch the (deprecated) orientationchange event to this DOMWindow and
  // recurse on its child frames.
  void sendOrientationChangeEvent();

  EventQueue* getEventQueue() const;
  void enqueueWindowEvent(Event*);
  void enqueueDocumentEvent(Event*);
  void enqueuePageshowEvent(PageshowEventPersistence);
  void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
  void enqueuePopstateEvent(PassRefPtr<SerializedScriptValue>);
  void dispatchWindowLoadEvent();
  void documentWasClosed();
  void statePopped(PassRefPtr<SerializedScriptValue>);

  // FIXME: This shouldn't be public once LocalDOMWindow becomes
  // ExecutionContext.
  void clearEventQueue();

  void acceptLanguagesChanged();

  FloatSize getViewportSize(IncludeScrollbarsInRect) const;

 protected:
  // EventTarget overrides.
  void addedEventListener(const AtomicString& eventType,
                          RegisteredEventListener&) override;
  void removedEventListener(const AtomicString& eventType,
                            const RegisteredEventListener&) override;

  // Protected DOMWindow overrides.
  void schedulePostMessage(MessageEvent*,
                           PassRefPtr<SecurityOrigin> target,
                           Document* source) override;

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already LocalDOMWindow.
  bool isLocalDOMWindow() const override { return true; }
  bool isRemoteDOMWindow() const override { return false; }
  void warnUnusedPreloads(TimerBase*);

  explicit LocalDOMWindow(LocalFrame&);
  void dispose();

  void dispatchLoadEvent();
  void clearDocument();

  void willDetachFrameHost();

  Member<Document> m_document;
  Member<DOMVisualViewport> m_visualViewport;
  TaskRunnerTimer<LocalDOMWindow> m_unusedPreloadsTimer;

  bool m_shouldPrintWhenFinishedLoading;

  mutable Member<Screen> m_screen;
  mutable Member<History> m_history;
  mutable Member<BarProp> m_locationbar;
  mutable Member<BarProp> m_menubar;
  mutable Member<BarProp> m_personalbar;
  mutable Member<BarProp> m_scrollbars;
  mutable Member<BarProp> m_statusbar;
  mutable Member<BarProp> m_toolbar;
  mutable Member<Navigator> m_navigator;
  mutable Member<StyleMedia> m_media;
  mutable TraceWrapperMember<CustomElementRegistry> m_customElements;
  Member<External> m_external;

  String m_status;
  String m_defaultStatus;

  mutable Member<ApplicationCache> m_applicationCache;

  Member<DOMWindowEventQueue> m_eventQueue;
  RefPtr<SerializedScriptValue> m_pendingStateObject;

  HeapHashSet<Member<PostMessageTimer>> m_postMessageTimers;
  HeapHashSet<WeakMember<EventListenerObserver>> m_eventListenerObservers;
};

DEFINE_TYPE_CASTS(LocalDOMWindow,
                  DOMWindow,
                  x,
                  x->isLocalDOMWindow(),
                  x.isLocalDOMWindow());

inline String LocalDOMWindow::status() const {
  return m_status;
}

inline String LocalDOMWindow::defaultStatus() const {
  return m_defaultStatus;
}

}  // namespace blink

#endif  // LocalDOMWindow_h

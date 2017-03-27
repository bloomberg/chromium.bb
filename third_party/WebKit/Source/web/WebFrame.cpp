// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebFrame.h"

#include "bindings/core/v8/WindowProxyManager.h"
#include "core/HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/Page.h"
#include "platform/UserGestureIndicator.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebSandboxFlags.h"
#include "web/OpenedFrameTracker.h"
#include "web/RemoteFrameOwner.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include <algorithm>

namespace blink {

bool WebFrame::swap(WebFrame* frame) {
  using std::swap;
  Frame* oldFrame = toImplBase()->frame();
  if (!oldFrame->isAttached())
    return false;

  // Unload the current Document in this frame: this calls unload handlers,
  // detaches child frames, etc. Since this runs script, make sure this frame
  // wasn't detached before continuing with the swap.
  // FIXME: There is no unit test for this condition, so one needs to be
  // written.
  if (!oldFrame->prepareForCommit())
    return false;

  if (m_parent) {
    if (m_parent->m_firstChild == this)
      m_parent->m_firstChild = frame;
    if (m_parent->m_lastChild == this)
      m_parent->m_lastChild = frame;
    // FIXME: This is due to the fact that the |frame| may be a provisional
    // local frame, because we don't know if the navigation will result in
    // an actual page or something else, like a download. The PlzNavigate
    // project will remove the need for provisional local frames.
    frame->m_parent = m_parent;
  }

  if (m_previousSibling) {
    m_previousSibling->m_nextSibling = frame;
    swap(m_previousSibling, frame->m_previousSibling);
  }
  if (m_nextSibling) {
    m_nextSibling->m_previousSibling = frame;
    swap(m_nextSibling, frame->m_nextSibling);
  }

  if (m_opener) {
    frame->setOpener(m_opener);
    setOpener(nullptr);
  }
  m_openedFrameTracker->transferTo(frame);

  Page* page = oldFrame->page();
  AtomicString name = oldFrame->tree().name();
  FrameOwner* owner = oldFrame->owner();

  v8::HandleScope handleScope(v8::Isolate::GetCurrent());
  WindowProxyManager::GlobalsVector globals;
  oldFrame->getWindowProxyManager()->clearForNavigation();
  oldFrame->getWindowProxyManager()->releaseGlobals(globals);

  // Although the Document in this frame is now unloaded, many resources
  // associated with the frame itself have not yet been freed yet.
  oldFrame->detach(FrameDetachType::Swap);

  // Clone the state of the current Frame into the one being swapped in.
  // FIXME: This is a bit clunky; this results in pointless decrements and
  // increments of connected subframes.
  if (frame->isWebLocalFrame()) {
    // TODO(dcheng): in an ideal world, both branches would just use
    // WebFrameImplBase's initializeCoreFrame() helper. However, Blink
    // currently requires a 'provisional' local frame to serve as a
    // placeholder for loading state when swapping to a local frame.
    // In this case, the core LocalFrame is already initialized, so just
    // update a bit of state.
    LocalFrame& localFrame = *toWebLocalFrameImpl(frame)->frame();
    DCHECK_EQ(owner, localFrame.owner());
    if (owner) {
      owner->setContentFrame(localFrame);
      if (owner->isLocal())
        toHTMLFrameOwnerElement(owner)->setWidget(localFrame.view());
    } else {
      localFrame.page()->setMainFrame(&localFrame);
      // This trace event is needed to detect the main frame of the
      // renderer in telemetry metrics. See crbug.com/692112#c11.
      TRACE_EVENT_INSTANT1("loading", "markAsMainFrame",
                           TRACE_EVENT_SCOPE_THREAD, "frame", &localFrame);
    }
  } else {
    toWebRemoteFrameImpl(frame)->initializeCoreFrame(*page, owner, name);
  }

  if (m_parent && oldFrame->hasReceivedUserGesture())
    frame->toImplBase()->frame()->setDocumentHasReceivedUserGesture();

  frame->toImplBase()->frame()->getWindowProxyManager()->setGlobals(globals);

  m_parent = nullptr;

  return true;
}

void WebFrame::detach() {
  toImplBase()->frame()->detach(FrameDetachType::Remove);
}

WebSecurityOrigin WebFrame::getSecurityOrigin() const {
  return WebSecurityOrigin(
      toImplBase()->frame()->securityContext()->getSecurityOrigin());
}

void WebFrame::setFrameOwnerSandboxFlags(WebSandboxFlags flags) {
  // At the moment, this is only used to replicate sandbox flags
  // for frames with a remote owner.
  FrameOwner* owner = toImplBase()->frame()->owner();
  DCHECK(owner);
  toRemoteFrameOwner(owner)->setSandboxFlags(static_cast<SandboxFlags>(flags));
}

WebInsecureRequestPolicy WebFrame::getInsecureRequestPolicy() const {
  return toImplBase()->frame()->securityContext()->getInsecureRequestPolicy();
}

void WebFrame::setFrameOwnerProperties(
    const WebFrameOwnerProperties& properties) {
  // At the moment, this is only used to replicate frame owner properties
  // for frames with a remote owner.
  RemoteFrameOwner* owner = toRemoteFrameOwner(toImplBase()->frame()->owner());
  DCHECK(owner);

  Frame* frame = toImplBase()->frame();
  DCHECK(frame);

  if (frame->isLocalFrame()) {
    toLocalFrame(frame)->document()->willChangeFrameOwnerProperties(
        properties.marginWidth, properties.marginHeight,
        static_cast<ScrollbarMode>(properties.scrollingMode));
  }

  owner->setBrowsingContextContainerName(properties.name);
  owner->setScrollingMode(properties.scrollingMode);
  owner->setMarginWidth(properties.marginWidth);
  owner->setMarginHeight(properties.marginHeight);
  owner->setAllowFullscreen(properties.allowFullscreen);
  owner->setAllowPaymentRequest(properties.allowPaymentRequest);
  owner->setCsp(properties.requiredCsp);
  owner->setAllowedFeatures(properties.allowedFeatures);
}

WebFrame* WebFrame::opener() const {
  return m_opener;
}

void WebFrame::setOpener(WebFrame* opener) {
  if (m_opener)
    m_opener->m_openedFrameTracker->remove(this);
  if (opener)
    opener->m_openedFrameTracker->add(this);
  m_opener = opener;
}

void WebFrame::insertAfter(WebFrame* newChild, WebFrame* previousSibling) {
  newChild->m_parent = this;

  WebFrame* next;
  if (!previousSibling) {
    // Insert at the beginning if no previous sibling is specified.
    next = m_firstChild;
    m_firstChild = newChild;
  } else {
    DCHECK_EQ(previousSibling->m_parent, this);
    next = previousSibling->m_nextSibling;
    previousSibling->m_nextSibling = newChild;
    newChild->m_previousSibling = previousSibling;
  }

  if (next) {
    newChild->m_nextSibling = next;
    next->m_previousSibling = newChild;
  } else {
    m_lastChild = newChild;
  }

  toImplBase()->frame()->tree().invalidateScopedChildCount();
  toImplBase()->frame()->page()->incrementSubframeCount();
}

void WebFrame::appendChild(WebFrame* child) {
  // TODO(dcheng): Original code asserts that the frames have the same Page.
  // We should add an equivalent check... figure out what.
  insertAfter(child, m_lastChild);
}

void WebFrame::removeChild(WebFrame* child) {
  child->m_parent = 0;

  if (m_firstChild == child)
    m_firstChild = child->m_nextSibling;
  else
    child->m_previousSibling->m_nextSibling = child->m_nextSibling;

  if (m_lastChild == child)
    m_lastChild = child->m_previousSibling;
  else
    child->m_nextSibling->m_previousSibling = child->m_previousSibling;

  child->m_previousSibling = child->m_nextSibling = 0;

  toImplBase()->frame()->tree().invalidateScopedChildCount();
  toImplBase()->frame()->page()->decrementSubframeCount();
}

void WebFrame::setParent(WebFrame* parent) {
  m_parent = parent;
}

WebFrame* WebFrame::parent() const {
  return m_parent;
}

WebFrame* WebFrame::top() const {
  WebFrame* frame = const_cast<WebFrame*>(this);
  for (WebFrame* parent = frame; parent; parent = parent->m_parent)
    frame = parent;
  return frame;
}

WebFrame* WebFrame::firstChild() const {
  return m_firstChild;
}

WebFrame* WebFrame::nextSibling() const {
  return m_nextSibling;
}

WebFrame* WebFrame::traverseNext() const {
  if (Frame* frame = toImplBase()->frame())
    return fromFrame(frame->tree().traverseNext());
  return nullptr;
}

WebFrame* WebFrame::fromFrameOwnerElement(const WebElement& webElement) {
  Element* element = webElement;

  if (!element->isFrameOwnerElement())
    return nullptr;
  return fromFrame(toHTMLFrameOwnerElement(element)->contentFrame());
}

bool WebFrame::isLoading() const {
  if (Frame* frame = toImplBase()->frame())
    return frame->isLoading();
  return false;
}

WebFrame* WebFrame::fromFrame(Frame* frame) {
  if (!frame)
    return 0;

  if (frame->isLocalFrame())
    return WebLocalFrameImpl::fromFrame(toLocalFrame(*frame));
  return WebRemoteFrameImpl::fromFrame(toRemoteFrame(*frame));
}

WebFrame::WebFrame(WebTreeScopeType scope)
    : m_scope(scope),
      m_parent(0),
      m_previousSibling(0),
      m_nextSibling(0),
      m_firstChild(0),
      m_lastChild(0),
      m_opener(0),
      m_openedFrameTracker(new OpenedFrameTracker) {}

WebFrame::~WebFrame() {
  m_openedFrameTracker.reset(0);
}

void WebFrame::traceFrame(Visitor* visitor, WebFrame* frame) {
  if (!frame)
    return;

  if (frame->isWebLocalFrame())
    visitor->trace(toWebLocalFrameImpl(frame));
  else
    visitor->trace(toWebRemoteFrameImpl(frame));
}

void WebFrame::traceFrames(Visitor* visitor, WebFrame* frame) {
  DCHECK(frame);
  traceFrame(visitor, frame->m_parent);
  for (WebFrame* child = frame->firstChild(); child;
       child = child->nextSibling())
    traceFrame(visitor, child);
}

void WebFrame::close() {
  m_openedFrameTracker->dispose();
}

}  // namespace blink

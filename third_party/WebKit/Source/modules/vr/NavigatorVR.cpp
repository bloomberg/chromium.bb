// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/NavigatorVR.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Fullscreen.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "modules/vr/VRPose.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

void rejectNavigatorDetached(ScriptPromiseResolver* resolver) {
  DOMException* exception = DOMException::create(
      InvalidStateError, "The object is no longer associated with a document.");
  resolver->reject(exception);
}

}  // namespace

NavigatorVR* NavigatorVR::from(Document& document) {
  if (!document.frame() || !document.frame()->domWindow())
    return nullptr;
  Navigator& navigator = *document.frame()->domWindow()->navigator();
  return &from(navigator);
}

NavigatorVR& NavigatorVR::from(Navigator& navigator) {
  NavigatorVR* supplement = static_cast<NavigatorVR*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorVR(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* scriptState,
                                         Navigator& navigator) {
  if (!navigator.frame()) {
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    rejectNavigatorDetached(resolver);
    return promise;
  }
  return NavigatorVR::from(navigator).getVRDisplays(scriptState);
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!document()) {
    rejectNavigatorDetached(resolver);
    return promise;
  }

  UseCounter::count(*document(), UseCounter::VRGetDisplays);
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (!executionContext->isSecureContext())
    UseCounter::count(*document(), UseCounter::VRGetDisplaysInsecureOrigin);

  Platform::current()->recordRapporURL("VR.WebVR.GetDisplays",
                                       document()->url());

  controller()->getDisplays(resolver);

  return promise;
}

VRController* NavigatorVR::controller() {
  if (!supplementable()->frame())
    return nullptr;

  if (!m_controller) {
    m_controller = new VRController(this);
    m_controller->setListeningForActivate(m_focused && m_listeningForActivate);
    m_controller->focusChanged();
  }

  return m_controller;
}

Document* NavigatorVR::document() {
  if (!supplementable()->frame())
    return nullptr;

  return supplementable()->frame()->document();
}

DEFINE_TRACE(NavigatorVR) {
  visitor->trace(m_controller);
  Supplement<Navigator>::trace(visitor);
}

NavigatorVR::NavigatorVR(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      FocusChangedObserver(navigator.frame()->page()) {
  navigator.frame()->domWindow()->registerEventListenerObserver(this);
  focusedFrameChanged();
}

NavigatorVR::~NavigatorVR() {}

const char* NavigatorVR::supplementName() {
  return "NavigatorVR";
}

void NavigatorVR::enqueueVREvent(VRDisplayEvent* event) {
  if (!supplementable()->frame())
    return;

  supplementable()->frame()->domWindow()->enqueueWindowEvent(event);
}

void NavigatorVR::dispatchVRGestureEvent(VRDisplayEvent* event) {
  if (!(supplementable()->frame()))
    return;

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(document()));
  LocalDOMWindow* window = supplementable()->frame()->domWindow();
  DCHECK(window);
  event->setTarget(window);
  window->dispatchEvent(event);
}

void NavigatorVR::focusedFrameChanged() {
  bool focused = isFrameFocused(supplementable()->frame());
  if (focused == m_focused)
    return;
  m_focused = focused;
  if (m_controller) {
    m_controller->setListeningForActivate(m_listeningForActivate && focused);
    m_controller->focusChanged();
  }
}

void NavigatorVR::didAddEventListener(LocalDOMWindow* window,
                                      const AtomicString& eventType) {
  // TODO(mthiesse): Remove fullscreen requirement for presentation. See
  // crbug.com/687369
  if (eventType == EventTypeNames::vrdisplayactivate &&
      supplementable()->frame() && supplementable()->frame()->document() &&
      Fullscreen::fullscreenEnabled(*supplementable()->frame()->document())) {
    m_listeningForActivate = true;
    controller()->setListeningForActivate(m_focused);
  } else if (eventType == EventTypeNames::vrdisplayconnect) {
    // If the page is listening for connection events make sure we've created a
    // controller so that we'll be notified of new devices.
    controller();
  }
}

void NavigatorVR::didRemoveEventListener(LocalDOMWindow* window,
                                         const AtomicString& eventType) {
  if (eventType == EventTypeNames::vrdisplayactivate &&
      !window->hasEventListeners(EventTypeNames::vrdisplayactivate)) {
    m_listeningForActivate = false;
    controller()->setListeningForActivate(false);
  }
}

void NavigatorVR::didRemoveAllEventListeners(LocalDOMWindow* window) {
  if (!m_controller)
    return;

  m_controller->setListeningForActivate(false);
  m_listeningForActivate = false;
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"

namespace blink {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator& navigator) {}

NavigatorServiceWorker* NavigatorServiceWorker::from(Document& document) {
  if (!document.frame() || !document.frame()->domWindow())
    return nullptr;
  Navigator& navigator = *document.frame()->domWindow()->navigator();
  return &from(navigator);
}

NavigatorServiceWorker& NavigatorServiceWorker::from(Navigator& navigator) {
  NavigatorServiceWorker* supplement = toNavigatorServiceWorker(navigator);
  if (!supplement) {
    supplement = new NavigatorServiceWorker(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  if (navigator.frame() &&
      navigator.frame()
          ->securityContext()
          ->getSecurityOrigin()
          ->canAccessServiceWorkers()) {
    // Ensure ServiceWorkerContainer. It can be cleared regardless of
    // |supplement|. See comments in NavigatorServiceWorker::serviceWorker() for
    // details.
    supplement->serviceWorker(navigator.frame(), ASSERT_NO_EXCEPTION);
  }
  return *supplement;
}

NavigatorServiceWorker* NavigatorServiceWorker::toNavigatorServiceWorker(
    Navigator& navigator) {
  return static_cast<NavigatorServiceWorker*>(
      Supplement<Navigator>::from(navigator, supplementName()));
}

const char* NavigatorServiceWorker::supplementName() {
  return "NavigatorServiceWorker";
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    ScriptState* scriptState,
    Navigator& navigator,
    ExceptionState& exceptionState) {
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  DCHECK(!navigator.frame() ||
         executionContext->getSecurityOrigin()->canAccessCheckSuborigins(
             navigator.frame()->securityContext()->getSecurityOrigin()));
  return NavigatorServiceWorker::from(navigator).serviceWorker(
      navigator.frame(), exceptionState);
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    ScriptState* scriptState,
    Navigator& navigator,
    String& errorMessage) {
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  DCHECK(!navigator.frame() ||
         executionContext->getSecurityOrigin()->canAccessCheckSuborigins(
             navigator.frame()->securityContext()->getSecurityOrigin()));
  return NavigatorServiceWorker::from(navigator).serviceWorker(
      navigator.frame(), errorMessage);
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    LocalFrame* frame,
    ExceptionState& exceptionState) {
  String errorMessage;
  ServiceWorkerContainer* result = serviceWorker(frame, errorMessage);
  if (!errorMessage.isEmpty()) {
    DCHECK(!result);
    exceptionState.throwSecurityError(errorMessage);
  }
  return result;
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    LocalFrame* frame,
    String& errorMessage) {
  if (frame &&
      !frame->securityContext()
           ->getSecurityOrigin()
           ->canAccessServiceWorkers()) {
    if (frame->securityContext()->isSandboxed(SandboxOrigin)) {
      errorMessage =
          "Service worker is disabled because the context is sandboxed and "
          "lacks the 'allow-same-origin' flag.";
    } else if (frame->securityContext()->getSecurityOrigin()->hasSuborigin()) {
      errorMessage =
          "Service worker is disabled because the context is in a suborigin.";
    } else {
      errorMessage =
          "Access to service workers is denied in this document origin.";
    }
    return nullptr;
  }
  if (!m_serviceWorker && frame) {
    // We need to create a new ServiceWorkerContainer when the frame
    // navigates to a new document. In practice, this happens only when the
    // frame navigates from the initial empty page to a new same-origin page.
    DCHECK(frame->domWindow());
    m_serviceWorker = ServiceWorkerContainer::create(
        frame->domWindow()->getExecutionContext(), this);
  }
  return m_serviceWorker.get();
}

void NavigatorServiceWorker::clearServiceWorker() {
  m_serviceWorker = nullptr;
}

DEFINE_TRACE(NavigatorServiceWorker) {
  visitor->trace(m_serviceWorker);
  Supplement<Navigator>::trace(visitor);
}

}  // namespace blink

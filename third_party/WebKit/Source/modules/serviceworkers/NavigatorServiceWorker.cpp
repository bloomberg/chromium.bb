// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator& navigator) {}

NavigatorServiceWorker* NavigatorServiceWorker::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorServiceWorker& NavigatorServiceWorker::From(Navigator& navigator) {
  NavigatorServiceWorker* supplement = ToNavigatorServiceWorker(navigator);
  if (!supplement) {
    supplement = new NavigatorServiceWorker(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  if (navigator.GetFrame() && navigator.GetFrame()
                                  ->GetSecurityContext()
                                  ->GetSecurityOrigin()
                                  ->CanAccessServiceWorkers()) {
    // Ensure ServiceWorkerContainer. It can be cleared regardless of
    // |supplement|. See comments in NavigatorServiceWorker::serviceWorker() for
    // details.
    supplement->serviceWorker(navigator.GetFrame(), ASSERT_NO_EXCEPTION);
  }
  return *supplement;
}

NavigatorServiceWorker* NavigatorServiceWorker::ToNavigatorServiceWorker(
    Navigator& navigator) {
  return static_cast<NavigatorServiceWorker*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
}

const char* NavigatorServiceWorker::SupplementName() {
  return "NavigatorServiceWorker";
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(!navigator.GetFrame() ||
         execution_context->GetSecurityOrigin()->CanAccess(
             navigator.GetFrame()->GetSecurityContext()->GetSecurityOrigin()));
  return NavigatorServiceWorker::From(navigator).serviceWorker(
      navigator.GetFrame(), exception_state);
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    ScriptState* script_state,
    Navigator& navigator,
    String& error_message) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(!navigator.GetFrame() ||
         execution_context->GetSecurityOrigin()->CanAccess(
             navigator.GetFrame()->GetSecurityContext()->GetSecurityOrigin()));
  return NavigatorServiceWorker::From(navigator).serviceWorker(
      navigator.GetFrame(), error_message);
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    LocalFrame* frame,
    ExceptionState& exception_state) {
  String error_message;
  ServiceWorkerContainer* result = serviceWorker(frame, error_message);
  if (!error_message.IsEmpty()) {
    DCHECK(!result);
    exception_state.ThrowSecurityError(error_message);
  }
  return result;
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    LocalFrame* frame,
    String& error_message) {
  if (frame && !frame->GetSecurityContext()
                    ->GetSecurityOrigin()
                    ->CanAccessServiceWorkers()) {
    if (frame->GetSecurityContext()->IsSandboxed(kSandboxOrigin)) {
      error_message =
          "Service worker is disabled because the context is sandboxed and "
          "lacks the 'allow-same-origin' flag.";
    } else if (frame->GetSecurityContext()
                   ->GetSecurityOrigin()
                   ->HasSuborigin()) {
      error_message =
          "Service worker is disabled because the context is in a suborigin.";
    } else {
      error_message =
          "Access to service workers is denied in this document origin.";
    }
    return nullptr;
  }
  if (!service_worker_ && frame) {
    // We need to create a new ServiceWorkerContainer when the frame
    // navigates to a new document. In practice, this happens only when the
    // frame navigates from the initial empty page to a new same-origin page.
    DCHECK(frame->DomWindow());
    service_worker_ = ServiceWorkerContainer::Create(
        frame->DomWindow()->GetExecutionContext(), this);
  }
  return service_worker_.Get();
}

void NavigatorServiceWorker::ClearServiceWorker() {
  service_worker_ = nullptr;
}

void NavigatorServiceWorker::Trace(blink::Visitor* visitor) {
  visitor->Trace(service_worker_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

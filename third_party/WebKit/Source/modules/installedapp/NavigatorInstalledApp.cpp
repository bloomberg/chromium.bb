// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installedapp/NavigatorInstalledApp.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/installedapp/InstalledAppController.h"
#include "modules/installedapp/RelatedApplication.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/modules/installedapp/WebRelatedApplication.h"

namespace blink {

NavigatorInstalledApp::NavigatorInstalledApp(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorInstalledApp* NavigatorInstalledApp::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorInstalledApp& NavigatorInstalledApp::From(Navigator& navigator) {
  NavigatorInstalledApp* supplement = static_cast<NavigatorInstalledApp*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorInstalledApp(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorInstalledApp::getInstalledRelatedApps(
    ScriptState* script_state,
    Navigator& navigator) {
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());
  return NavigatorInstalledApp::From(navigator).getInstalledRelatedApps(
      script_state);
}

class RelatedAppArray {
  STATIC_ONLY(RelatedAppArray);

 public:
  using WebType = const WebVector<WebRelatedApplication>&;

  static HeapVector<Member<RelatedApplication>> Take(
      ScriptPromiseResolver*,
      const WebVector<WebRelatedApplication>& web_info) {
    HeapVector<Member<RelatedApplication>> applications;
    for (const auto& web_application : web_info)
      applications.push_back(RelatedApplication::Create(
          web_application.platform, web_application.url, web_application.id));
    return applications;
  }
};

ScriptPromise NavigatorInstalledApp::getInstalledRelatedApps(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  InstalledAppController* app_controller = Controller();
  if (!app_controller) {  // If the associated frame is detached
    DOMException* exception = DOMException::Create(
        kInvalidStateError,
        "The object is no longer associated to a document.");
    resolver->Reject(exception);
    return promise;
  }

  if (!app_controller->GetSupplementable()->IsMainFrame()) {
    DOMException* exception =
        DOMException::Create(kInvalidStateError,
                             "getInstalledRelatedApps() is only supported in "
                             "top-level browsing contexts.");
    resolver->Reject(exception);
    return promise;
  }

  app_controller->GetInstalledRelatedApps(WTF::WrapUnique(
      new CallbackPromiseAdapter<RelatedAppArray, void>(resolver)));
  return promise;
}

InstalledAppController* NavigatorInstalledApp::Controller() {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  return InstalledAppController::From(*GetSupplementable()->GetFrame());
}

const char* NavigatorInstalledApp::SupplementName() {
  return "NavigatorInstalledApp";
}

void NavigatorInstalledApp::Trace(blink::Visitor* visitor) {
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

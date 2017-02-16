// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installedapp/NavigatorInstalledApp.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/installedapp/InstalledAppController.h"
#include "modules/installedapp/RelatedApplication.h"
#include "public/platform/modules/installedapp/WebRelatedApplication.h"
#include "wtf/PtrUtil.h"

namespace blink {

NavigatorInstalledApp::NavigatorInstalledApp(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorInstalledApp* NavigatorInstalledApp::from(Document& document) {
  if (!document.frame() || !document.frame()->domWindow())
    return nullptr;
  Navigator& navigator = *document.frame()->domWindow()->navigator();
  return &from(navigator);
}

NavigatorInstalledApp& NavigatorInstalledApp::from(Navigator& navigator) {
  NavigatorInstalledApp* supplement = static_cast<NavigatorInstalledApp*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorInstalledApp(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorInstalledApp::getInstalledRelatedApps(
    ScriptState* scriptState,
    Navigator& navigator) {
  return NavigatorInstalledApp::from(navigator).getInstalledRelatedApps(
      scriptState);
}

class RelatedAppArray {
  STATIC_ONLY(RelatedAppArray);

 public:
  using WebType = const WebVector<WebRelatedApplication>&;

  static HeapVector<Member<RelatedApplication>> take(
      ScriptPromiseResolver*,
      const WebVector<WebRelatedApplication>& webInfo) {
    HeapVector<Member<RelatedApplication>> applications;
    for (const auto& webApplication : webInfo)
      applications.push_back(RelatedApplication::create(
          webApplication.platform, webApplication.url, webApplication.id));
    return applications;
  }
};

ScriptPromise NavigatorInstalledApp::getInstalledRelatedApps(
    ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  InstalledAppController* appController = controller();
  if (!appController) {  // If the associated frame is detached
    DOMException* exception = DOMException::create(
        InvalidStateError, "The object is no longer associated to a document.");
    resolver->reject(exception);
    return promise;
  }

  appController->getInstalledRelatedApps(
      scriptState->getExecutionContext()->getSecurityOrigin(),
      WTF::wrapUnique(
          new CallbackPromiseAdapter<RelatedAppArray, void>(resolver)));
  return promise;
}

InstalledAppController* NavigatorInstalledApp::controller() {
  if (!supplementable()->frame())
    return nullptr;

  return InstalledAppController::from(*supplementable()->frame());
}

const char* NavigatorInstalledApp::supplementName() {
  return "NavigatorInstalledApp";
}

DEFINE_TRACE(NavigatorInstalledApp) {
  Supplement<Navigator>::trace(visitor);
}

}  // namespace blink

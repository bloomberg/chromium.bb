// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerLinkResource.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/html/HTMLLinkElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"
#include "platform/bindings/ScriptState.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/serviceworker/service_worker_error_type.mojom-blink.h"

namespace blink {

namespace {

class RegistrationCallback
    : public WebServiceWorkerProvider::WebServiceWorkerRegistrationCallbacks {
 public:
  explicit RegistrationCallback(LinkLoaderClient* client) : client_(client) {}
  ~RegistrationCallback() override {}

  void OnSuccess(
      std::unique_ptr<WebServiceWorkerRegistration::Handle> handle) override {
    Platform::Current()
        ->CurrentThread()
        ->Scheduler()
        ->TimerTaskRunner()
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&LinkLoaderClient::LinkLoaded, client_));
  }

  void OnError(const WebServiceWorkerError& error) override {
    Platform::Current()
        ->CurrentThread()
        ->Scheduler()
        ->TimerTaskRunner()
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&LinkLoaderClient::LinkLoadingErrored, client_));
  }

 private:
  WTF_MAKE_NONCOPYABLE(RegistrationCallback);

  Persistent<LinkLoaderClient> client_;
};
}

ServiceWorkerLinkResource* ServiceWorkerLinkResource::Create(
    HTMLLinkElement* owner) {
  return new ServiceWorkerLinkResource(owner);
}

ServiceWorkerLinkResource::~ServiceWorkerLinkResource() {}

void ServiceWorkerLinkResource::Process() {
  if (!owner_ || !owner_->GetDocument().GetFrame())
    return;

  if (!owner_->ShouldLoadLink())
    return;

  Document& document = owner_->GetDocument();

  KURL script_url = owner_->Href();

  String scope = owner_->Scope();
  KURL scope_url;
  if (scope.IsNull())
    scope_url = KURL(script_url, "./");
  else
    scope_url = document.CompleteURL(scope);
  scope_url.RemoveFragmentIdentifier();

  String error_message;
  ServiceWorkerContainer* container = NavigatorServiceWorker::serviceWorker(
      ToScriptStateForMainWorld(owner_->GetDocument().GetFrame()),
      *document.GetFrame()->DomWindow()->navigator(), error_message);

  if (!container) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kErrorMessageLevel,
        "Cannot register service worker with <link> element. " +
            error_message));
    WTF::MakeUnique<RegistrationCallback>(owner_)->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kSecurity,
                              error_message));
    return;
  }

  container->RegisterServiceWorkerImpl(
      &document, script_url, scope_url,
      WTF::MakeUnique<RegistrationCallback>(owner_));
}

bool ServiceWorkerLinkResource::HasLoaded() const {
  return false;
}

void ServiceWorkerLinkResource::OwnerRemoved() {
  Process();
}

ServiceWorkerLinkResource::ServiceWorkerLinkResource(HTMLLinkElement* owner)
    : LinkResource(owner) {}

}  // namespace blink

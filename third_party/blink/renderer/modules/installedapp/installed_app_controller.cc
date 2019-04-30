// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"

#include <utility>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

InstalledAppController::~InstalledAppController() = default;

void InstalledAppController::GetInstalledRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks) {
  // When detached, the fetch logic is no longer valid.
  if (context_destroyed_) {
    // TODO(mgiuca): AbortError rather than simply undefined.
    // https://crbug.com/687846
    callbacks->OnError();
    return;
  }

  // Get the list of related applications from the manifest.
  // Upon returning, filter the result list to those apps that are installed.
  ManifestManager::From(*GetSupplementable())
      ->RequestManifest(
          WTF::Bind(&InstalledAppController::OnGetManifestForRelatedApps,
                    WrapPersistent(this), std::move(callbacks)));
}

void InstalledAppController::ProvideTo(LocalFrame& frame) {
  Supplement<LocalFrame>::ProvideTo(
      frame, MakeGarbageCollected<InstalledAppController>(frame));
}

InstalledAppController* InstalledAppController::From(LocalFrame& frame) {
  InstalledAppController* controller =
      Supplement<LocalFrame>::From<InstalledAppController>(frame);
  DCHECK(controller);
  return controller;
}

const char InstalledAppController::kSupplementName[] = "InstalledAppController";

InstalledAppController::InstalledAppController(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.GetDocument()) {}

void InstalledAppController::ContextDestroyed(ExecutionContext*) {
  provider_.reset();
  context_destroyed_ = true;
}

void InstalledAppController::OnGetManifestForRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks,
    const WebURL& /*url*/,
    const Manifest& manifest) {
  Vector<mojom::blink::RelatedApplicationPtr> mojo_related_apps;
  for (const Manifest::RelatedApplication& related_application :
       manifest.related_applications) {
    mojom::blink::RelatedApplicationPtr converted_application(
        mojom::blink::RelatedApplication::New());
    converted_application->platform =
        WebString::FromUTF16(related_application.platform);
    converted_application->id = WebString::FromUTF16(related_application.id);
    if (!related_application.url.is_empty()) {
      converted_application->url =
          WebString::FromUTF8(related_application.url.spec());
    }
    mojo_related_apps.push_back(std::move(converted_application));
  }

  if (!provider_) {
    // See https://bit.ly/2S0zRAS for task types.
    GetSupplementable()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&provider_, GetExecutionContext()->GetTaskRunner(
                                          blink::TaskType::kMiscPlatformAPI)));
    // TODO(mgiuca): Set a connection error handler. This requires a refactor to
    // work like NavigatorShare.cpp (retain a persistent list of clients to
    // reject all of their promises).
    DCHECK(provider_);
  }

  provider_->FilterInstalledApps(
      std::move(mojo_related_apps),
      WTF::Bind(&InstalledAppController::OnFilterInstalledApps,
                WrapPersistent(this), WTF::Passed(std::move(callbacks))));
}

void InstalledAppController::OnFilterInstalledApps(
    std::unique_ptr<blink::AppInstalledCallbacks> callbacks,
    Vector<mojom::blink::RelatedApplicationPtr> result) {
  Vector<blink::WebRelatedApplication> applications;
  for (const auto& res : result) {
    blink::WebRelatedApplication app;
    app.platform = res->platform;
    app.url = res->url;
    app.id = res->id;
    applications.push_back(app);
  }
  callbacks->OnSuccess(applications);
}

void InstalledAppController::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink

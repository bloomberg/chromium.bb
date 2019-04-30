// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/installedapp/related_apps_fetcher.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#include <utility>

namespace blink {

InstalledAppController::~InstalledAppController() = default;

void InstalledAppController::GetInstalledRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks) {
  // When detached, the fetcher is no longer valid.
  if (!related_apps_fetcher_) {
    // TODO(mgiuca): AbortError rather than simply undefined.
    // https://crbug.com/687846
    callbacks->OnError();
    return;
  }

  // Get the list of related applications from the manifest. This requires a
  // request to the content layer (because the manifest is not a Blink concept).
  // Upon returning, filter the result list to those apps that are installed.
  // TODO(mgiuca): This roundtrip to content could be eliminated if the Manifest
  // class was moved from content into Blink.
  related_apps_fetcher_->GetManifestRelatedApplications(
      WTF::Bind(&InstalledAppController::OnGetRelatedAppsCallback,
                WrapWeakPersistent(this), std::move(callbacks)));
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
      ContextLifecycleObserver(frame.GetDocument()),
      related_apps_fetcher_(MakeGarbageCollected<RelatedAppsFetcher>(
          ManifestManager::From(frame))) {}

void InstalledAppController::ContextDestroyed(ExecutionContext*) {
  provider_.reset();
  related_apps_fetcher_ = nullptr;
}

void InstalledAppController::OnGetRelatedAppsCallback(
    std::unique_ptr<AppInstalledCallbacks> callbacks,
    const WebVector<WebRelatedApplication>& related_apps) {
  // TODO: Fix the order of the parameters in ::FilterByInstalledApps
  // and bound it right away as the completion callback.
  FilterByInstalledApps(related_apps, std::move(callbacks));
}

void InstalledAppController::FilterByInstalledApps(
    const blink::WebVector<blink::WebRelatedApplication>& related_apps,
    std::unique_ptr<blink::AppInstalledCallbacks> callbacks) {
  WTF::Vector<mojom::blink::RelatedApplicationPtr> mojo_related_apps;
  for (const auto& related_application : related_apps) {
    mojom::blink::RelatedApplicationPtr converted_application(
        mojom::blink::RelatedApplication::New());
    DCHECK(!related_application.platform.IsEmpty());
    converted_application->platform = related_application.platform;
    converted_application->id = related_application.id;
    converted_application->url = related_application.url;
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
    WTF::Vector<mojom::blink::RelatedApplicationPtr> result) {
  WTF::Vector<blink::WebRelatedApplication> applications;
  for (const auto& res : result) {
    blink::WebRelatedApplication app;
    app.platform = res->platform;
    app.url = res->url;
    app.id = res->id;
    applications.push_back(app);
  }
  callbacks->OnSuccess(
      blink::WebVector<blink::WebRelatedApplication>(applications));
}

void InstalledAppController::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(related_apps_fetcher_);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installedapp/InstalledAppController.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/InterfaceProvider.h"
#include "wtf/Functional.h"

#include <utility>

namespace blink {

// Callbacks for the result of
// WebRelatedAppsFetcher::getManifestRelatedApplications. Calls
// filterByInstalledApps upon receiving the list of related applications.
class InstalledAppController::GetRelatedAppsCallbacks
    : public AppInstalledCallbacks {
 public:
  GetRelatedAppsCallbacks(InstalledAppController* controller,
                          std::unique_ptr<AppInstalledCallbacks> callbacks)
      : m_controller(controller),
        m_callbacks(std::move(callbacks)) {}

  // AppInstalledCallbacks overrides:
  void onSuccess(const WebVector<WebRelatedApplication>& relatedApps) override {
    if (!m_controller)
      return;

    m_controller->filterByInstalledApps(relatedApps, std::move(m_callbacks));
  }
  void onError() override { m_callbacks->onError(); }

 private:
  WeakPersistent<InstalledAppController> m_controller;
  std::unique_ptr<AppInstalledCallbacks> m_callbacks;
};

InstalledAppController::~InstalledAppController() {}

void InstalledAppController::getInstalledRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks) {
  // When detached, the fetcher is no longer valid.
  if (!m_relatedAppsFetcher) {
    // TODO(mgiuca): AbortError rather than simply undefined.
    // https://crbug.com/687846
    callbacks->onError();
    return;
  }

  // Get the list of related applications from the manifest. This requires a
  // request to the content layer (because the manifest is not a Blink concept).
  // Upon returning, filter the result list to those apps that are installed.
  // TODO(mgiuca): This roundtrip to content could be eliminated if the Manifest
  // class was moved from content into Blink.
  m_relatedAppsFetcher->getManifestRelatedApplications(
      WTF::makeUnique<GetRelatedAppsCallbacks>(this, std::move(callbacks)));
}

void InstalledAppController::provideTo(
    LocalFrame& frame,
    WebRelatedAppsFetcher* relatedAppsFetcher) {
  DCHECK(RuntimeEnabledFeatures::installedAppEnabled());

  InstalledAppController* controller =
      new InstalledAppController(frame, relatedAppsFetcher);
  Supplement<LocalFrame>::provideTo(frame, supplementName(), controller);
}

InstalledAppController* InstalledAppController::from(LocalFrame& frame) {
  InstalledAppController* controller = static_cast<InstalledAppController*>(
      Supplement<LocalFrame>::from(frame, supplementName()));
  DCHECK(controller);
  return controller;
}

const char* InstalledAppController::supplementName() {
  return "InstalledAppController";
}

InstalledAppController::InstalledAppController(
    LocalFrame& frame,
    WebRelatedAppsFetcher* relatedAppsFetcher)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.document()),
      m_relatedAppsFetcher(relatedAppsFetcher) {}

void InstalledAppController::contextDestroyed(ExecutionContext*) {
  m_provider.reset();
  m_relatedAppsFetcher = nullptr;
}

void InstalledAppController::filterByInstalledApps(
    const blink::WebVector<blink::WebRelatedApplication>& relatedApps,
    std::unique_ptr<blink::AppInstalledCallbacks> callbacks) {
  WTF::Vector<mojom::blink::RelatedApplicationPtr> mojoRelatedApps;
  for (const auto& relatedApplication : relatedApps) {
    mojom::blink::RelatedApplicationPtr convertedApplication(
        mojom::blink::RelatedApplication::New());
    DCHECK(!relatedApplication.platform.isEmpty());
    convertedApplication->platform = relatedApplication.platform;
    convertedApplication->id = relatedApplication.id;
    convertedApplication->url = relatedApplication.url;
    mojoRelatedApps.push_back(std::move(convertedApplication));
  }

  if (!m_provider) {
    supplementable()->interfaceProvider()->getInterface(
        mojo::MakeRequest(&m_provider));
    // TODO(mgiuca): Set a connection error handler. This requires a refactor to
    // work like NavigatorShare.cpp (retain a persistent list of clients to
    // reject all of their promises).
    DCHECK(m_provider);
  }

  m_provider->FilterInstalledApps(
      std::move(mojoRelatedApps),
      convertToBaseCallback(
          WTF::bind(&InstalledAppController::OnFilterInstalledApps,
                    wrapPersistent(this), WTF::passed(std::move(callbacks)))));
}

void InstalledAppController::OnFilterInstalledApps(
    std::unique_ptr<blink::AppInstalledCallbacks> callbacks,
    WTF::Vector<mojom::blink::RelatedApplicationPtr> result) {
  std::vector<blink::WebRelatedApplication> applications;
  for (const auto& res : result) {
    blink::WebRelatedApplication app;
    app.platform = res->platform;
    app.url = res->url;
    app.id = res->id;
    applications.push_back(app);
  }
  callbacks->onSuccess(
      blink::WebVector<blink::WebRelatedApplication>(applications));
}

DEFINE_TRACE(InstalledAppController) {
  Supplement<LocalFrame>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink

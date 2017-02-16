// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installedapp/InstalledAppController.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"
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
                          WTF::RefPtr<SecurityOrigin> url,
                          std::unique_ptr<AppInstalledCallbacks> callbacks)
      : m_controller(controller),
        m_url(url),
        m_callbacks(std::move(callbacks)) {}

  // AppInstalledCallbacks overrides:
  void onSuccess(const WebVector<WebRelatedApplication>& relatedApps) override {
    if (!m_controller)
      return;

    m_controller->filterByInstalledApps(m_url, relatedApps,
                                        std::move(m_callbacks));
  }
  void onError() override { m_callbacks->onError(); }

 private:
  WeakPersistent<InstalledAppController> m_controller;
  WTF::RefPtr<SecurityOrigin> m_url;
  std::unique_ptr<AppInstalledCallbacks> m_callbacks;
};

InstalledAppController::~InstalledAppController() {}

void InstalledAppController::getInstalledRelatedApps(
    WTF::RefPtr<SecurityOrigin> url,
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
      WTF::makeUnique<GetRelatedAppsCallbacks>(this, url,
                                               std::move(callbacks)));
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
  m_relatedAppsFetcher = nullptr;
}

void InstalledAppController::filterByInstalledApps(
    WTF::RefPtr<SecurityOrigin> /*origin*/,
    const WebVector<WebRelatedApplication>& /*relatedApps*/,
    std::unique_ptr<AppInstalledCallbacks> callbacks) {
  // TODO(mgiuca): Call through to the browser to filter the list of
  // applications down to just those that are installed on the device.
  // For now, just return the empty list (no applications are installed).
  callbacks->onSuccess(WebVector<WebRelatedApplication>());
}

DEFINE_TRACE(InstalledAppController) {
  Supplement<LocalFrame>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink

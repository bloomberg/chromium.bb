// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/related_apps_fetcher.h"

#include <utility>

#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/platform/modules/installedapp/web_related_application.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

RelatedAppsFetcher::RelatedAppsFetcher(ManifestManager* manifest_manager)
    : manifest_manager_(manifest_manager) {}

void RelatedAppsFetcher::GetManifestRelatedApplications(
    GetManifestRelatedApplicationsCallback completion_callback) {
  manifest_manager_->RequestManifest(
      WTF::Bind(&RelatedAppsFetcher::OnGetManifestForRelatedApplications,
                WrapWeakPersistent(this), std::move(completion_callback)));
}

void RelatedAppsFetcher::OnGetManifestForRelatedApplications(
    GetManifestRelatedApplicationsCallback completion_callback,
    const WebURL& /*url*/,
    const Manifest& manifest) {
  WTF::Vector<WebRelatedApplication> web_related_applications;
  for (const auto& related_application : manifest.related_applications) {
    WebRelatedApplication web_related_application;
    web_related_application.platform =
        WebString::FromUTF16(related_application.platform);
    web_related_application.id = WebString::FromUTF16(related_application.id);
    if (!related_application.url.is_empty()) {
      web_related_application.url =
          WebString::FromUTF8(related_application.url.spec());
    }
    web_related_applications.push_back(std::move(web_related_application));
  }
  std::move(completion_callback).Run(std::move(web_related_applications));
}

void RelatedAppsFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(manifest_manager_);
}

}  // namespace blink

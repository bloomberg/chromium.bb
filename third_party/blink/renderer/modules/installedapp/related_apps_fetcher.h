// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_RELATED_APPS_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_RELATED_APPS_FETCHER_H_

#include <string>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ManifestManager;
class WebURL;
template <typename T>
class WebVector;
struct Manifest;
struct WebRelatedApplication;

class RelatedAppsFetcher : public GarbageCollected<RelatedAppsFetcher> {
 public:
  explicit RelatedAppsFetcher(ManifestManager* manifest_manager);

  using GetManifestRelatedApplicationsCallback =
      base::OnceCallback<void(const WebVector<WebRelatedApplication>&)>;
  void GetManifestRelatedApplications(
      GetManifestRelatedApplicationsCallback completion_callback);

  void Trace(blink::Visitor*);

 private:
  // Callback for when the manifest has been fetched, triggered by a call to
  // GetManifestRelatedApplications.
  void OnGetManifestForRelatedApplications(
      GetManifestRelatedApplicationsCallback completion_callback,
      const WebURL& url,
      const Manifest& manifest);

  Member<ManifestManager> manifest_manager_;

  DISALLOW_COPY_AND_ASSIGN(RelatedAppsFetcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_RELATED_APPS_FETCHER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INSTALLEDAPP_WEB_RELATED_APPS_FETCHER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INSTALLEDAPP_WEB_RELATED_APPS_FETCHER_H_

#include "base/callback.h"
#include "third_party/blink/public/platform/modules/installedapp/web_related_application.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"

#include <memory>

namespace blink {

using GetManifestRelatedApplicationsCallback =
    base::OnceCallback<void(const WebVector<WebRelatedApplication>&)>;

class WebRelatedAppsFetcher {
 public:
  virtual ~WebRelatedAppsFetcher() = default;

  // Gets the list of related applications from the web frame's manifest.
  virtual void GetManifestRelatedApplications(
      GetManifestRelatedApplicationsCallback) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INSTALLEDAPP_WEB_RELATED_APPS_FETCHER_H_

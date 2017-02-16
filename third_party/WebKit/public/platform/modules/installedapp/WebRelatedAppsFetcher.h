// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRelatedAppsFetcher_h
#define WebRelatedAppsFetcher_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/installedapp/WebRelatedApplication.h"

#include <memory>

namespace blink {

using AppInstalledCallbacks =
    WebCallbacks<const WebVector<WebRelatedApplication>&, void>;

class WebRelatedAppsFetcher {
 public:
  virtual ~WebRelatedAppsFetcher() {}

  // Gets the list of related applications from the web frame's manifest.
  virtual void getManifestRelatedApplications(
      std::unique_ptr<AppInstalledCallbacks>) = 0;
};

}  // namespace blink

#endif  // WebRelatedAppsFetcher_h

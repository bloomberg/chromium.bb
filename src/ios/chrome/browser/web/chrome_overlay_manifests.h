// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_CHROME_OVERLAY_MANIFESTS_H_
#define IOS_CHROME_BROWSER_WEB_CHROME_OVERLAY_MANIFESTS_H_

#include "services/service_manager/public/cpp/manifest.h"

// Returns the manifest Chrome amends to the web_browser service manifest. This
// allows Chrome to extend the capabilities exposed and/or required by
// web_browser service instances.
const service_manager::Manifest& GetChromeWebBrowserOverlayManifest();

// Returns the manifest Chrome amends to the web_packaged_services service
// manifest. This allows Chrome to extend the set of in-process services
// packaged by the browser.
const service_manager::Manifest& GetChromeWebPackagedServicesOverlayManifest();

#endif  // IOS_CHROME_BROWSER_WEB_CHROME_OVERLAY_MANIFESTS_H_

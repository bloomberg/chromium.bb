// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FEATURES_H_
#define IOS_CHROME_BROWSER_WEB_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace web {

// Used to control the state of the WebPageTextAccessibility feature.
extern const base::Feature kWebPageTextAccessibility;

// Feature flag to keep the mobile version for Google SRP. Should be used when
// the desktop version is requested by default.
extern const base::Feature kMobileGoogleSRP;

// Feature flag to reload the page instead of showing the SadTab when the
// renderer crashes.
extern const base::Feature kReloadSadTab;

}  // namespace web

#endif  // IOS_CHROME_BROWSER_WEB_FEATURES_H_

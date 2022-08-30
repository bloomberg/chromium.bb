// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_
#define COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_

#include "build/build_config.h"

namespace base {
struct Feature;
}  // namespace base

namespace webapps {
namespace features {

#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kAddToHomescreenMessaging;
extern const base::Feature kInstallableAmbientBadgeInfoBar;
extern const base::Feature kInstallableAmbientBadgeMessage;
#endif  // BUILDFLAG(IS_ANDROID)

extern const base::Feature kSkipServiceWorkerCheckAll;
extern const base::Feature kSkipServiceWorkerCheckInstallOnly;

bool SkipBannerServiceWorkerCheck();
bool SkipInstallServiceWorkerCheck();

}  // namespace features
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_

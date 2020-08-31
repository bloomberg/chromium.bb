// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/features.h"

namespace web {

const base::Feature kWebPageTextAccessibility{
    "WebPageTextAccessibility", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMobileGoogleSRP{"MobileGoogleSRP",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReloadSadTab{"ReloadSadTab",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace web

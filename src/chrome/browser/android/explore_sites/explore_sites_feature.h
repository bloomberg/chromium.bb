// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FEATURE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FEATURE_H_

namespace chrome {
namespace android {
namespace explore_sites {

extern const char kExploreSitesVariationParameterName[];

extern const char kExploreSitesVariationExperimental[];

enum class ExploreSitesVariation { ENABLED, EXPERIMENT, DISABLED };

ExploreSitesVariation GetExploreSitesVariation();

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FEATURE_H_

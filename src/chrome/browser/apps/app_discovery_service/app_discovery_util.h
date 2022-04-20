// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "chrome/browser/apps/app_discovery_service/result.h"

namespace apps {

enum class ResultType {
  kTestType,
  kRecommendedArcApps,
  kGameSearchCatalog,
};

enum class AppSource {
  kTestSource,
  kPlay,
  kGames,
};

enum class DiscoveryError {
  kSuccess,             // Successfully got app data to return.
  kErrorRequestFailed,  // Failed to get requested data.
  kErrorMalformedData   // Failed to parse received data.
};

using ResultCallback =
    base::OnceCallback<void(const std::vector<Result>& results,
                            DiscoveryError error)>;

using RepeatingResultCallback =
    base::RepeatingCallback<void(const std::vector<Result>& results)>;

using ResultCallbackList =
    base::RepeatingCallbackList<void(const std::vector<Result>& results)>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_USER_POPULATION_HELPER_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_USER_POPULATION_HELPER_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

// A convenience function that creates a ChromeUserPopulation proto for the
// given |browser_context| and populates it appropriately for WebLayer.
safe_browsing::ChromeUserPopulation GetUserPopulationForBrowserContext(
    content::BrowserContext* browser_context);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_USER_POPULATION_HELPER_H_

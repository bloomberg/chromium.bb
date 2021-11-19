// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/signin_switches.h"

namespace signin {

bool ForceStartupSigninPromo() {
  return base::FeatureList::IsEnabled(switches::kForceStartupSigninPromo);
}

bool ForceDisableExtendedSyncPromos() {
  return base::FeatureList::IsEnabled(
      switches::kForceDisableExtendedSyncPromos);
}

const char kDelayThresholdMinutesToUpdateGaiaCookie[] =
    "minutes-delay-to-restore-gaia-cookies-if-deleted";

const char kWaitThresholdMillisecondsForCapabilitiesApi[] =
    "wait-threshold-milliseconds-for-capabilities-api";

}  // namespace signin

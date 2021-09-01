// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/commerce_feature_list.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace commerce {

const base::Feature kCommerceMerchantViewer{"CommerceMerchantViewer",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommercePriceTracking{"CommercePriceTracking",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory{
    &kCommerceMerchantViewer, "delete_all_merchants_on_clear_history", false};
}  // namespace commerce

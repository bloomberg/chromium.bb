// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FEATURES_H_
#define COMPONENTS_GCM_DRIVER_FEATURES_H_

#include "base/feature_list.h"

namespace gcm {

namespace features {

extern const base::Feature kInvalidateTokenFeature;
extern const char kParamNameTokenInvalidationPeriodDays[];

// The period after which the GCM token becomes stale.
base::TimeDelta GetTokenInvalidationInterval();

}  // namespace features

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FEATURES_H_

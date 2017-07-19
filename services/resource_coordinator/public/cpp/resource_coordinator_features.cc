// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace features {

// Globally enable the GRC.
const base::Feature kGlobalResourceCoordinator{
    "GlobalResourceCoordinator", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features

namespace resource_coordinator {

bool IsResourceCoordinatorEnabled() {
  return base::FeatureList::IsEnabled(features::kGlobalResourceCoordinator);
}

}  // namespace resource_coordinator

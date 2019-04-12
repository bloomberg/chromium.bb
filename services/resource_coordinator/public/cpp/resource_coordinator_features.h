// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/resource_coordinator module.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_FEATURES_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

#if defined(OS_WIN)
extern const COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_FEATURES)
    base::Feature kEmptyWorkingSet;
#endif

}  // namespace features

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_RESOURCE_COORDINATOR_FEATURES_H_

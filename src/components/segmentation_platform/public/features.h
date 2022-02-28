// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace segmentation_platform {
namespace features {

// Core feature flag for segmentation platform.
extern const base::Feature kSegmentationPlatformFeature;

// Feature flag for segmentation platform dummy model that is used for
// experimental models and data collection.
extern const base::Feature kSegmentationPlatformDummyFeature;

// Feature flag for using a segmentation based model for query tiles to decide
// whether or not to show query tiles.
extern const base::Feature kSegmentationPlatformQueryTilesFeature;

}  // namespace features
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_

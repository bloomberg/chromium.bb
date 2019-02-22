// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_features.h"

namespace display {
namespace features {

const base::Feature kHighDynamicRange{"HighDynamicRange",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Enables using the monitor's provided color space information when
// rendering.
// TODO(mcasas): remove this flag http://crbug.com/771345.
const base::Feature kUseMonitorColorSpace{"UseMonitorColorSpace",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // OS_CHROMEOS

// This features allows listing all display modes of external displays in the
// display settings and setting any one of them exactly as requested, which can
// be very useful for debugging and development purposes.
const base::Feature kListAllDisplayModes = {"ListAllDisplayModes",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

bool IsListAllDisplayModesEnabled() {
  return base::FeatureList::IsEnabled(kListAllDisplayModes);
}

}  // namespace features
}  // namespace display

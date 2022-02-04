
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/features.h"
#include "build/chromeos_buildflags.h"

namespace ui {

const base::Feature kWaylandOverlayDelegation{"WaylandOverlayDelegation",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// This feature flag enables a mode where the wayland client would submit
// buffers at a scale of 1 and the server applies the respective scale transform
// to properly composite the buffers. This mode is used to support fractional
// scale factor.
const base::Feature kWaylandSurfaceSubmissionInPixelCoordinates{
    "WaylandSurfaceSubmissionInPixelCoordinates",
    base::FEATURE_DISABLED_BY_DEFAULT};


bool IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() {
  return base::FeatureList::IsEnabled(
      kWaylandSurfaceSubmissionInPixelCoordinates);
}

bool IsWaylandOverlayDelegationEnabled() {
  return base::FeatureList::IsEnabled(kWaylandOverlayDelegation);
}

}  // namespace ui

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane_detection_state.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_plane_detection_state_init.h"
namespace blink {

XRPlaneDetectionState::XRPlaneDetectionState(
    XRPlaneDetectionStateInit* plane_detection_state_init) {
  if (plane_detection_state_init) {
    if (plane_detection_state_init->hasEnabled()) {
      enabled_ = plane_detection_state_init->enabled();
    }
  }
}

}  // namespace blink

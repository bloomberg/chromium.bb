// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_location.h"

#include "base/logging.h"

namespace device {

MockWMRInputLocation::MockWMRInputLocation(ControllerFrameData data)
    : data_(data) {}

MockWMRInputLocation::~MockWMRInputLocation() = default;

bool MockWMRInputLocation::TryGetPosition(
    ABI::Windows::Foundation::Numerics::Vector3* position) const {
  DCHECK(position);
  // TODO(https://crbug.com/926048): Properly implement.
  position->X = 0;
  position->Y = 0;
  position->Z = 0;
  return true;
}

bool MockWMRInputLocation::TryGetVelocity(
    ABI::Windows::Foundation::Numerics::Vector3* velocity) const {
  DCHECK(velocity);
  // TODO(https://crbug.com/926048): Properly implement.
  velocity->X = 0;
  velocity->Y = 0;
  velocity->Z = 0;
  return true;
}

bool MockWMRInputLocation::TryGetOrientation(
    ABI::Windows::Foundation::Numerics::Quaternion* orientation) const {
  DCHECK(orientation);
  // TODO(https://crbug.com/926048): Properly implement.
  orientation->X = 0;
  orientation->Y = 0;
  orientation->Z = 0;
  orientation->W = 1;
  return true;
}

bool MockWMRInputLocation::TryGetAngularVelocity(
    ABI::Windows::Foundation::Numerics::Vector3* angular_velocity) const {
  DCHECK(angular_velocity);
  // TODO(https://crbug.com/926048): Properly implement.
  angular_velocity->X = 0;
  angular_velocity->Y = 0;
  angular_velocity->Z = 0;
  return true;
}

}  // namespace device

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_location.h"

#include "base/logging.h"
#include "device/vr/test/test_hook.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform_util.h"

namespace device {

MockWMRInputLocation::MockWMRInputLocation(ControllerFrameData data)
    : data_(data) {
  DecomposeTransform(&decomposed_device_to_origin_,
                     PoseFrameDataToTransform(data.pose_data));
}

MockWMRInputLocation::~MockWMRInputLocation() = default;

bool MockWMRInputLocation::TryGetPosition(
    ABI::Windows::Foundation::Numerics::Vector3* position) const {
  DCHECK(position);
  if (!data_.pose_data.is_valid)
    return false;

  position->X = decomposed_device_to_origin_.translate[0];
  position->Y = decomposed_device_to_origin_.translate[1];
  position->Z = decomposed_device_to_origin_.translate[2];
  return true;
}

bool MockWMRInputLocation::TryGetVelocity(
    ABI::Windows::Foundation::Numerics::Vector3* velocity) const {
  DCHECK(velocity);
  if (!data_.pose_data.is_valid)
    return false;
  // We could potentially store a history of poses and calculate the velocity,
  // but that is more complicated and doesn't currently provide any benefit for
  // tests. So, just report 0s.
  velocity->X = 0;
  velocity->Y = 0;
  velocity->Z = 0;
  return true;
}

bool MockWMRInputLocation::TryGetOrientation(
    ABI::Windows::Foundation::Numerics::Quaternion* orientation) const {
  DCHECK(orientation);
  if (!data_.pose_data.is_valid)
    return false;

  orientation->X = decomposed_device_to_origin_.quaternion.x();
  orientation->Y = decomposed_device_to_origin_.quaternion.y();
  orientation->Z = decomposed_device_to_origin_.quaternion.z();
  orientation->W = decomposed_device_to_origin_.quaternion.w();
  return true;
}

bool MockWMRInputLocation::TryGetAngularVelocity(
    ABI::Windows::Foundation::Numerics::Vector3* angular_velocity) const {
  DCHECK(angular_velocity);
  if (!data_.pose_data.is_valid)
    return false;
  // We could potentially store a history of poses and calculate the angular
  // velocity, but that is more complicated and doesn't currently provide any
  // benefit for tests. So, just report 0s.
  angular_velocity->X = 0;
  angular_velocity->Y = 0;
  angular_velocity->Z = 0;
  return true;
}

}  // namespace device

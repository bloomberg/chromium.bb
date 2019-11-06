// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_OPENVR_GAMEPAD_HELPER_H_
#define DEVICE_VR_OPENVR_OPENVR_GAMEPAD_HELPER_H_

#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "third_party/openvr/src/headers/openvr.h"

namespace device {

class OpenVRGamepadHelper {
 public:
  static mojom::XRGamepadDataPtr GetGamepadData(vr::IVRSystem* system);
  static base::Optional<Gamepad> GetXRGamepad(
      vr::IVRSystem* system,
      uint32_t controller_id,
      vr::VRControllerState_t controller_state,
      device::mojom::XRHandedness handedness);
};

}  // namespace device
#endif  // DEVICE_VR_OPENVR_OPENVR_GAMEPAD_HELPER_H_

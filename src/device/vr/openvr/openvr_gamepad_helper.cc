// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_gamepad_helper.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/vr_device.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

mojom::XRGamepadButtonPtr GetGamepadButton(
    const vr::VRControllerState_t& controller_state,
    uint64_t supported_buttons,
    vr::EVRButtonId button_id) {
  uint64_t button_mask = vr::ButtonMaskFromId(button_id);
  if ((supported_buttons & button_mask) != 0) {
    auto ret = mojom::XRGamepadButton::New();
    bool button_pressed = (controller_state.ulButtonPressed & button_mask) != 0;
    bool button_touched = (controller_state.ulButtonTouched & button_mask) != 0;
    ret->touched = button_touched;
    ret->pressed = button_pressed;
    ret->value = button_pressed ? 1.0 : 0.0;
    return ret;
  }

  return nullptr;
}

}  // namespace

mojom::XRGamepadDataPtr OpenVRGamepadHelper::GetGamepadData(
    vr::IVRSystem* vr_system) {
  mojom::XRGamepadDataPtr ret = mojom::XRGamepadData::New();

  vr::TrackedDevicePose_t tracked_devices_poses[vr::k_unMaxTrackedDeviceCount];
  vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, 0.0f,
                                             tracked_devices_poses,
                                             vr::k_unMaxTrackedDeviceCount);
  for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
    if (vr_system->GetTrackedDeviceClass(i) !=
        vr::TrackedDeviceClass_Controller)
      continue;

    vr::VRControllerState_t controller_state;
    bool have_state = vr_system->GetControllerState(i, &controller_state,
                                                    sizeof(controller_state));
    if (!have_state)
      continue;

    auto gamepad = mojom::XRGamepad::New();
    gamepad->controller_id = i;

    vr::ETrackedControllerRole hand =
        vr_system->GetControllerRoleForTrackedDeviceIndex(i);
    switch (hand) {
      case vr::TrackedControllerRole_Invalid:
        gamepad->hand = device::mojom::XRHandedness::NONE;
        break;
      case vr::TrackedControllerRole_LeftHand:
        gamepad->hand = device::mojom::XRHandedness::LEFT;
        break;
      case vr::TrackedControllerRole_RightHand:
        gamepad->hand = device::mojom::XRHandedness::RIGHT;
        break;
    }

    uint64_t supported_buttons = vr_system->GetUint64TrackedDeviceProperty(
        i, vr::Prop_SupportedButtons_Uint64);
    for (uint32_t j = 0; j < vr::k_unControllerStateAxisCount; ++j) {
      int32_t axis_type = vr_system->GetInt32TrackedDeviceProperty(
          i,
          static_cast<vr::TrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + j));
      switch (axis_type) {
        case vr::k_eControllerAxis_Joystick:
        case vr::k_eControllerAxis_TrackPad: {
          gamepad->axes.push_back(controller_state.rAxis[j].x);
          gamepad->axes.push_back(controller_state.rAxis[j].y);
          auto button = GetGamepadButton(
              controller_state, supported_buttons,
              static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + j));
          if (button) {
            gamepad->buttons.push_back(std::move(button));
          }
        } break;
        case vr::k_eControllerAxis_Trigger: {
          auto button = mojom::XRGamepadButton::New();
          button->value = controller_state.rAxis[j].x;
          uint64_t button_mask = vr::ButtonMaskFromId(
              static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + j));
          if ((supported_buttons & button_mask) != 0) {
            button->pressed =
                (controller_state.ulButtonPressed & button_mask) != 0;
          }
          gamepad->buttons.push_back(std::move(button));
        } break;
      }
    }

    auto button =
        GetGamepadButton(controller_state, supported_buttons, vr::k_EButton_A);
    if (button)
      gamepad->buttons.push_back(std::move(button));
    button = GetGamepadButton(controller_state, supported_buttons,
                              vr::k_EButton_Grip);
    if (button)
      gamepad->buttons.push_back(std::move(button));
    button = GetGamepadButton(controller_state, supported_buttons,
                              vr::k_EButton_ApplicationMenu);
    if (button)
      gamepad->buttons.push_back(std::move(button));
    button = GetGamepadButton(controller_state, supported_buttons,
                              vr::k_EButton_DPad_Left);
    if (button)
      gamepad->buttons.push_back(std::move(button));
    button = GetGamepadButton(controller_state, supported_buttons,
                              vr::k_EButton_DPad_Up);
    if (button)
      gamepad->buttons.push_back(std::move(button));
    button = GetGamepadButton(controller_state, supported_buttons,
                              vr::k_EButton_DPad_Right);
    if (button)
      gamepad->buttons.push_back(std::move(button));
    button = GetGamepadButton(controller_state, supported_buttons,
                              vr::k_EButton_DPad_Down);
    if (button)
      gamepad->buttons.push_back(std::move(button));

    const vr::TrackedDevicePose_t& pose = tracked_devices_poses[i];
    if (pose.bPoseIsValid) {
      const vr::HmdMatrix34_t& mat = pose.mDeviceToAbsoluteTracking;
      gfx::Transform transform(
          mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3], mat.m[1][0],
          mat.m[1][1], mat.m[1][2], mat.m[1][3], mat.m[2][0], mat.m[2][1],
          mat.m[2][2], mat.m[2][3], 0, 0, 0, 1);

      gfx::DecomposedTransform src_pose;
      gfx::DecomposeTransform(&src_pose, transform);
      auto dst_pose = mojom::VRPose::New();

      dst_pose->orientation = std::vector<float>(
          {src_pose.quaternion.x(), src_pose.quaternion.y(),
           src_pose.quaternion.z(), src_pose.quaternion.w()});
      dst_pose->position =
          std::vector<float>({src_pose.translate[0], src_pose.translate[1],
                              src_pose.translate[2]});
      dst_pose->angularVelocity = std::vector<float>(
          {pose.vAngularVelocity.v[0], pose.vAngularVelocity.v[1],
           pose.vAngularVelocity.v[2]});
      dst_pose->linearVelocity = std::vector<float>(
          {pose.vVelocity.v[0], pose.vVelocity.v[1], pose.vVelocity.v[2]});

      gamepad->pose = std::move(dst_pose);
    }

    ret->gamepads.push_back(std::move(gamepad));
  }

  return ret;
}

}  // namespace device

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_controller.h"

#include <stdint.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

const char* GetStringFromType(OpenXrHandednessType type) {
  switch (type) {
    case OpenXrHandednessType::kLeft:
      return "left";
    case OpenXrHandednessType::kRight:
      return "right";
    case OpenXrHandednessType::kCount:
      NOTREACHED();
      return "";
  }
}

std::string GetTopLevelUserPath(OpenXrHandednessType type) {
  return std::string("/user/hand/") + GetStringFromType(type);
}

}  // namespace

OpenXrController::OpenXrController()
    : description_(nullptr),
      type_(OpenXrHandednessType::kCount),  // COUNT refers to invalid.
      instance_(XR_NULL_HANDLE),
      session_(XR_NULL_HANDLE),
      action_set_(XR_NULL_HANDLE),
      grip_pose_action_{XR_NULL_HANDLE},
      grip_pose_space_(XR_NULL_HANDLE),
      pointer_pose_action_(XR_NULL_HANDLE),
      pointer_pose_space_(XR_NULL_HANDLE),
      interaction_profile_(OpenXrInteractionProfileType::kCount) {}

OpenXrController::~OpenXrController() {
  // We don't need to destroy all of the actions because destroying an
  // action set destroys all actions that are part of the set.

  if (action_set_ != XR_NULL_HANDLE) {
    xrDestroyActionSet(action_set_);
  }
  if (grip_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(grip_pose_space_);
  }
  if (pointer_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(pointer_pose_space_);
  }
}
XrResult OpenXrController::Initialize(
    OpenXrHandednessType type,
    XrInstance instance,
    XrSession session,
    const OpenXRPathHelper* path_helper,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  DCHECK(bindings);
  type_ = type;
  instance_ = instance;
  session_ = session;
  path_helper_ = path_helper;

  std::string action_set_name =
      std::string(GetStringFromType(type_)) + "_action_set";

  XrActionSetCreateInfo action_set_create_info = {
      XR_TYPE_ACTION_SET_CREATE_INFO};

  errno_t error = strcpy_s(action_set_create_info.actionSetName,
                           base::size(action_set_create_info.actionSetName),
                           action_set_name.c_str());
  DCHECK(!error);
  error = strcpy_s(action_set_create_info.localizedActionSetName,
                   base::size(action_set_create_info.localizedActionSetName),
                   action_set_name.c_str());
  DCHECK(!error);

  RETURN_IF_XR_FAILED(
      xrCreateActionSet(instance_, &action_set_create_info, &action_set_));

  RETURN_IF_XR_FAILED(InitializeControllerActions());

  SuggestBindings(bindings);
  RETURN_IF_XR_FAILED(InitializeControllerSpaces());

  return XR_SUCCESS;
}

XrResult OpenXrController::InitializeControllerActions() {
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kTrigger));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kSqueeze));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kTrackpad));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kThumbstick));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kThumbrest));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kButton1));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kButton2));

  const std::string type_string = GetStringFromType(type_);
  const std::string name_prefix = type_string + "_controller_";
  // Axis Actions
  RETURN_IF_XR_FAILED(
      CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, name_prefix + "trackpad_axis",
                   &(axis_action_map_[OpenXrAxisType::kTrackpad])));
  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_VECTOR2F_INPUT, name_prefix + "thumbstick_axis",
      &(axis_action_map_[OpenXrAxisType::kThumbstick])));

  // Generic Pose Actions
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_POSE_INPUT,
                                   name_prefix + "grip_pose",
                                   &grip_pose_action_));
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_POSE_INPUT,
                                   name_prefix + "aim_pose",
                                   &pointer_pose_action_));

  return XR_SUCCESS;
}

XrResult OpenXrController::SuggestBindings(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) const {
  const std::string binding_prefix = GetTopLevelUserPath(type_);

  for (auto interaction_profile : kOpenXrControllerInteractionProfiles) {
    XrPath interaction_profile_path =
        path_helper_->GetInteractionProfileXrPath(interaction_profile.type);
    RETURN_IF_XR_FAILED(SuggestActionBinding(bindings, interaction_profile_path,
                                             grip_pose_action_,
                                             binding_prefix + "/input/grip"));
    RETURN_IF_XR_FAILED(SuggestActionBinding(bindings, interaction_profile_path,
                                             pointer_pose_action_,
                                             binding_prefix + "/input/aim"));

    const OpenXrButtonPathMap* button_maps;
    size_t button_map_size;
    switch (type_) {
      case OpenXrHandednessType::kLeft:
        button_maps = interaction_profile.left_button_maps;
        button_map_size = interaction_profile.left_button_map_size;
        break;
      case OpenXrHandednessType::kRight:
        button_maps = interaction_profile.right_button_maps;
        button_map_size = interaction_profile.right_button_map_size;
        break;
      case OpenXrHandednessType::kCount:
        NOTREACHED() << "Controller can only be left or right";
        return XR_ERROR_VALIDATION_FAILURE;
    }

    for (size_t button_map_index = 0; button_map_index < button_map_size;
         button_map_index++) {
      const OpenXrButtonPathMap& cur_button_map = button_maps[button_map_index];
      OpenXrButtonType button_type = cur_button_map.type;
      for (size_t action_map_index = 0;
           action_map_index < cur_button_map.action_map_size;
           action_map_index++) {
        const OpenXrButtonActionPathMap& cur_action_map =
            cur_button_map.action_maps[action_map_index];
        RETURN_IF_XR_FAILED(SuggestActionBinding(
            bindings, interaction_profile_path,
            button_action_map_.at(button_type).at(cur_action_map.type),
            binding_prefix + cur_action_map.path));
      }
    }

    for (size_t axis_map_index = 0;
         axis_map_index < interaction_profile.axis_map_size; axis_map_index++) {
      const OpenXrAxisPathMap& cur_axis_map =
          interaction_profile.axis_maps[axis_map_index];
      RETURN_IF_XR_FAILED(
          SuggestActionBinding(bindings, interaction_profile_path,
                               axis_action_map_.at(cur_axis_map.type),
                               binding_prefix + cur_axis_map.path));
    }
  }

  return XR_SUCCESS;
}

XrResult OpenXrController::InitializeControllerSpaces() {
  RETURN_IF_XR_FAILED(CreateActionSpace(grip_pose_action_, &grip_pose_space_));

  RETURN_IF_XR_FAILED(
      CreateActionSpace(pointer_pose_action_, &pointer_pose_space_));

  return XR_SUCCESS;
}

uint32_t OpenXrController::GetId() const {
  return static_cast<uint32_t>(type_);
}

device::mojom::XRHandedness OpenXrController::GetHandness() const {
  switch (type_) {
    case OpenXrHandednessType::kLeft:
      return device::mojom::XRHandedness::LEFT;
    case OpenXrHandednessType::kRight:
      return device::mojom::XRHandedness::RIGHT;
    case OpenXrHandednessType::kCount:
      // LEFT controller and RIGHT controller are currently the only supported
      // controllers. In the future, other controllers such as sound (which
      // does not have a handedness) will be added here.
      NOTREACHED();
      return device::mojom::XRHandedness::NONE;
  }
}

mojom::XRInputSourceDescriptionPtr OpenXrController::GetDescription(
    XrTime predicted_display_time) {
  // Description only need to be set once unless interaction profiles changes.
  if (!description_) {
    // UpdateInteractionProfile() can not be called inside Initialize() function
    // because XrGetCurrentInteractionProfile can't be called before
    // xrSuggestInteractionProfileBindings getting called.
    if (XR_FAILED(UpdateInteractionProfile())) {
      return nullptr;
    }
    description_ = device::mojom::XRInputSourceDescription::New();
    description_->handedness = GetHandness();
    description_->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;
    description_->profiles =
        path_helper_->GetInputProfiles(interaction_profile_);
  }

  if (!description_->input_from_pointer) {
    description_->input_from_pointer =
        GetPointerFromGripTransform(predicted_display_time);
  }

  return description_.Clone();
}

base::Optional<GamepadButton> OpenXrController::GetButton(
    OpenXrButtonType type) const {
  GamepadButton ret;
  // Button should at least have one of the three actions;
  bool has_value = false;

  DCHECK(button_action_map_.count(type) == 1);
  auto button = button_action_map_.at(type);
  XrActionStateBoolean press_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_SUCCEEDED(QueryState(button[OpenXrButtonActionType::kPress],
                              &press_state_bool)) &&
      press_state_bool.isActive) {
    ret.pressed = press_state_bool.currentState;
    has_value = true;
  } else {
    ret.pressed = false;
  }

  XrActionStateBoolean touch_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_SUCCEEDED(QueryState(button[OpenXrButtonActionType::kTouch],
                              &touch_state_bool)) &&
      touch_state_bool.isActive) {
    ret.touched = touch_state_bool.currentState;
    has_value = true;
  } else {
    ret.touched = ret.pressed;
  }

  XrActionStateFloat value_state_float = {XR_TYPE_ACTION_STATE_FLOAT};
  if (XR_SUCCEEDED(QueryState(button[OpenXrButtonActionType::kValue],
                              &value_state_float)) &&
      value_state_float.isActive) {
    ret.value = value_state_float.currentState;
    has_value = true;
  } else {
    ret.value = ret.pressed ? 1.0 : 0.0;
  }

  if (!has_value) {
    return base::nullopt;
  }

  return ret;
}

std::vector<double> OpenXrController::GetAxis(OpenXrAxisType type) const {
  XrActionStateVector2f axis_state_v2f = {XR_TYPE_ACTION_STATE_VECTOR2F};
  if (XR_FAILED(QueryState(axis_action_map_.at(type), &axis_state_v2f)) ||
      !axis_state_v2f.isActive) {
    return {};
  }

  return {axis_state_v2f.currentState.x, axis_state_v2f.currentState.y};
}

XrResult OpenXrController::UpdateInteractionProfile() {
  XrPath top_level_user_path;

  std::string top_level_user_path_string = GetTopLevelUserPath(type_);
  RETURN_IF_XR_FAILED(xrStringToPath(
      instance_, top_level_user_path_string.c_str(), &top_level_user_path));

  XrInteractionProfileState interaction_profile_state = {
      XR_TYPE_INTERACTION_PROFILE_STATE};
  RETURN_IF_XR_FAILED(xrGetCurrentInteractionProfile(
      session_, top_level_user_path, &interaction_profile_state));
  interaction_profile_ = path_helper_->GetInputProfileType(
      interaction_profile_state.interactionProfile);

  if (description_) {
    // TODO(crbug.com/1006072):
    // Query USB vendor and product ID From OpenXR.
    description_->profiles =
        path_helper_->GetInputProfiles(interaction_profile_);
  }
  return XR_SUCCESS;
}

base::Optional<gfx::Transform> OpenXrController::GetMojoFromGripTransform(
    XrTime predicted_display_time,
    XrSpace local_space,
    bool* emulated_position) const {
  return GetTransformFromSpaces(predicted_display_time, grip_pose_space_,
                                local_space, emulated_position);
}

base::Optional<gfx::Transform> OpenXrController::GetPointerFromGripTransform(
    XrTime predicted_display_time) const {
  bool emulated_position;
  return GetTransformFromSpaces(predicted_display_time, pointer_pose_space_,
                                grip_pose_space_, &emulated_position);
}

base::Optional<gfx::Transform> OpenXrController::GetTransformFromSpaces(
    XrTime predicted_display_time,
    XrSpace target,
    XrSpace origin,
    bool* emulated_position) const {
  XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION};
  // emulated_position indicates when there is a fallback from a fully-tracked
  // (i.e. 6DOF) type case to some form of orientation-only type tracking
  // (i.e. 3DOF/IMU type sensors)
  // Thus we have to make sure orientation is tracked.
  // Valid Bit only indicates it's either tracked or emulated, we have to check
  // for XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT to make sure orientation is
  // tracked.
  if (FAILED(
          xrLocateSpace(target, origin, predicted_display_time, &location)) ||
      !(location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) ||
      !(location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
    return base::nullopt;
  }

  // Convert the orientation and translation given by runtime into a
  // transformation matrix.
  gfx::DecomposedTransform decomp;
  decomp.quaternion =
      gfx::Quaternion(location.pose.orientation.x, location.pose.orientation.y,
                      location.pose.orientation.z, location.pose.orientation.w);
  decomp.translate[0] = location.pose.position.x;
  decomp.translate[1] = location.pose.position.y;
  decomp.translate[2] = location.pose.position.z;

  *emulated_position = true;
  if (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {
    *emulated_position = false;
  }

  return gfx::ComposeTransform(decomp);
}

XrResult OpenXrController::CreateActionsForButton(
    OpenXrButtonType button_type) {
  const std::string type_string = GetStringFromType(type_);
  std::string name_prefix = type_string + "_controller_";

  switch (button_type) {
    case OpenXrButtonType::kTrigger:
      name_prefix += "trigger_";
      break;
    case OpenXrButtonType::kSqueeze:
      name_prefix += "squeeze_";
      break;
    case OpenXrButtonType::kTrackpad:
      name_prefix += "trackpad_";
      break;
    case OpenXrButtonType::kThumbstick:
      name_prefix += "thumbstick_";
      break;
    case OpenXrButtonType::kThumbrest:
      name_prefix += "thumbrest_";
      break;
    case OpenXrButtonType::kButton1:
      name_prefix += "upper_button_";
      break;
    case OpenXrButtonType::kButton2:
      name_prefix += "lower_button_";
      break;
  }

  std::unordered_map<OpenXrButtonActionType, XrAction>& cur_button =
      button_action_map_[button_type];
  XrAction new_action;
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT,
                                   name_prefix + "button_press", &new_action));
  cur_button[OpenXrButtonActionType::kPress] = new_action;
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_FLOAT_INPUT,
                                   name_prefix + "button_value", &new_action));
  cur_button[OpenXrButtonActionType::kValue] = new_action;
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT,
                                   name_prefix + "button_touch", &new_action));
  cur_button[OpenXrButtonActionType::kTouch] = new_action;
  return XR_SUCCESS;
}

XrResult OpenXrController::CreateAction(XrActionType type,
                                        const std::string& action_name,
                                        XrAction* action) {
  DCHECK(action);
  XrActionCreateInfo action_create_info = {XR_TYPE_ACTION_CREATE_INFO};
  action_create_info.actionType = type;

  errno_t error =
      strcpy_s(action_create_info.actionName,
               base::size(action_create_info.actionName), action_name.data());
  DCHECK(error == 0);
  error = strcpy_s(action_create_info.localizedActionName,
                   base::size(action_create_info.localizedActionName),
                   action_name.data());
  DCHECK(error == 0);
  return xrCreateAction(action_set_, &action_create_info, action);
}

XrResult OpenXrController::SuggestActionBinding(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings,
    XrPath interaction_profile_path,
    XrAction action,
    std::string binding_string) const {
  XrPath binding_path;
  // make sure all actions we try to suggest binding are initialized.
  DCHECK(action != XR_NULL_HANDLE);
  RETURN_IF_XR_FAILED(
      xrStringToPath(instance_, binding_string.c_str(), &binding_path));
  (*bindings)[interaction_profile_path].push_back({action, binding_path});

  return XR_SUCCESS;
}

XrResult OpenXrController::CreateActionSpace(XrAction action, XrSpace* space) {
  DCHECK(space);
  XrActionSpaceCreateInfo action_space_create_info = {};
  action_space_create_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  action_space_create_info.action = action;
  action_space_create_info.subactionPath = XR_NULL_PATH;
  action_space_create_info.poseInActionSpace = PoseIdentity();
  return xrCreateActionSpace(session_, &action_space_create_info, space);
}

}  // namespace device

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/isolated_gamepad_data_fetcher.h"
#include "device/vr/vr_device.h"

namespace device {

IsolatedGamepadDataFetcher::Factory::Factory(
    device::mojom::XRDeviceId display_id,
    device::mojom::IsolatedXRGamepadProviderFactoryPtr factory)
    : display_id_(display_id), factory_(std::move(factory)) {}

IsolatedGamepadDataFetcher::Factory::~Factory() {}

std::unique_ptr<GamepadDataFetcher>
IsolatedGamepadDataFetcher::Factory::CreateDataFetcher() {
  device::mojom::IsolatedXRGamepadProviderPtr provider;
  factory_->GetIsolatedXRGamepadProvider(mojo::MakeRequest(&provider));
  return std::make_unique<IsolatedGamepadDataFetcher>(display_id_,
                                                      std::move(provider));
}

GamepadSource IsolatedGamepadDataFetcher::Factory::source() {
  return (display_id_ == device::mojom::XRDeviceId::OPENVR_DEVICE_ID)
             ? GAMEPAD_SOURCE_OPENVR
             : GAMEPAD_SOURCE_OCULUS;
}

IsolatedGamepadDataFetcher::IsolatedGamepadDataFetcher(
    device::mojom::XRDeviceId display_id,
    device::mojom::IsolatedXRGamepadProviderPtr provider)
    : display_id_(display_id) {
  // We bind provider_ on the poling thread, but we're created on the main UI
  // thread.
  provider_info_ = provider.PassInterface();
}

IsolatedGamepadDataFetcher::~IsolatedGamepadDataFetcher() = default;

GamepadSource IsolatedGamepadDataFetcher::source() {
  return (display_id_ == device::mojom::XRDeviceId::OPENVR_DEVICE_ID)
             ? GAMEPAD_SOURCE_OPENVR
             : GAMEPAD_SOURCE_OCULUS;
}

void IsolatedGamepadDataFetcher::OnDataUpdated(
    device::mojom::XRGamepadDataPtr data) {
  data_ = std::move(data);
  have_outstanding_request_ = false;
}

GamepadPose GamepadPoseFromXRPose(device::mojom::VRPose* pose) {
  GamepadPose ret = {};
  ret.not_null = pose;
  if (!pose) {
    return ret;
  }

  if (pose->position) {
    ret.position.not_null = true;
    ret.position.x = (*pose->position)[0];
    ret.position.y = (*pose->position)[1];
    ret.position.z = (*pose->position)[2];
  }

  if (pose->orientation) {
    ret.orientation.not_null = true;
    ret.orientation.x = (*pose->orientation)[0];
    ret.orientation.y = (*pose->orientation)[1];
    ret.orientation.z = (*pose->orientation)[2];
    ret.orientation.w = (*pose->orientation)[3];
  }

  if (pose->angularVelocity) {
    ret.angular_velocity.not_null = true;
    ret.angular_velocity.x = (*pose->angularVelocity)[0];
    ret.angular_velocity.y = (*pose->angularVelocity)[1];
    ret.angular_velocity.z = (*pose->angularVelocity)[2];
  }

  if (pose->linearVelocity) {
    ret.linear_velocity.not_null = true;
    ret.linear_velocity.x = (*pose->linearVelocity)[0];
    ret.linear_velocity.y = (*pose->linearVelocity)[1];
    ret.linear_velocity.z = (*pose->linearVelocity)[2];
  }

  if (pose->angularAcceleration) {
    ret.angular_acceleration.not_null = true;
    ret.angular_acceleration.x = (*pose->angularAcceleration)[0];
    ret.angular_acceleration.y = (*pose->angularAcceleration)[1];
    ret.angular_acceleration.z = (*pose->angularAcceleration)[2];
  }

  if (pose->linearAcceleration) {
    ret.linear_acceleration.not_null = true;
    ret.linear_acceleration.x = (*pose->linearAcceleration)[0];
    ret.linear_acceleration.y = (*pose->linearAcceleration)[1];
    ret.linear_acceleration.z = (*pose->linearAcceleration)[2];
  }

  return ret;
}

void IsolatedGamepadDataFetcher::GetGamepadData(bool devices_changed_hint) {
  if (!provider_ && provider_info_) {
    provider_.Bind(std::move(provider_info_));
  }

  // If we don't have a provider, we can't give out data.
  if (!provider_) {
    return;
  }

  // Request new data.
  if (!have_outstanding_request_) {
    have_outstanding_request_ = true;
    provider_->RequestUpdate(base::BindOnce(
        &IsolatedGamepadDataFetcher::OnDataUpdated, base::Unretained(this)));
  }

  // If we have no data to give out, nothing to do.
  if (!data_) {
    return;
  }

  // Keep track of gamepads that go missing.
  std::set<unsigned int> seen_gamepads;

  // Give out data_, but we need to translate it.
  for (size_t i = 0; i < data_->gamepads.size(); ++i) {
    auto& source = data_->gamepads[i];
    PadState* state = GetPadState(source->controller_id);
    if (!state)
      continue;
    Gamepad& dest = state->data;
    dest.connected = true;

    seen_gamepads.insert(source->controller_id);
    dest.timestamp = CurrentTimeInMicroseconds();
    dest.pose = GamepadPoseFromXRPose(source->pose.get());
    dest.pose.has_position = source->can_provide_position;
    dest.pose.has_orientation = source->can_provide_orientation;
    dest.hand = source->hand == device::mojom::XRHandedness::LEFT
                    ? GamepadHand::kLeft
                    : source->hand == device::mojom::XRHandedness::RIGHT
                          ? GamepadHand::kRight
                          : GamepadHand::kNone;
    dest.display_id = static_cast<unsigned int>(display_id_);
    dest.is_xr = true;

    dest.axes_length = source->axes.size();
    for (size_t j = 0; j < source->axes.size(); ++j) {
      dest.axes[j] = source->axes[j];
    }

    dest.buttons_length = source->buttons.size();
    for (size_t j = 0; j < source->buttons.size(); ++j) {
      dest.buttons[j] =
          GamepadButton(source->buttons[j]->pressed,
                        source->buttons[j]->touched, source->buttons[j]->value);
    }

    // Gamepad extensions refer to display_id, corresponding to the WebVR
    // VRDisplay's dipslayId property.
    // As WebVR is deprecated, and we only hand out a maximum of one "display"
    // per runtime, we use the XRDeviceId now to associate the controller with
    // a headset.  This doesn't change behavior, but the device/display naming
    // could be confusing here.
    if (display_id_ == device::mojom::XRDeviceId::OPENVR_DEVICE_ID) {
      swprintf(dest.id, Gamepad::kIdLengthCap, L"OpenVR Gamepad");
    } else {
      if (dest.hand == GamepadHand::kLeft) {
        swprintf(dest.id, Gamepad::kIdLengthCap, L"Oculus Touch (Left)");
      } else if (dest.hand == GamepadHand::kRight) {
        swprintf(dest.id, Gamepad::kIdLengthCap, L"Oculus Touch (Right)");
      } else {
        swprintf(dest.id, Gamepad::kIdLengthCap, L"Oculus Remote");
      }
    }

    dest.mapping[0] = 0;
  }

  // Remove any gamepads that aren't connected.
  for (auto id : active_gamepads_) {
    if (seen_gamepads.find(id) == seen_gamepads.end()) {
      PadState* state = GetPadState(id);
      if (state) {
        state->data.connected = false;
      }
    }
  }
  active_gamepads_ = std::move(seen_gamepads);
}

void IsolatedGamepadDataFetcher::PauseHint(bool paused) {}

void IsolatedGamepadDataFetcher::OnAddedToProvider() {}

void IsolatedGamepadDataFetcher::Factory::RemoveGamepad(
    device::mojom::XRDeviceId id) {
  using namespace device;
  // TODO(crbug.com/868101) - Remove this.
  std::map<device::mojom::XRDeviceId, GamepadSource>
      device_id_to_gamepad_source = {
          {device::mojom::XRDeviceId::OCULUS_DEVICE_ID, GAMEPAD_SOURCE_OCULUS},
          {device::mojom::XRDeviceId::OPENVR_DEVICE_ID, GAMEPAD_SOURCE_OPENVR},
      };

  GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      device_id_to_gamepad_source[id]);
}

void IsolatedGamepadDataFetcher::Factory::AddGamepad(
    device::mojom::XRDeviceId device_id,
    device::mojom::IsolatedXRGamepadProviderFactoryPtr gamepad_factory) {
  device::GamepadDataFetcherManager::GetInstance()->AddFactory(
      new device::IsolatedGamepadDataFetcher::Factory(
          device_id, std::move(gamepad_factory)));
}

}  // namespace device

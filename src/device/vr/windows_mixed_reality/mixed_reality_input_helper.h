// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "base/synchronization/lock.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/util/gamepad_builder.h"

namespace device {

enum class ButtonName {
  kSelect,
  kGrip,
  kTouchpad,
  kThumbstick,
};

struct ParsedInputState {
  mojom::XRInputSourceStatePtr source_state;
  std::unordered_map<ButtonName, GamepadBuilder::ButtonData> button_data;
  GamepadPose gamepad_pose;
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
  ParsedInputState();
  ~ParsedInputState();
  ParsedInputState(ParsedInputState&& other);
};

class WMRCoordinateSystem;
class WMRInputManager;
class WMRInputSourceState;
class WMRInputSourceEventArgs;
class WMRTimestamp;
class MixedRealityInputHelper {
 public:
  MixedRealityInputHelper(HWND hwnd);
  virtual ~MixedRealityInputHelper();
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      const WMRCoordinateSystem* origin,
      const WMRTimestamp* timestamp);

  mojom::XRGamepadDataPtr GetWebVRGamepadData(const WMRCoordinateSystem* origin,
                                              const WMRTimestamp* timestamp);

  void Dispose();

 private:
  bool EnsureSpatialInteractionManager();

  ParsedInputState LockedParseWindowsSourceState(
      const WMRInputSourceState* state,
      const WMRCoordinateSystem* origin);

  void OnSourcePressed(const WMRInputSourceEventArgs& args);
  void OnSourceReleased(const WMRInputSourceEventArgs& args);
  void ProcessSourceEvent(const WMRInputSourceEventArgs& args, bool is_pressed);

  void SubscribeEvents();
  void UnsubscribeEvents();

  std::unique_ptr<WMRInputManager> input_manager_;
  std::unique_ptr<
      base::CallbackList<void(const WMRInputSourceEventArgs&)>::Subscription>
      pressed_subscription_;
  std::unique_ptr<
      base::CallbackList<void(const WMRInputSourceEventArgs&)>::Subscription>
      released_subscription_;

  struct ControllerState {
    bool pressed = false;
    bool clicked = false;
    base::Optional<gfx::Transform> grip_from_pointer = base::nullopt;
    ControllerState();
    virtual ~ControllerState();
  };
  std::unordered_map<uint32_t, ControllerState> controller_states_;
  HWND hwnd_;

  std::vector<std::unique_ptr<WMRInputSourceState>> pending_voice_states_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MixedRealityInputHelper);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_

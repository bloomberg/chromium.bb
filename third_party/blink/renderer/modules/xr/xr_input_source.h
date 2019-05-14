// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace device {
class Gamepad;
}

namespace blink {

class XRGripSpace;
class XRSession;
class XRSpace;
class XRTargetRaySpace;

class XRInputSource : public ScriptWrappable, public Gamepad::Client {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XRInputSource);

 public:
  enum Handedness {
    kHandUninitialized = -1,
    kHandNone = 0,
    kHandLeft = 1,
    kHandRight = 2
  };

  enum TargetRayMode { kGaze = 1, kTrackedPointer = 2, kScreen = 3 };

  static XRInputSource* CreateOrUpdateFrom(
      XRInputSource* other /* may be null, input */,
      XRSession* session,
      const device::mojom::blink::XRInputSourceStatePtr& state);

  XRInputSource(XRSession*, uint32_t source_id);
  XRInputSource(XRSession*,
                const device::mojom::blink::XRInputSourceStatePtr& state);
  XRInputSource(const XRInputSource& other);
  ~XRInputSource() override = default;

  XRSession* session() const { return session_; }

  const String& handedness() const { return handedness_string_; }
  const String& targetRayMode() const { return target_ray_mode_string_; }
  bool emulatedPosition() const { return emulated_position_; }
  XRSpace* targetRaySpace() const;
  XRSpace* gripSpace() const;
  Gamepad* gamepad() const;

  uint32_t source_id() const { return source_id_; }

  void SetTargetRayMode(TargetRayMode);
  void SetPointerTransformMatrix(std::unique_ptr<TransformationMatrix>);

  // Gamepad::Client
  GamepadHapticActuator* GetVibrationActuatorForGamepad(
      const Gamepad&) override {
    // TODO(https://crbug.com/955097): XRInputSource implementation of
    // Gamepad::Client must manage vibration actuator state in a similar way to
    // NavigatorGamepad.
    return nullptr;
  }

  void Trace(blink::Visitor*) override;

  int16_t active_frame_id = -1;
  bool primary_input_pressed = false;
  bool selection_cancelled = false;

 private:
  friend class XRGripSpace;
  friend class XRTargetRaySpace;

  void SetHandedness(Handedness);
  void SetEmulatedPosition(bool emulated_position);
  void SetBasePoseMatrix(std::unique_ptr<TransformationMatrix>);

  // Use to check if the updates that would/should be made by a given
  // XRInputSourceState would invalidate any SameObject properties guaranteed
  // by the idl, and thus require the xr_input_source to be recreated.
  bool InvalidatesSameObject(
      const device::mojom::blink::XRInputSourceStatePtr& state);
  void UpdateGamepad(const device::Gamepad& gamepad);

  const Member<XRSession> session_;
  const uint32_t source_id_;
  Member<XRTargetRaySpace> target_ray_space_;
  Member<XRGripSpace> grip_space_;
  Member<Gamepad> gamepad_;

  Handedness handedness_ = kHandUninitialized;
  String handedness_string_;

  TargetRayMode target_ray_mode_;
  String target_ray_mode_string_;

  bool emulated_position_ = false;

  // TODO(crbug.com/945947): Revisit use of std::unique_ptr.
  std::unique_ptr<TransformationMatrix> base_pose_matrix_;

  // This is the transform to apply to the base_pose_matrix_ to get the pointer
  // matrix. In most cases it should be static.
  // TODO(crbug.com/945947): Revisit use of std::unique_ptr.
  std::unique_ptr<TransformationMatrix> pointer_transform_matrix_;

  // gamepad_ uses this to get relative timestamps.
  TimeTicks base_timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_grip_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

XRInputSource* XRInputSource::CreateOrUpdateFrom(
    XRInputSource* other,
    XRSession* session,
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if (!state)
    return other;

  XRInputSource* updated_source = other;
  if (other && other->InvalidatesSameObject(state)) {
    updated_source = MakeGarbageCollected<XRInputSource>(*other);

    // Need to explicitly override any of the properties that could cause us to
    // recreate the object.
    // TODO(https://crbug.com/962724): Simplify this creation pattern
    if (state->gamepad) {
      updated_source->gamepad_ = MakeGarbageCollected<Gamepad>(
          updated_source, 0, updated_source->base_timestamp_, TimeTicks::Now());
    } else {
      updated_source->gamepad_ = nullptr;
    }
  } else if (!other) {
    updated_source = MakeGarbageCollected<XRInputSource>(session, state);
  }

  if (state->gamepad) {
    updated_source->UpdateGamepad(*(state->gamepad));
  }

  // Update the input source's description if this state update includes them.
  if (state->description) {
    const device::mojom::blink::XRInputSourceDescriptionPtr& desc =
        state->description;

    // Setting target ray mode and handedness is fine here because earlier in
    // this function the input source was re-created if necessary.
    updated_source->SetTargetRayMode(desc->target_ray_mode);
    updated_source->SetHandedness(desc->handedness);
    updated_source->SetEmulatedPosition(desc->emulated_position);

    if (desc->pointer_offset && desc->pointer_offset->matrix.has_value()) {
      TransformationMatrix pointer_matrix =
          WTFFloatVectorToTransformationMatrix(
              desc->pointer_offset->matrix.value());
      updated_source->SetPointerTransformMatrix(&pointer_matrix);
    }
  }

  if (state->grip && state->grip->matrix.has_value()) {
    TransformationMatrix grip_matrix =
        WTFFloatVectorToTransformationMatrix(state->grip->matrix.value());
    updated_source->SetBasePoseMatrix(&grip_matrix);
  } else {
    updated_source->SetBasePoseMatrix(nullptr);
  }

  return updated_source;
}

XRInputSource::XRInputSource(XRSession* session,
                             uint32_t source_id,
                             device::mojom::XRTargetRayMode target_ray_mode)
    : session_(session),
      source_id_(source_id),
      target_ray_space_(MakeGarbageCollected<XRTargetRaySpace>(session, this)),
      grip_space_(MakeGarbageCollected<XRGripSpace>(session, this)),
      base_timestamp_(session->xr()->NavigationStart()) {
  SetTargetRayMode(target_ray_mode);
  SetHandedness(device::mojom::XRHandedness::NONE);
}

XRInputSource::XRInputSource(
    XRSession* session,
    const device::mojom::blink::XRInputSourceStatePtr& state)
    : XRInputSource(session,
                    state->source_id,
                    device::mojom::XRTargetRayMode::GAZING) {
  if (state->gamepad) {
    gamepad_ = MakeGarbageCollected<Gamepad>(this, 0, base_timestamp_,
                                             TimeTicks::Now());
  }
}

// Must make new target_ray_space_ and grip_space_ to ensure that they point to
// the correct XRInputSource object. Otherwise, the controller position gets
// stuck when an XRInputSource gets re-created. Also need to make a deep copy of
// the matrices since they use unique_ptrs.
XRInputSource::XRInputSource(const XRInputSource& other)
    : active_frame_id_(other.active_frame_id_),
      primary_input_pressed_(other.primary_input_pressed_),
      selection_cancelled_(other.selection_cancelled_),
      session_(other.session_),
      source_id_(other.source_id_),
      target_ray_space_(
          MakeGarbageCollected<XRTargetRaySpace>(other.session_, this)),
      grip_space_(MakeGarbageCollected<XRGripSpace>(other.session_, this)),
      gamepad_(other.gamepad_),
      emulated_position_(other.emulated_position_),
      base_timestamp_(other.base_timestamp_) {
  // Since these setters also set strings, for convenience, setting them via
  // their existing setters.
  SetTargetRayMode(other.target_ray_mode_);
  SetHandedness(other.handedness_);

  SetBasePoseMatrix(other.base_pose_matrix_.get());
  SetPointerTransformMatrix(other.pointer_transform_matrix_.get());
}

XRSpace* XRInputSource::gripSpace() const {
  if (target_ray_mode_ == device::mojom::XRTargetRayMode::POINTING) {
    return grip_space_;
  }

  return nullptr;
}

XRSpace* XRInputSource::targetRaySpace() const {
  return target_ray_space_;
}

Gamepad* XRInputSource::gamepad() const {
  return gamepad_;
}

bool XRInputSource::InvalidatesSameObject(
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if ((state->gamepad && !gamepad_) || (!state->gamepad && gamepad_)) {
    return true;
  }

  if (state->description) {
    if (state->description->handedness != handedness_) {
      return true;
    }

    if (state->description->target_ray_mode != target_ray_mode_) {
      return true;
    }
  }

  return false;
}

void XRInputSource::UpdateGamepad(const device::Gamepad& gamepad) {
  DCHECK(gamepad_);
  gamepad_->UpdateFromDeviceState(gamepad);
}

void XRInputSource::SetTargetRayMode(
    device::mojom::XRTargetRayMode target_ray_mode) {
  target_ray_mode_ = target_ray_mode;

  switch (target_ray_mode_) {
    case device::mojom::XRTargetRayMode::GAZING:
      target_ray_mode_string_ = "gaze";
      return;
    case device::mojom::XRTargetRayMode::POINTING:
      target_ray_mode_string_ = "tracked-pointer";
      return;
    case device::mojom::XRTargetRayMode::TAPPING:
      target_ray_mode_string_ = "screen";
      return;
  }

  NOTREACHED() << "Unknown target ray mode: " << target_ray_mode_;
}

void XRInputSource::SetHandedness(device::mojom::XRHandedness handedness) {
  handedness_ = handedness;

  switch (handedness_) {
    case device::mojom::XRHandedness::NONE:
      handedness_string_ = "none";
      return;
    case device::mojom::XRHandedness::LEFT:
      handedness_string_ = "left";
      return;
    case device::mojom::XRHandedness::RIGHT:
      handedness_string_ = "right";
      return;
  }

  NOTREACHED() << "Unknown handedness: " << handedness_;
}

void XRInputSource::SetEmulatedPosition(bool emulated_position) {
  emulated_position_ = emulated_position;
}

void XRInputSource::SetBasePoseMatrix(
    const TransformationMatrix* base_pose_matrix) {
  if (base_pose_matrix) {
    base_pose_matrix_ =
        std::make_unique<TransformationMatrix>(*base_pose_matrix);
  } else {
    base_pose_matrix_ = nullptr;
  }
}

void XRInputSource::SetPointerTransformMatrix(
    const TransformationMatrix* pointer_transform_matrix) {
  if (pointer_transform_matrix) {
    pointer_transform_matrix_ =
        std::make_unique<TransformationMatrix>(*pointer_transform_matrix);
  } else {
    pointer_transform_matrix_ = nullptr;
  }
}

void XRInputSource::SetGamepadConnected(bool state) {
  if (gamepad_)
    gamepad_->SetConnected(state);
}

void XRInputSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(target_ray_space_);
  visitor->Trace(grip_space_);
  visitor->Trace(gamepad_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

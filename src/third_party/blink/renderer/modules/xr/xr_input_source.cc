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

namespace blink {

namespace {

// TODO(https://crbug.com/962712): Switch to use typemapping instead.
XRInputSource::TargetRayMode MojomToBlinkTargetRayMode(
    device::mojom::XRTargetRayMode target_ray_mode) {
  switch (target_ray_mode) {
    case device::mojom::XRTargetRayMode::GAZING:
      return XRInputSource::TargetRayMode::kGaze;
    case device::mojom::XRTargetRayMode::POINTING:
      return XRInputSource::TargetRayMode::kTrackedPointer;
    case device::mojom::XRTargetRayMode::TAPPING:
      return XRInputSource::TargetRayMode::kScreen;
  }

  NOTREACHED();
}

// TODO(https://crbug.com/962712): Switch to use typemapping instead.
XRInputSource::Handedness MojomToBlinkHandedness(
    device::mojom::XRHandedness handedness) {
  switch (handedness) {
    case device::mojom::XRHandedness::NONE:
      return XRInputSource::Handedness::kHandNone;
    case device::mojom::XRHandedness::LEFT:
      return XRInputSource::Handedness::kHandLeft;
    case device::mojom::XRHandedness::RIGHT:
      return XRInputSource::Handedness::kHandRight;
  }

  NOTREACHED();
}
}  //  anonymous namespace

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
    updated_source->SetTargetRayMode(
        MojomToBlinkTargetRayMode(desc->target_ray_mode));
    updated_source->SetHandedness(MojomToBlinkHandedness(desc->handedness));
    updated_source->SetEmulatedPosition(desc->emulated_position);

    if (desc->pointer_offset && desc->pointer_offset->matrix.has_value()) {
      const WTF::Vector<float>& m = desc->pointer_offset->matrix.value();
      std::unique_ptr<TransformationMatrix> pointer_matrix =
          std::make_unique<TransformationMatrix>(
              m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10],
              m[11], m[12], m[13], m[14], m[15]);
      updated_source->SetPointerTransformMatrix(std::move(pointer_matrix));
    }
  }

  if (state->grip && state->grip->matrix.has_value()) {
    const Vector<float>& m = state->grip->matrix.value();
    std::unique_ptr<TransformationMatrix> grip_matrix =
        std::make_unique<TransformationMatrix>(
            m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10],
            m[11], m[12], m[13], m[14], m[15]);
    updated_source->SetBasePoseMatrix(std::move(grip_matrix));
  } else {
    updated_source->SetBasePoseMatrix(nullptr);
  }

  return updated_source;
}

XRInputSource::XRInputSource(XRSession* session,
                             uint32_t source_id,
                             TargetRayMode target_ray_mode)
    : session_(session),
      source_id_(source_id),
      target_ray_space_(MakeGarbageCollected<XRTargetRaySpace>(session, this)),
      grip_space_(MakeGarbageCollected<XRGripSpace>(session, this)),
      base_timestamp_(session->xr()->NavigationStart()) {
  SetTargetRayMode(target_ray_mode);
  SetHandedness(kHandNone);
}

XRInputSource::XRInputSource(
    XRSession* session,
    const device::mojom::blink::XRInputSourceStatePtr& state)
    : XRInputSource(session, state->source_id, kGaze) {
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

  if (other.base_pose_matrix_) {
    base_pose_matrix_ = std::make_unique<TransformationMatrix>(
        *(other.base_pose_matrix_.get()));
  }

  if (other.pointer_transform_matrix_) {
    pointer_transform_matrix_ = std::make_unique<TransformationMatrix>(
        *(other.pointer_transform_matrix_.get()));
  }
}

XRSpace* XRInputSource::gripSpace() const {
  if (target_ray_mode_ == kTrackedPointer) {
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
    Handedness other_handedness =
        MojomToBlinkHandedness(state->description->handedness);
    if (other_handedness != handedness_) {
      return true;
    }

    TargetRayMode other_mode =
        MojomToBlinkTargetRayMode(state->description->target_ray_mode);
    if (other_mode != target_ray_mode_) {
      return true;
    }
  }

  return false;
}

void XRInputSource::UpdateGamepad(const device::Gamepad& gamepad) {
  DCHECK(gamepad_);
  gamepad_->UpdateFromDeviceState(gamepad);
}

void XRInputSource::SetTargetRayMode(TargetRayMode target_ray_mode) {
  if (target_ray_mode_ == target_ray_mode)
    return;

  target_ray_mode_ = target_ray_mode;

  switch (target_ray_mode_) {
    case kGaze:
      target_ray_mode_string_ = "gaze";
      return;
    case kTrackedPointer:
      target_ray_mode_string_ = "tracked-pointer";
      return;
    case kScreen:
      target_ray_mode_string_ = "screen";
      return;
  }

  NOTREACHED() << "Unknown target ray mode: " << target_ray_mode_;
}

void XRInputSource::SetHandedness(Handedness handedness) {
  if (handedness_ == handedness)
    return;

  handedness_ = handedness;

  switch (handedness_) {
    case kHandNone:
      handedness_string_ = "none";
      return;
    case kHandLeft:
      handedness_string_ = "left";
      return;
    case kHandRight:
      handedness_string_ = "right";
      return;
    case kHandUninitialized:
      NOTREACHED() << "Cannot set handedness to uninitialized";
      return;
  }

  NOTREACHED() << "Unknown handedness: " << handedness_;
}

void XRInputSource::SetEmulatedPosition(bool emulated_position) {
  emulated_position_ = emulated_position;
}

void XRInputSource::SetBasePoseMatrix(
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  base_pose_matrix_ = std::move(base_pose_matrix);
}

void XRInputSource::SetPointerTransformMatrix(
    std::unique_ptr<TransformationMatrix> pointer_transform_matrix) {
  pointer_transform_matrix_ = std::move(pointer_transform_matrix);
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

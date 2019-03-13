// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRInputSource::XRInputSource(XRSession* session, uint32_t source_id)
    : session_(session),
      source_id_(source_id),
      target_ray_space_(MakeGarbageCollected<XRSpace>(session)),
      grip_space_(MakeGarbageCollected<XRSpace>(session)) {
  SetTargetRayMode(kGaze);
  SetHandedness(kHandNone);
  target_ray_space_->SetInputSource(this, true);
  grip_space_->SetInputSource(this, false);
}

XRSpace* XRInputSource::gripSpace() const {
  if (target_ray_mode_ == kTrackedPointer) {
    return grip_space_;
  }

  return nullptr;
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

void XRInputSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(target_ray_space_);
  visitor->Trace(grip_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"

namespace blink {

XRTargetRaySpace::XRTargetRaySpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

std::unique_ptr<TransformationMatrix> XRTargetRaySpace::GetPointerPoseForScreen(
    XRSpace* other_space,
    const TransformationMatrix& base_pose_matrix) {
  // If the pointer origin is the screen we need the head's base pose and
  // the pointer transform matrix to continue. The pointer transform will
  // represent the point the canvas was clicked as an offset from the view.
  if (!input_source_->pointer_transform_matrix_) {
    return nullptr;
  }

  // Multiply the head pose and pointer transform to get the final pointer.
  std::unique_ptr<TransformationMatrix> pointer_pose =
      other_space->TransformBasePose(base_pose_matrix);
  if (!pointer_pose) {
    return nullptr;
  }

  pointer_pose->Multiply(*(input_source_->pointer_transform_matrix_));
  return pointer_pose;
}

std::unique_ptr<TransformationMatrix> XRTargetRaySpace::GetTrackedPointerPose(
    XRSpace* other_space,
    const TransformationMatrix& base_pose_matrix) {
  if (!input_source_->base_pose_matrix_) {
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> grip_pose =
      other_space->TransformBaseInputPose(*(input_source_->base_pose_matrix_),
                                          base_pose_matrix);

  if (!grip_pose) {
    return nullptr;
  }

  if (input_source_->pointer_transform_matrix_) {
    grip_pose->Multiply(*(input_source_->pointer_transform_matrix_));
  }

  return grip_pose;
}

XRPose* XRTargetRaySpace::getPose(
    XRSpace* other_space,
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  // If we don't have a valid base pose (most common when tracking is lost),
  // we can't get a target ray pose regardless of the mode.
  if (!base_pose_matrix) {
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> pointer_pose = nullptr;
  switch (input_source_->target_ray_mode_) {
    case XRInputSource::kScreen: {
      pointer_pose = GetPointerPoseForScreen(other_space, *base_pose_matrix);
      break;
    }
    case XRInputSource::kGaze: {
      // If the pointer origin is the users head, this is a gaze cursor and the
      // returned pointer is based on the device pose. Just return the head pose
      // as the pointer pose.
      pointer_pose = other_space->TransformBasePose(*base_pose_matrix);
      break;
    }
    case XRInputSource::kTrackedPointer: {
      pointer_pose = GetTrackedPointerPose(other_space, *base_pose_matrix);
      break;
    }
    default: {
      return nullptr;
    }
  }

  if (!pointer_pose) {
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so that
  // things like teleportation works.
  pointer_pose = std::make_unique<TransformationMatrix>(
      other_space->InverseOriginOffsetMatrix().Multiply(*pointer_pose));

  return MakeGarbageCollected<XRPose>(std::move(pointer_pose),
                                      input_source_->emulatedPosition());
}

void XRTargetRaySpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink

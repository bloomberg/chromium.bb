// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_grip_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"

namespace blink {

XRGripSpace::XRGripSpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

XRPose* XRGripSpace::getPose(
    XRSpace* other_space,
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  // Grip is only available when using tracked pointer for input.
  if (input_source_->target_ray_mode_ != XRInputSource::kTrackedPointer) {
    return nullptr;
  }

  // Make sure the required pose matrices are available.
  if (!base_pose_matrix || !input_source_->base_pose_matrix_) {
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> grip_pose =
      other_space->TransformBaseInputPose(*(input_source_->base_pose_matrix_),
                                          *base_pose_matrix);

  if (!grip_pose) {
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so
  // that things like teleportation works.
  grip_pose = std::make_unique<TransformationMatrix>(
      other_space->InverseOriginOffsetMatrix().Multiply(*grip_pose));

  return MakeGarbageCollected<XRPose>(std::move(grip_pose),
                                      input_source_->emulatedPosition());
}

void XRGripSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink

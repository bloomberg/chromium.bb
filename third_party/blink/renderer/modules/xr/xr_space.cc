// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_space.h"

#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRSpace::XRSpace(XRSession* session) : session_(session) {}

XRSpace::~XRSpace() = default;

std::unique_ptr<TransformationMatrix> XRSpace::GetTransformToMojoSpace() {
  // The base XRSpace does not have any relevant information, so can't determine
  // a transform here.
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::DefaultPose() {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  return nullptr;
}

std::unique_ptr<TransformationMatrix> XRSpace::TransformBaseInputPose(
    const TransformationMatrix& base_input_pose,
    const TransformationMatrix& base_pose) {
  return nullptr;
}

TransformationMatrix XRSpace::OriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

TransformationMatrix XRSpace::InverseOriginOffsetMatrix() {
  TransformationMatrix identity;
  return identity;
}

XRPose* XRSpace::getPose(XRSpace* other_space,
                         const TransformationMatrix* base_pose_matrix) {
  std::unique_ptr<TransformationMatrix> mojo_from_this =
      GetTransformToMojoSpace();
  if (!mojo_from_this) {
    return nullptr;
  }

  // Rigid transforms should always be invertible.
  DCHECK(mojo_from_this->IsInvertible());
  TransformationMatrix this_from_mojo = mojo_from_this->Inverse();

  std::unique_ptr<TransformationMatrix> mojo_from_other =
      other_space->GetTransformToMojoSpace();
  if (!mojo_from_other) {
    return nullptr;
  }

  // TODO(crbug.com/969133): Update how EmulatedPosition is determined here once
  // spec issue https://github.com/immersive-web/webxr/issues/534 has been
  // resolved.
  TransformationMatrix this_from_other =
      this_from_mojo.Multiply(*mojo_from_other);
  return MakeGarbageCollected<XRPose>(this_from_other,
                                      session()->EmulatedPosition());
}

std::unique_ptr<TransformationMatrix> XRSpace::GetViewerPoseMatrix(
    const TransformationMatrix* base_pose_matrix) {
  std::unique_ptr<TransformationMatrix> pose;

  // If we don't have a valid base pose, request the reference space's default
  // pose. Most common when tracking is lost.
  if (base_pose_matrix) {
    pose = TransformBasePose(*base_pose_matrix);
  } else {
    pose = DefaultPose();
  }

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!pose || !pose->IsInvertible()) {
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so that
  // things like teleportation works.
  return std::make_unique<TransformationMatrix>(
      InverseOriginOffsetMatrix().Multiply(*pose));
}

ExecutionContext* XRSpace::GetExecutionContext() const {
  return session()->GetExecutionContext();
}

const AtomicString& XRSpace::InterfaceName() const {
  return event_target_names::kXRSpace;
}

void XRSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink

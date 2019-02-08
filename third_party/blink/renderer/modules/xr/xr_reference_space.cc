// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_stage_bounds.h"

namespace blink {

// origin offset starts as identity transform
XRReferenceSpace::XRReferenceSpace(XRSession* session)
    : XRSpace(session),
      origin_offset_(MakeGarbageCollected<XRRigidTransform>(nullptr, nullptr)) {
}

XRReferenceSpace::~XRReferenceSpace() = default;

// Returns a default pose if no base pose is available. Only applicable to
// identity reference spaces.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::DefaultPose() {
  // An identity reference space always returns an identity matrix.
  return TransformationMatrix::Create();
}

// Transforms a given pose from a "base" reference space used by the XR
// service to the space represenced by this reference space.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  // Always return the default pose.
  return DefaultPose();
}

// Serves the same purpose as TransformBasePose, but for input poses. Needs to
// know the head pose so that some cases can properly adjust the input's
// relative position, but typically will be identical to TransformBasePose.
std::unique_ptr<TransformationMatrix> XRReferenceSpace::TransformBaseInputPose(
    const TransformationMatrix& base_input_pose,
    const TransformationMatrix& base_pose) {
  return TransformBasePose(base_input_pose);
}

void XRReferenceSpace::setOriginOffset(XRRigidTransform* transform) {
  origin_offset_ = transform;
}

void XRReferenceSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(origin_offset_);
  XRSpace::Trace(visitor);
}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_stage_bounds.h"

namespace blink {

XRBoundedReferenceSpace::XRBoundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(session) {}

XRBoundedReferenceSpace::~XRBoundedReferenceSpace() = default;

void XRBoundedReferenceSpace::UpdateBoundsGeometry(
    XRStageBounds* bounds_geometry) {
  // TODO(https://crbug.com/917411): Support bounds geometry and fire a 'reset'
  // event when the bounds change.
}

// No default pose for bounded reference spaces.
std::unique_ptr<TransformationMatrix> XRBoundedReferenceSpace::DefaultPose() {
  return nullptr;
}

void XRBoundedReferenceSpace::UpdateFloorLevelTransform() {
  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stageParameters) {
    // Use the transform given by xrDisplayInfo's stageParameters if available.
    const WTF::Vector<float>& m =
        display_info->stageParameters->standingTransform;
    floor_level_transform_ = TransformationMatrix::Create(
        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10],
        m[11], m[12], m[13], m[14], m[15]);
  } else {
    // If stage parameters aren't available set the transform to null, which
    // will subsequently cause this reference space to return null poses.
    floor_level_transform_.reset();
  }

  display_info_id_ = session()->DisplayInfoPtrId();
}

// Transforms a given pose from a "base" reference space used by the XR
// service to the bounded (floor level) reference space. Ideally in the future
// this reference space can be used without additional transforms, with
// the various XR backends returning poses already in the right space.
std::unique_ptr<TransformationMatrix>
XRBoundedReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  // Check first to see if the xrDisplayInfo has updated since the last
  // call. If so, update the pose transform.
  if (display_info_id_ != session()->DisplayInfoPtrId())
    UpdateFloorLevelTransform();

  // If the reference space has a transform apply it to the base pose and return
  // that, otherwise return null.
  if (floor_level_transform_) {
    std::unique_ptr<TransformationMatrix> pose(
        TransformationMatrix::Create(*floor_level_transform_));
    pose->Multiply(base_pose);
    return pose;
  }

  return nullptr;
}

void XRBoundedReferenceSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounds_geometry_);
  XRReferenceSpace::Trace(visitor);
}

}  // namespace blink

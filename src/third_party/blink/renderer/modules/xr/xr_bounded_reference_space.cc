// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRBoundedReferenceSpace::XRBoundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(session) {}

XRBoundedReferenceSpace::~XRBoundedReferenceSpace() = default;

void XRBoundedReferenceSpace::setOriginOffset(XRRigidTransform* transform) {
  XRReferenceSpace::setOriginOffset(transform);

  // Force a bounds update.
  stage_parameters_id_ = 0;
  EnsureUpdated();
}

// No default pose for bounded reference spaces.
std::unique_ptr<TransformationMatrix> XRBoundedReferenceSpace::DefaultPose() {
  return nullptr;
}

void XRBoundedReferenceSpace::EnsureUpdated() {
  // Check first to see if the stage parameters have updated since the last
  // call. We only need to update the transform and bounds if it has.
  if (stage_parameters_id_ == session()->StageParametersId())
    return;

  stage_parameters_id_ = session()->StageParametersId();

  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stageParameters) {
    // Use the transform given by xrDisplayInfo's stageParameters if available.
    const WTF::Vector<float>& m =
        display_info->stageParameters->standingTransform;
    floor_level_transform_ = std::make_unique<TransformationMatrix>(
        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10],
        m[11], m[12], m[13], m[14], m[15]);

    // In order to ensure that the bounds continue to line up with the user's
    // physical environment we need to transform by the inverse of the
    // originOffset.
    TransformationMatrix bounds_transform =
        originOffset()->InverseTransformMatrix();

    if (display_info->stageParameters->bounds) {
      bounds_geometry_.clear();

      for (const auto& bound : *(display_info->stageParameters->bounds)) {
        FloatPoint3D p =
            bounds_transform.MapPoint(FloatPoint3D(bound->x, 0.0, bound->z));
        bounds_geometry_.push_back(
            DOMPointReadOnly::Create(p.X(), p.Y(), p.Z(), 1.0));
      }
    } else {
      double hx = display_info->stageParameters->sizeX * 0.5;
      double hz = display_info->stageParameters->sizeZ * 0.5;
      FloatPoint3D a = bounds_transform.MapPoint(FloatPoint3D(hx, 0.0, -hz));
      FloatPoint3D b = bounds_transform.MapPoint(FloatPoint3D(hx, 0.0, hz));
      FloatPoint3D c = bounds_transform.MapPoint(FloatPoint3D(-hx, 0.0, hz));
      FloatPoint3D d = bounds_transform.MapPoint(FloatPoint3D(-hx, 0.0, -hz));

      bounds_geometry_.clear();
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(a.X(), a.Y(), a.Z(), 1.0));
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(b.X(), b.Y(), b.Z(), 1.0));
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(c.X(), c.Y(), c.Z(), 1.0));
      bounds_geometry_.push_back(
          DOMPointReadOnly::Create(d.X(), d.Y(), d.Z(), 1.0));
    }
  } else {
    // If stage parameters aren't available set the transform to null, which
    // will subsequently cause this reference space to return null poses.
    floor_level_transform_.reset();
    bounds_geometry_.clear();
  }

  DispatchEvent(*XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
}

// Transforms a given pose from a "base" reference space used by the XR
// service to the bounded (floor level) reference space. Ideally in the future
// this reference space can be used without additional transforms, with
// the various XR backends returning poses already in the right space.
std::unique_ptr<TransformationMatrix>
XRBoundedReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  EnsureUpdated();

  // If the reference space has a transform apply it to the base pose and return
  // that, otherwise return null.
  if (floor_level_transform_) {
    std::unique_ptr<TransformationMatrix> pose(
        std::make_unique<TransformationMatrix>(*floor_level_transform_));
    pose->Multiply(base_pose);
    return pose;
  }

  return nullptr;
}

HeapVector<Member<DOMPointReadOnly>> XRBoundedReferenceSpace::boundsGeometry() {
  EnsureUpdated();
  return bounds_geometry_;
}

void XRBoundedReferenceSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounds_geometry_);
  XRReferenceSpace::Trace(visitor);
}

}  // namespace blink

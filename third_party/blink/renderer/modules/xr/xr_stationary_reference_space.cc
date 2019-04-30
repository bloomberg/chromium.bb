// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_stationary_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

// Rough estimate of avg human eye height in meters.
const double kDefaultEmulationHeight = 1.6;

XRStationaryReferenceSpace::XRStationaryReferenceSpace(XRSession* session,
                                                       Subtype subtype)
    : XRReferenceSpace(session), subtype_(subtype) {}

XRStationaryReferenceSpace::~XRStationaryReferenceSpace() = default;

void XRStationaryReferenceSpace::UpdateFloorLevelTransform() {
  const device::mojom::blink::VRDisplayInfoPtr& display_info =
      session()->GetVRDisplayInfo();

  if (display_info && display_info->stageParameters) {
    // Use the transform given by xrDisplayInfo's stageParameters if available.
    const WTF::Vector<float>& m =
        display_info->stageParameters->standingTransform;
    floor_level_transform_ = std::make_unique<TransformationMatrix>(
        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10],
        m[11], m[12], m[13], m[14], m[15]);
  } else {
    // Otherwise, create a transform based on the default emulated height.
    floor_level_transform_ = std::make_unique<TransformationMatrix>();
    floor_level_transform_->Translate3d(0, kDefaultEmulationHeight, 0);
  }

  display_info_id_ = session()->DisplayInfoPtrId();
}

// No default pose for stationary reference spaces.
std::unique_ptr<TransformationMatrix>
XRStationaryReferenceSpace::DefaultPose() {
  return nullptr;
}

// Transforms a pose into the correct space.
std::unique_ptr<TransformationMatrix>
XRStationaryReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  switch (subtype_) {
    case kSubtypeEyeLevel:
      // Currently all base poses are 'eye-level' poses, so return directly.
      return std::make_unique<TransformationMatrix>(base_pose);
      break;
    case kSubtypeFloorLevel:
      // Currently all base poses are 'eye-level' space, so use of 'floor-level'
      // reference spaces requires adjustment. Ideally the service will
      // eventually provide poses in the requested space directly, avoiding the
      // need to do additional transformation here.

      // Check first to see if the xrDisplayInfo has updated since the last
      // call. If so, update the floor-level transform.
      if (display_info_id_ != session()->DisplayInfoPtrId())
        UpdateFloorLevelTransform();

      // Apply the floor-level transform to the base pose.
      if (floor_level_transform_) {
        std::unique_ptr<TransformationMatrix> pose(
            std::make_unique<TransformationMatrix>(*floor_level_transform_));
        pose->Multiply(base_pose);
        return pose;
      }
      break;
    case kSubtypePositionDisabled: {
      // 'position-disabled' poses must not contain any translation components,
      // and as a result the space the base pose is originally in doesn't matter
      // much. Strip out translation component and return.
      std::unique_ptr<TransformationMatrix> pose(
          std::make_unique<TransformationMatrix>(base_pose));
      pose->SetM41(0.0);
      pose->SetM42(0.0);
      pose->SetM43(0.0);
      return pose;
    } break;
  }

  return nullptr;
}

// Serves the same purpose as TransformBasePose, but for input poses. Needs to
// know the head pose so that cases like the head-model frame of reference can
// properly adjust the input's relative position.
std::unique_ptr<TransformationMatrix>
XRStationaryReferenceSpace::TransformBaseInputPose(
    const TransformationMatrix& base_input_pose,
    const TransformationMatrix& base_pose) {
  switch (subtype_) {
    case kSubtypePositionDisabled: {
      std::unique_ptr<TransformationMatrix> head_model_pose(
          TransformBasePose(base_pose));

      // Get the positional delta between the base pose and the head model pose.
      float dx = head_model_pose->M41() - base_pose.M41();
      float dy = head_model_pose->M42() - base_pose.M42();
      float dz = head_model_pose->M43() - base_pose.M43();

      // Translate the controller by the same delta so that it shows up in the
      // right relative position.
      std::unique_ptr<TransformationMatrix> pose(
          std::make_unique<TransformationMatrix>(base_input_pose));
      pose->SetM41(pose->M41() + dx);
      pose->SetM42(pose->M42() + dy);
      pose->SetM43(pose->M43() + dz);

      return pose;
    } break;
    case kSubtypeEyeLevel:
    case kSubtypeFloorLevel:
      return TransformBasePose(base_input_pose);
      break;
  }

  return nullptr;
}

void XRStationaryReferenceSpace::Trace(blink::Visitor* visitor) {
  XRReferenceSpace::Trace(visitor);
}

void XRStationaryReferenceSpace::OnReset() {
  DispatchEvent(*XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
}

}  // namespace blink

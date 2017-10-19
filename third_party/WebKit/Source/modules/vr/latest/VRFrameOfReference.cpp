// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRFrameOfReference.h"

#include "modules/vr/latest/VRStageBounds.h"

namespace blink {

// Rough estimate of avg human eye height in meters.
const double kDefaultEmulationHeight = 1.6;

VRFrameOfReference::VRFrameOfReference(VRSession* session, Type type)
    : VRCoordinateSystem(session), type_(type) {}

VRFrameOfReference::~VRFrameOfReference() {}

void VRFrameOfReference::UpdatePoseTransform(
    std::unique_ptr<TransformationMatrix> transform) {
  pose_transform_ = std::move(transform);
}

void VRFrameOfReference::UpdateStageBounds(VRStageBounds* bounds) {
  bounds_ = bounds;
  // TODO(bajones): Fire a boundschange event
}

// Enables emulated height when using a stage frame of reference, which should
// only be used if the sytem does not have a native concept of how far above the
// floor the VRDevice is at any given moment. This applies a static vertical
// offset to the coordinate system so that the user feels approximately like
// they are standing on a floor plane located at Y = 0. An explicit offset in
// meters can be given if the page has specific needs.
void VRFrameOfReference::UseEmulatedHeight(double value) {
  if (value == 0.0) {
    value = kDefaultEmulationHeight;
  }
  emulatedHeight_ = value;
  pose_transform_ = TransformationMatrix::Create();
  pose_transform_->Translate3d(0, emulatedHeight_, 0);
}

// Transforms a given pose from a "base" coordinate system used by the VR
// service to the frame of reference's coordinate system. This model is a bit
// over-simplified and will need to be made more robust when we start dealing
// with world-scale 6DoF tracking.
std::unique_ptr<TransformationMatrix> VRFrameOfReference::TransformBasePose(
    const TransformationMatrix& base_pose) {
  switch (type_) {
    case kTypeHeadModel: {
      // TODO(bajones): Detect if base pose is already neck modeled and return
      // it unchanged if so for better performance.

      // Strip out translation component.
      std::unique_ptr<TransformationMatrix> pose(
          TransformationMatrix::Create(base_pose));
      pose->SetM41(0.0);
      pose->SetM42(0.0);
      pose->SetM43(0.0);
      // TODO(bajones): Apply our own neck model
      return pose;
    } break;
    case kTypeEyeLevel:
      // For now we assume that all base poses are delivered as eye-level poses.
      // Thus in this case we just return the pose without transformation.
      return TransformationMatrix::Create(base_pose);
      break;
    case kTypeStage:
      // If the stage has a transform apply it to the base pose and return that,
      // otherwise return null.
      if (pose_transform_) {
        std::unique_ptr<TransformationMatrix> pose(
            TransformationMatrix::Create(*pose_transform_));
        pose->Multiply(base_pose);
        return pose;
      }
      break;
  }

  return nullptr;
}

void VRFrameOfReference::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounds_);
  VRCoordinateSystem::Trace(visitor);
}

}  // namespace blink

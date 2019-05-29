// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_

#include <memory>

#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRRigidTransform;

class XRReferenceSpace : public XRSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Used for metrics, don't remove or change values.
  enum class Type : int {
    kTypeViewer = 0,
    kTypeLocal = 1,
    kTypeLocalFloor = 2,
    kTypeBoundedFloor = 3,
    kTypeUnbounded = 4,
    kMaxValue = kTypeUnbounded,
  };

  static Type StringToReferenceSpaceType(const String& reference_space_type);

  XRReferenceSpace(XRSession*, Type);
  XRReferenceSpace(XRSession*, XRRigidTransform*, Type);
  ~XRReferenceSpace() override;

  XRPose* getPose(
      XRSpace* other_space,
      std::unique_ptr<TransformationMatrix> base_pose_matrix) override;
  std::unique_ptr<TransformationMatrix> DefaultPose() override;
  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;
  std::unique_ptr<TransformationMatrix> TransformBaseInputPose(
      const TransformationMatrix& base_input_pose,
      const TransformationMatrix& base_pose) override;

  std::unique_ptr<TransformationMatrix> GetTransformToMojoSpace() override;

  TransformationMatrix OriginOffsetMatrix() override;
  TransformationMatrix InverseOriginOffsetMatrix() override;

  XRReferenceSpace* getOffsetReferenceSpace(XRRigidTransform* transform);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(reset, kReset)

  void Trace(blink::Visitor*) override;

  virtual void OnReset();

 private:
  virtual XRReferenceSpace* cloneWithOriginOffset(
      XRRigidTransform* origin_offset);

  void UpdateFloorLevelTransform();

  unsigned int display_info_id_ = 0;

  std::unique_ptr<TransformationMatrix> floor_level_transform_;
  Member<XRRigidTransform> origin_offset_;
  Type type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_

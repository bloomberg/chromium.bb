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
  explicit XRReferenceSpace(XRSession*);
  ~XRReferenceSpace() override;

  std::unique_ptr<TransformationMatrix> DefaultPose() override;
  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;
  std::unique_ptr<TransformationMatrix> TransformBaseInputPose(
      const TransformationMatrix& base_input_pose,
      const TransformationMatrix& base_pose) override;

  std::unique_ptr<TransformationMatrix> GetTransformToMojoSpace() override;

  XRRigidTransform* originOffset() const { return origin_offset_; }
  void setOriginOffset(XRRigidTransform*);
  TransformationMatrix InverseOriginOffsetMatrix() override;

  void Trace(blink::Visitor*) override;

 private:
  Member<XRRigidTransform> origin_offset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_REFERENCE_SPACE_H_

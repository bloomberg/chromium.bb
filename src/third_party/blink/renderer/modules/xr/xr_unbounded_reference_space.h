// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_UNBOUNDED_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_UNBOUNDED_REFERENCE_SPACE_H_

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRUnboundedReferenceSpace final : public XRReferenceSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRUnboundedReferenceSpace(XRSession*);
  ~XRUnboundedReferenceSpace() override;

  std::unique_ptr<TransformationMatrix> DefaultPose() override;
  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;

  void Trace(blink::Visitor*) override;

 private:
  std::unique_ptr<TransformationMatrix> pose_transform_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_UNBOUNDED_REFERENCE_SPACE_H_

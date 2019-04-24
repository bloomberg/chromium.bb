// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_STATIONARY_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_STATIONARY_REFERENCE_SPACE_H_

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRStationaryReferenceSpace final : public XRReferenceSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Subtype {
    kSubtypeEyeLevel,
    kSubtypeFloorLevel,
    kSubtypePositionDisabled
  };

  XRStationaryReferenceSpace(XRSession*, Subtype);
  ~XRStationaryReferenceSpace() override;

  std::unique_ptr<TransformationMatrix> DefaultPose() override;
  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;
  std::unique_ptr<TransformationMatrix> TransformBaseInputPose(
      const TransformationMatrix& base_input_pose,
      const TransformationMatrix& base_pose) override;

  const String& subtype() const { return subtype_string_; }

  void Trace(blink::Visitor*) override;

 private:
  void UpdateFloorLevelTransform();

  unsigned int display_info_id_ = 0;
  Subtype subtype_;
  String subtype_string_;
  std::unique_ptr<TransformationMatrix> floor_level_transform_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_STATIONARY_REFERENCE_SPACE_H_

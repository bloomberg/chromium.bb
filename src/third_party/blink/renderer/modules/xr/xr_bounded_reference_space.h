// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_

#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRStageBounds;

class XRBoundedReferenceSpace final : public XRReferenceSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRBoundedReferenceSpace(XRSession*);
  ~XRBoundedReferenceSpace() override;

  void UpdateBoundsGeometry(XRStageBounds*);

  std::unique_ptr<TransformationMatrix> DefaultPose() override;
  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;

  HeapVector<Member<DOMPointReadOnly>> boundsGeometry() const {
    return bounds_geometry_;
  }

  void Trace(blink::Visitor*) override;

 private:
  void UpdateFloorLevelTransform();

  HeapVector<Member<DOMPointReadOnly>> bounds_geometry_;
  std::unique_ptr<TransformationMatrix> floor_level_transform_;
  unsigned int display_info_id_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_BOUNDED_REFERENCE_SPACE_H_

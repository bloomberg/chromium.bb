// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_SPACE_H_

#include <memory>

#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

class XRViewerSpace : public XRSpace {
 public:
  XRViewerSpace(XRSession*);
  XRPose* getPose(
      XRSpace* other_space,
      std::unique_ptr<TransformationMatrix> base_pose_matrix) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_SPACE_H_

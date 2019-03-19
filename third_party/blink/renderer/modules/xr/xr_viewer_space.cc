// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRViewerSpace::XRViewerSpace(XRSession* session) : XRSpace(session) {}

XRPose* XRViewerSpace::getPose(
    XRSpace* other_space,
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  std::unique_ptr<TransformationMatrix> viewer_pose_matrix =
      other_space->GetViewerPoseMatrix(std::move(base_pose_matrix));
  if (!viewer_pose_matrix) {
    return nullptr;
  }

  return MakeGarbageCollected<XRPose>(std::move(viewer_pose_matrix),
                                      session()->EmulatedPosition());
}

}  // namespace blink

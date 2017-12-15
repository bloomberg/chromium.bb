// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRPresentationFrame.h"

#include "modules/xr/XRCoordinateSystem.h"
#include "modules/xr/XRDevicePose.h"
#include "modules/xr/XRSession.h"
#include "modules/xr/XRView.h"

namespace blink {

XRPresentationFrame::XRPresentationFrame(XRSession* session)
    : session_(session) {}

const HeapVector<Member<XRView>>& XRPresentationFrame::views() const {
  return session_->views();
}

XRDevicePose* XRPresentationFrame::getDevicePose(
    XRCoordinateSystem* coordinate_system) const {
  // If we don't have a valid base pose return null. Most common when tracking
  // is lost.
  if (!base_pose_matrix_ || !coordinate_system) {
    return nullptr;
  }

  // Must use a coordinate system created from the same session.
  if (coordinate_system->session() != session_) {
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> pose =
      coordinate_system->TransformBasePose(*base_pose_matrix_);

  if (!pose) {
    return nullptr;
  }

  return new XRDevicePose(session(), std::move(pose));
}

void XRPresentationFrame::UpdateBasePose(
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  base_pose_matrix_ = std::move(base_pose_matrix);
}

void XRPresentationFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

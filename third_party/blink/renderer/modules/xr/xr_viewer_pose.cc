// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

XRViewerPose::XRViewerPose(
    XRSession* session,
    std::unique_ptr<TransformationMatrix> pose_model_matrix)
    : XRPose(std::move(pose_model_matrix), session->EmulatedPosition()) {
  // TODO(https://crbug.com/958014): Ensure that this copy doesn't still
  // point to the underlying views objects from session.
  // session will update views if required
  // views array gets copied to views_
  views_ = session->views();

  for (Member<XRView>& view : views_) {
    view->UpdatePoseMatrix(transform_->TransformMatrix());
  }
}

void XRViewerPose::Trace(blink::Visitor* visitor) {
  visitor->Trace(views_);
  XRPose::Trace(visitor);
}

}  // namespace blink

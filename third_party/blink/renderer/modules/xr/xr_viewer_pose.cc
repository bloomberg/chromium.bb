// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"

#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

XRViewerPose::XRViewerPose(
    XRSession* session,
    std::unique_ptr<TransformationMatrix> pose_model_matrix)
    : session_(session), pose_model_matrix_(std::move(pose_model_matrix)) {
  // Can only update views with an invertible matrix.
  DCHECK(pose_model_matrix_->IsInvertible());

  TransformationMatrix inv_pose_matrix = pose_model_matrix_->Inverse();

  // session will update views if required
  // views array gets copied to views_
  views_ = session->views();

  for (Member<XRView>& view : views_) {
    view->UpdateViewMatrix(inv_pose_matrix);
  }
}

const HeapVector<Member<XRView>>& XRViewerPose::views() const {
  return views_;
}

DOMFloat32Array* XRViewerPose::poseModelMatrix() const {
  if (!pose_model_matrix_)
    return nullptr;
  return transformationMatrixToDOMFloat32Array(*pose_model_matrix_);
}

void XRViewerPose::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(views_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

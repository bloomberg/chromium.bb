// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRDevicePose.h"

#include "modules/xr/XRSession.h"
#include "modules/xr/XRView.h"

namespace blink {

namespace {

DOMFloat32Array* transformationMatrixToFloat32Array(
    const TransformationMatrix& matrix) {
  float array[] = {
      static_cast<float>(matrix.M11()), static_cast<float>(matrix.M12()),
      static_cast<float>(matrix.M13()), static_cast<float>(matrix.M14()),
      static_cast<float>(matrix.M21()), static_cast<float>(matrix.M22()),
      static_cast<float>(matrix.M23()), static_cast<float>(matrix.M24()),
      static_cast<float>(matrix.M31()), static_cast<float>(matrix.M32()),
      static_cast<float>(matrix.M33()), static_cast<float>(matrix.M34()),
      static_cast<float>(matrix.M41()), static_cast<float>(matrix.M42()),
      static_cast<float>(matrix.M43()), static_cast<float>(matrix.M44())};

  return DOMFloat32Array::Create(array, 16);
}

}  // namespace

XRDevicePose::XRDevicePose(
    XRSession* session,
    std::unique_ptr<TransformationMatrix> pose_model_matrix)
    : session_(session), pose_model_matrix_(std::move(pose_model_matrix)) {}

DOMFloat32Array* XRDevicePose::poseModelMatrix() const {
  if (!pose_model_matrix_)
    return nullptr;
  return transformationMatrixToFloat32Array(*pose_model_matrix_);
}

DOMFloat32Array* XRDevicePose::getViewMatrix(XRView* view) {
  if (view->session() != session_)
    return nullptr;

  if (!pose_model_matrix_->IsInvertible())
    return nullptr;

  TransformationMatrix view_matrix(pose_model_matrix_->Inverse());

  // Transform by the negative offset, since we're operating on the inverted
  // matrix
  const FloatPoint3D& view_offset = view->offset();
  view_matrix.PostTranslate3d(-view_offset.X(), -view_offset.Y(),
                              -view_offset.Z());

  return transformationMatrixToFloat32Array(view_matrix);
}

void XRDevicePose::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

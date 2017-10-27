// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRDevicePose.h"

#include "modules/vr/latest/VRSession.h"

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

VRDevicePose::VRDevicePose(
    VRSession* session,
    std::unique_ptr<TransformationMatrix> pose_model_matrix)
    : session_(session), pose_model_matrix_(std::move(pose_model_matrix)) {}

DOMFloat32Array* VRDevicePose::poseModelMatrix() const {
  if (!pose_model_matrix_)
    return nullptr;
  return transformationMatrixToFloat32Array(*pose_model_matrix_);
}

void VRDevicePose::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

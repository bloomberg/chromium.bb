// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRInputPose.h"

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

XRInputPose::XRInputPose(std::unique_ptr<TransformationMatrix> pointer_matrix,
                         std::unique_ptr<TransformationMatrix> grip_matrix,
                         bool emulated_position)
    : pointer_matrix_(std::move(pointer_matrix)),
      grip_matrix_(std::move(grip_matrix)),
      emulated_position_(emulated_position) {}

XRInputPose::~XRInputPose() {}

DOMFloat32Array* XRInputPose::pointerMatrix() const {
  if (!pointer_matrix_)
    return nullptr;
  return transformationMatrixToFloat32Array(*pointer_matrix_);
}

DOMFloat32Array* XRInputPose::gripMatrix() const {
  if (!grip_matrix_)
    return nullptr;
  return transformationMatrixToFloat32Array(*grip_matrix_);
}

void XRInputPose::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

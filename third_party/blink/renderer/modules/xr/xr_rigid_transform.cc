// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"

#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

XRRigidTransform::XRRigidTransform(DOMPointInit* position,
                                   DOMPointInit* orientation) {
  if (position) {
    position_ = DOMPointReadOnly::Create(position->x(), position->y(),
                                         position->z(), 1.0);
  } else {
    position_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }

  if (orientation) {
    orientation_ = makeNormalizedQuaternion(orientation);
  } else {
    orientation_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }

  // Computing transformation matrix from position and orientation is expensive,
  // so compute it lazily in matrix().
}

XRRigidTransform* XRRigidTransform::Create(DOMPointInit* position,
                                           DOMPointInit* orientation) {
  return MakeGarbageCollected<XRRigidTransform>(position, orientation);
}

DOMFloat32Array* XRRigidTransform::matrix() {
  if (!matrix_) {
    matrix_ = TransformationMatrix::Create();
    TransformationMatrix::DecomposedType decomp;
    memset(&decomp, 0, sizeof(decomp));
    decomp.perspective_w = 1;
    decomp.scale_x = 1;
    decomp.scale_y = 1;
    decomp.scale_z = 1;

    decomp.quaternion_x = orientation_->x();
    decomp.quaternion_y = orientation_->y();
    decomp.quaternion_z = orientation_->z();
    decomp.quaternion_w = orientation_->w();

    decomp.translate_x = position_->x();
    decomp.translate_y = position_->y();
    decomp.translate_z = position_->z();

    matrix_->Recompose(decomp);
  }

  return transformationMatrixToDOMFloat32Array(*matrix_);
}

void XRRigidTransform::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_);
  visitor->Trace(orientation_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

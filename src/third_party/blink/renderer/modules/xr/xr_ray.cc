// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_ray.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

XRRay::XRRay(std::unique_ptr<TransformationMatrix> matrix) {
  Set(std::move(matrix));
}

XRRay::XRRay(XRRigidTransform* transform) {
  DOMFloat32Array* m = transform->matrix();

  Set(DOMFloat32ArrayToTransformationMatrix(m));
}

XRRay::XRRay(DOMPointInit* origin, DOMPointInit* direction) {
  FloatPoint3D o;
  if (origin) {
    o = FloatPoint3D(origin->x(), origin->y(), origin->z());
  } else {
    o = FloatPoint3D(0.f, 0.f, 0.f);
  }

  FloatPoint3D d;
  if (direction) {
    d = FloatPoint3D(direction->x(), direction->y(), direction->z());
  } else {
    d = FloatPoint3D(0.f, 0.f, -1.f);
  }

  Set(o, d);
}

void XRRay::Set(std::unique_ptr<TransformationMatrix> matrix) {
  FloatPoint3D origin = matrix->MapPoint(FloatPoint3D(0, 0, 0));
  FloatPoint3D direction = matrix->MapPoint(FloatPoint3D(0, 0, -1));
  direction.Move(-origin.X(), -origin.Y(), -origin.Z());

  Set(origin, direction);
}

// Sets member variables from passed in |origin| and |direction|.
// All constructors eventually invoke this method.
// If the |direction|'s length is 0, this method will initialize direction to
// default vector (0, 0, -1).
void XRRay::Set(FloatPoint3D origin, FloatPoint3D direction) {
  DVLOG(3) << __FUNCTION__ << ": origin=" << origin.ToString()
           << ", direction=" << direction.ToString();

  if (direction.length() == 0.0f) {
    direction = FloatPoint3D{0, 0, -1};
  } else {
    direction.Normalize();
  }

  origin_ = DOMPointReadOnly::Create(origin.X(), origin.Y(), origin.Z(), 1.0);
  direction_ = DOMPointReadOnly::Create(direction.X(), direction.Y(),
                                        direction.Z(), 0.0);
}

XRRay* XRRay::Create(XRRigidTransform* transform) {
  return MakeGarbageCollected<XRRay>(transform);
}

XRRay* XRRay::Create() {
  return MakeGarbageCollected<XRRay>(nullptr, nullptr);
}

XRRay* XRRay::Create(DOMPointInit* origin) {
  return MakeGarbageCollected<XRRay>(origin, nullptr);
}

XRRay* XRRay::Create(DOMPointInit* origin, DOMPointInit* direction) {
  return MakeGarbageCollected<XRRay>(origin, direction);
}

XRRay::~XRRay() {}

DOMFloat32Array* XRRay::matrix() {
  DVLOG(3) << __FUNCTION__;

  if (!matrix_) {
    // Returned matrix should represent transformation from ray originating at
    // (0,0,0) with direction (0,0,-1) into ray originating at |origin_| with
    // direction |direction_|.

    TransformationMatrix matrix;

    // Translation from 0 to |origin_| is simply translation by |origin_|.
    matrix.Translate3d(origin_->x(), origin_->y(), origin_->z());

    const blink::FloatPoint3D initialRayDirection =
        blink::FloatPoint3D{0.f, 0.f, -1.f};
    const blink::FloatPoint3D desiredRayDirection = {
        direction_->x(), direction_->y(), direction_->z()};

    float cos_angle = initialRayDirection.Dot(desiredRayDirection);

    if (cos_angle > 0.9999) {
      // Vectors are co-linear or almost co-linear & face the same direction,
      // no rotation is needed.
    } else if (cos_angle < -0.9999) {
      // Vectors are co-linear or almost co-linear & face the opposite
      // direction, rotation by 180 degrees is needed & can be around any vector
      // perpendicular to (0,0,-1) so let's rotate by (1, 0, 0).
      blink::FloatPoint3D axis = FloatPoint3D{1, 0, 0};
      cos_angle = -1;

      matrix.Rotate3d(axis.X(), axis.Y(), axis.Z(),
                      rad2deg(std::acos(cos_angle)));
    } else {
      // Rotation needed - create it from axis-angle.
      blink::FloatPoint3D axis = initialRayDirection.Cross(desiredRayDirection);

      matrix.Rotate3d(axis.X(), axis.Y(), axis.Z(),
                      rad2deg(std::acos(cos_angle)));
    }

    matrix_ = transformationMatrixToDOMFloat32Array(matrix);
  }

  if (!matrix_ || !matrix_->View() || !matrix_->View()->Data()) {
    // A page may take the matrix value and detach it so matrix_ is a detached
    // array buffer.  This breaks the inspector, so return null instead.
    return nullptr;
  }

  return matrix_;
}

void XRRay::Trace(blink::Visitor* visitor) {
  visitor->Trace(origin_);
  visitor->Trace(direction_);
  visitor->Trace(matrix_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

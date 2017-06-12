// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/quaternion.h"

#include <algorithm>
#include <cmath>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

const double kEpsilon = 1e-5;

}  // namespace

Quaternion::Quaternion(const Vector3dF& axis, double theta) {
  // Rotation angle is the product of |angle| and the magnitude of |axis|.
  double length = axis.Length();
  if (std::abs(length) < kEpsilon)
    return;

  Vector3dF normalized = axis;
  normalized.Scale(1.0 / length);

  theta *= 0.5;
  double s = sin(theta);
  x_ = normalized.x() * s;
  y_ = normalized.y() * s;
  z_ = normalized.z() * s;
  w_ = cos(theta);
}

Quaternion::Quaternion(const Vector3dF& from, const Vector3dF& to) {
  double dot = gfx::DotProduct(from, to);
  double norm = sqrt(from.LengthSquared() * to.LengthSquared());
  double real = norm + dot;
  gfx::Vector3dF axis;
  if (real < kEpsilon * norm) {
    real = 0.0f;
    axis = std::abs(from.x()) > std::abs(from.z())
               ? gfx::Vector3dF{-from.y(), from.x(), 0.0}
               : gfx::Vector3dF{0.0, -from.z(), from.y()};
  } else {
    axis = gfx::CrossProduct(from, to);
  }
  x_ = axis.x();
  y_ = axis.y();
  z_ = axis.z();
  w_ = real;
  *this = this->Normalized();
}

// Taken from http://www.w3.org/TR/css3-transforms/.
Quaternion Quaternion::Slerp(const Quaternion& q, double t) const {
  double dot = x_ * q.x_ + y_ * q.y_ + z_ * q.z_ + w_ * q.w_;

  // Clamp dot to -1.0 <= dot <= 1.0.
  dot = std::min(std::max(dot, -1.0), 1.0);

  // Quaternions are facing the same direction.
  if (std::abs(dot - 1.0) < kEpsilon || std::abs(dot + 1.0) < kEpsilon)
    return *this;

  // TODO(vmpstr): In case the dot is 0, the vectors are exactly opposite
  // of each other. In this case, it's technically not correct to just pick one
  // of the vectors, we instead need to pick how to interpolate. However, the
  // spec isn't clear on this. If we don't handle the -1 case explicitly, it
  // results in inf and nans however, which is worse. See crbug.com/506543 for
  // more discussion.
  if (std::abs(dot) < kEpsilon)
    return *this;

  double denom = std::sqrt(1.0 - dot * dot);
  double theta = std::acos(dot);
  double w = std::sin(t * theta) * (1.0 / denom);

  double s1 = std::cos(t * theta) - dot * w;
  double s2 = w;

  return (s1 * *this) + (s2 * q);
}

Quaternion Quaternion::Lerp(const Quaternion& q, double t) const {
  return (((1.0 - t) * *this) + (t * q)).Normalized();
}

Quaternion Quaternion::Normalized() const {
  double length = x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_;
  if (length < kEpsilon)
    return *this;
  return *this / sqrt(length);
}

std::string Quaternion::ToString() const {
  return base::StringPrintf("[%f %f %f %f]", x_, y_, z_, w_);
}

}  // namespace gfx

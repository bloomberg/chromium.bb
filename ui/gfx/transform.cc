// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "ui/gfx/transform.h"

#include <cmath>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform_util.h"

namespace gfx {

namespace {

const SkMScalar kEpsilon = std::numeric_limits<float>::epsilon();

SkMScalar TanDegrees(double degrees) {
  double radians = degrees * M_PI / 180;
  return SkDoubleToMScalar(std::tan(radians));
}

inline bool ApproximatelyZero(SkMScalar x, SkMScalar tolerance) {
  return std::abs(x) <= tolerance;
}

inline bool ApproximatelyOne(SkMScalar x, SkMScalar tolerance) {
  return std::abs(x - SkDoubleToMScalar(1.0)) <= tolerance;
}

static float Round(float f) {
  if (f == 0.f)
    return f;
  return (f > 0.f) ? std::floor(f + 0.5f) : std::ceil(f - 0.5f);
}

}  // namespace

Transform::Transform(SkMScalar col1row1,
                     SkMScalar col2row1,
                     SkMScalar col3row1,
                     SkMScalar col4row1,
                     SkMScalar col1row2,
                     SkMScalar col2row2,
                     SkMScalar col3row2,
                     SkMScalar col4row2,
                     SkMScalar col1row3,
                     SkMScalar col2row3,
                     SkMScalar col3row3,
                     SkMScalar col4row3,
                     SkMScalar col1row4,
                     SkMScalar col2row4,
                     SkMScalar col3row4,
                     SkMScalar col4row4)
    : matrix_(SkMatrix44::kUninitialized_Constructor) {
  matrix_.set(0, 0, col1row1);
  matrix_.set(1, 0, col1row2);
  matrix_.set(2, 0, col1row3);
  matrix_.set(3, 0, col1row4);

  matrix_.set(0, 1, col2row1);
  matrix_.set(1, 1, col2row2);
  matrix_.set(2, 1, col2row3);
  matrix_.set(3, 1, col2row4);

  matrix_.set(0, 2, col3row1);
  matrix_.set(1, 2, col3row2);
  matrix_.set(2, 2, col3row3);
  matrix_.set(3, 2, col3row4);

  matrix_.set(0, 3, col4row1);
  matrix_.set(1, 3, col4row2);
  matrix_.set(2, 3, col4row3);
  matrix_.set(3, 3, col4row4);
}

Transform::Transform(SkMScalar col1row1,
                     SkMScalar col2row1,
                     SkMScalar col1row2,
                     SkMScalar col2row2,
                     SkMScalar x_translation,
                     SkMScalar y_translation)
    : matrix_(SkMatrix44::kIdentity_Constructor) {
  matrix_.set(0, 0, col1row1);
  matrix_.set(1, 0, col1row2);
  matrix_.set(0, 1, col2row1);
  matrix_.set(1, 1, col2row2);
  matrix_.set(0, 3, x_translation);
  matrix_.set(1, 3, y_translation);
}

Transform::Transform(const Quaternion& q)
    : matrix_(SkMatrix44::kUninitialized_Constructor) {
  double x = q.x();
  double y = q.y();
  double z = q.z();
  double w = q.w();

  // Implicitly calls matrix.setIdentity()
  matrix_.set3x3(SkDoubleToMScalar(1.0 - 2.0 * (y * y + z * z)),
                 SkDoubleToMScalar(2.0 * (x * y + z * w)),
                 SkDoubleToMScalar(2.0 * (x * z - y * w)),
                 SkDoubleToMScalar(2.0 * (x * y - z * w)),
                 SkDoubleToMScalar(1.0 - 2.0 * (x * x + z * z)),
                 SkDoubleToMScalar(2.0 * (y * z + x * w)),
                 SkDoubleToMScalar(2.0 * (x * z + y * w)),
                 SkDoubleToMScalar(2.0 * (y * z - x * w)),
                 SkDoubleToMScalar(1.0 - 2.0 * (x * x + y * y)));
}

void Transform::RotateAboutXAxis(double degrees) {
  double radians = degrees * M_PI / 180;
  SkMScalar cosTheta = SkDoubleToMScalar(std::cos(radians));
  SkMScalar sinTheta = SkDoubleToMScalar(std::sin(radians));
  if (matrix_.isIdentity()) {
      matrix_.set3x3(1, 0, 0,
                     0, cosTheta, sinTheta,
                     0, -sinTheta, cosTheta);
  } else {
    SkMatrix44 rot(SkMatrix44::kUninitialized_Constructor);
    rot.set3x3(1, 0, 0,
               0, cosTheta, sinTheta,
               0, -sinTheta, cosTheta);
    matrix_.preConcat(rot);
  }
}

void Transform::RotateAboutYAxis(double degrees) {
  double radians = degrees * M_PI / 180;
  SkMScalar cosTheta = SkDoubleToMScalar(std::cos(radians));
  SkMScalar sinTheta = SkDoubleToMScalar(std::sin(radians));
  if (matrix_.isIdentity()) {
      // Note carefully the placement of the -sinTheta for rotation about
      // y-axis is different than rotation about x-axis or z-axis.
      matrix_.set3x3(cosTheta, 0, -sinTheta,
                     0, 1, 0,
                     sinTheta, 0, cosTheta);
  } else {
    SkMatrix44 rot(SkMatrix44::kUninitialized_Constructor);
    rot.set3x3(cosTheta, 0, -sinTheta,
               0, 1, 0,
               sinTheta, 0, cosTheta);
    matrix_.preConcat(rot);
  }
}

void Transform::RotateAboutZAxis(double degrees) {
  double radians = degrees * M_PI / 180;
  SkMScalar cosTheta = SkDoubleToMScalar(std::cos(radians));
  SkMScalar sinTheta = SkDoubleToMScalar(std::sin(radians));
  if (matrix_.isIdentity()) {
      matrix_.set3x3(cosTheta, sinTheta, 0,
                     -sinTheta, cosTheta, 0,
                     0, 0, 1);
  } else {
    SkMatrix44 rot(SkMatrix44::kUninitialized_Constructor);
    rot.set3x3(cosTheta, sinTheta, 0,
               -sinTheta, cosTheta, 0,
               0, 0, 1);
    matrix_.preConcat(rot);
  }
}

void Transform::RotateAbout(const Vector3dF& axis, double degrees) {
  if (matrix_.isIdentity()) {
    matrix_.setRotateDegreesAbout(SkFloatToMScalar(axis.x()),
                                  SkFloatToMScalar(axis.y()),
                                  SkFloatToMScalar(axis.z()),
                                  SkDoubleToMScalar(degrees));
  } else {
    SkMatrix44 rot(SkMatrix44::kUninitialized_Constructor);
    rot.setRotateDegreesAbout(SkFloatToMScalar(axis.x()),
                              SkFloatToMScalar(axis.y()),
                              SkFloatToMScalar(axis.z()),
                              SkDoubleToMScalar(degrees));
    matrix_.preConcat(rot);
  }
}

void Transform::Scale(SkMScalar x, SkMScalar y) { matrix_.preScale(x, y, 1); }

void Transform::Scale3d(SkMScalar x, SkMScalar y, SkMScalar z) {
  matrix_.preScale(x, y, z);
}

void Transform::Translate(const Vector2dF& offset) {
  Translate(offset.x(), offset.y());
}

void Transform::Translate(SkMScalar x, SkMScalar y) {
  matrix_.preTranslate(x, y, 0);
}

void Transform::Translate3d(const Vector3dF& offset) {
  Translate3d(offset.x(), offset.y(), offset.z());
}

void Transform::Translate3d(SkMScalar x, SkMScalar y, SkMScalar z) {
  matrix_.preTranslate(x, y, z);
}

void Transform::Skew(double angle_x, double angle_y) {
  if (matrix_.isIdentity()) {
    matrix_.set(0, 1, TanDegrees(angle_x));
    matrix_.set(1, 0, TanDegrees(angle_y));
  } else {
    SkMatrix44 skew(SkMatrix44::kIdentity_Constructor);
    skew.set(0, 1, TanDegrees(angle_x));
    skew.set(1, 0, TanDegrees(angle_y));
    matrix_.preConcat(skew);
  }
}

void Transform::ApplyPerspectiveDepth(SkMScalar depth) {
  if (depth == 0)
    return;
  if (matrix_.isIdentity()) {
    matrix_.set(3, 2, -SK_MScalar1 / depth);
  } else {
    SkMatrix44 m(SkMatrix44::kIdentity_Constructor);
    m.set(3, 2, -SK_MScalar1 / depth);
    matrix_.preConcat(m);
  }
}

void Transform::PreconcatTransform(const Transform& transform) {
  matrix_.preConcat(transform.matrix_);
}

void Transform::ConcatTransform(const Transform& transform) {
  matrix_.postConcat(transform.matrix_);
}

bool Transform::IsApproximatelyIdentityOrTranslation(
    SkMScalar tolerance) const {
  DCHECK_GE(tolerance, 0);
  return
      ApproximatelyOne(matrix_.get(0, 0), tolerance) &&
      ApproximatelyZero(matrix_.get(1, 0), tolerance) &&
      ApproximatelyZero(matrix_.get(2, 0), tolerance) &&
      matrix_.get(3, 0) == 0 &&
      ApproximatelyZero(matrix_.get(0, 1), tolerance) &&
      ApproximatelyOne(matrix_.get(1, 1), tolerance) &&
      ApproximatelyZero(matrix_.get(2, 1), tolerance) &&
      matrix_.get(3, 1) == 0 &&
      ApproximatelyZero(matrix_.get(0, 2), tolerance) &&
      ApproximatelyZero(matrix_.get(1, 2), tolerance) &&
      ApproximatelyOne(matrix_.get(2, 2), tolerance) &&
      matrix_.get(3, 2) == 0 &&
      matrix_.get(3, 3) == 1;
}

bool Transform::IsIdentityOrIntegerTranslation() const {
  if (!IsIdentityOrTranslation())
    return false;

  float t[] = {matrix_.get(0, 3), matrix_.get(1, 3), matrix_.get(2, 3)};
  bool no_fractional_translation =
      base::IsValueInRangeForNumericType<int>(t[0]) &&
      base::IsValueInRangeForNumericType<int>(t[1]) &&
      base::IsValueInRangeForNumericType<int>(t[2]) &&
      static_cast<int>(t[0]) == t[0] && static_cast<int>(t[1]) == t[1] &&
      static_cast<int>(t[2]) == t[2];

  return no_fractional_translation;
}

bool Transform::IsBackFaceVisible() const {
  // Compute whether a layer with a forward-facing normal of (0, 0, 1, 0)
  // would have its back face visible after applying the transform.
  if (matrix_.isIdentity())
    return false;

  // This is done by transforming the normal and seeing if the resulting z
  // value is positive or negative. However, note that transforming a normal
  // actually requires using the inverse-transpose of the original transform.
  //
  // We can avoid inverting and transposing the matrix since we know we want
  // to transform only the specific normal vector (0, 0, 1, 0). In this case,
  // we only need the 3rd row, 3rd column of the inverse-transpose. We can
  // calculate only the 3rd row 3rd column element of the inverse, skipping
  // everything else.
  //
  // For more information, refer to:
  //   http://en.wikipedia.org/wiki/Invertible_matrix#Analytic_solution
  //

  double determinant = matrix_.determinant();

  // If matrix was not invertible, then just assume back face is not visible.
  if (determinant == 0)
    return false;

  // Compute the cofactor of the 3rd row, 3rd column.
  double cofactor_part_1 =
      matrix_.get(0, 0) * matrix_.get(1, 1) * matrix_.get(3, 3);

  double cofactor_part_2 =
      matrix_.get(0, 1) * matrix_.get(1, 3) * matrix_.get(3, 0);

  double cofactor_part_3 =
      matrix_.get(0, 3) * matrix_.get(1, 0) * matrix_.get(3, 1);

  double cofactor_part_4 =
      matrix_.get(0, 0) * matrix_.get(1, 3) * matrix_.get(3, 1);

  double cofactor_part_5 =
      matrix_.get(0, 1) * matrix_.get(1, 0) * matrix_.get(3, 3);

  double cofactor_part_6 =
      matrix_.get(0, 3) * matrix_.get(1, 1) * matrix_.get(3, 0);

  double cofactor33 =
      cofactor_part_1 +
      cofactor_part_2 +
      cofactor_part_3 -
      cofactor_part_4 -
      cofactor_part_5 -
      cofactor_part_6;

  // Technically the transformed z component is cofactor33 / determinant.  But
  // we can avoid the costly division because we only care about the resulting
  // +/- sign; we can check this equivalently by multiplication.
  return cofactor33 * determinant < -kEpsilon;
}

bool Transform::GetInverse(Transform* transform) const {
  if (!matrix_.invert(&transform->matrix_)) {
    // Initialize the return value to identity if this matrix turned
    // out to be un-invertible.
    transform->MakeIdentity();
    return false;
  }

  return true;
}

bool Transform::Preserves2dAxisAlignment() const {
  // Check whether an axis aligned 2-dimensional rect would remain axis-aligned
  // after being transformed by this matrix (and implicitly projected by
  // dropping any non-zero z-values).
  //
  // The 4th column can be ignored because translations don't affect axis
  // alignment. The 3rd column can be ignored because we are assuming 2d
  // inputs, where z-values will be zero. The 3rd row can also be ignored
  // because we are assuming 2d outputs, and any resulting z-value is dropped
  // anyway. For the inner 2x2 portion, the only effects that keep a rect axis
  // aligned are (1) swapping axes and (2) scaling axes. This can be checked by
  // verifying only 1 element of every column and row is non-zero.  Degenerate
  // cases that project the x or y dimension to zero are considered to preserve
  // axis alignment.
  //
  // If the matrix does have perspective component that is affected by x or y
  // values: The current implementation conservatively assumes that axis
  // alignment is not preserved.

  bool has_x_or_y_perspective =
      matrix_.get(3, 0) != 0 || matrix_.get(3, 1) != 0;

  int num_non_zero_in_row_0 = 0;
  int num_non_zero_in_row_1 = 0;
  int num_non_zero_in_col_0 = 0;
  int num_non_zero_in_col_1 = 0;

  if (std::abs(matrix_.get(0, 0)) > kEpsilon) {
    num_non_zero_in_row_0++;
    num_non_zero_in_col_0++;
  }

  if (std::abs(matrix_.get(0, 1)) > kEpsilon) {
    num_non_zero_in_row_0++;
    num_non_zero_in_col_1++;
  }

  if (std::abs(matrix_.get(1, 0)) > kEpsilon) {
    num_non_zero_in_row_1++;
    num_non_zero_in_col_0++;
  }

  if (std::abs(matrix_.get(1, 1)) > kEpsilon) {
    num_non_zero_in_row_1++;
    num_non_zero_in_col_1++;
  }

  return
      num_non_zero_in_row_0 <= 1 &&
      num_non_zero_in_row_1 <= 1 &&
      num_non_zero_in_col_0 <= 1 &&
      num_non_zero_in_col_1 <= 1 &&
      !has_x_or_y_perspective;
}

void Transform::Transpose() {
  matrix_.transpose();
}

void Transform::FlattenTo2d() {
  matrix_.set(2, 0, 0.0);
  matrix_.set(2, 1, 0.0);
  matrix_.set(0, 2, 0.0);
  matrix_.set(1, 2, 0.0);
  matrix_.set(2, 2, 1.0);
  matrix_.set(3, 2, 0.0);
  matrix_.set(2, 3, 0.0);
}

bool Transform::IsFlat() const {
  return matrix_.get(2, 0) == 0.0 && matrix_.get(2, 1) == 0.0 &&
         matrix_.get(0, 2) == 0.0 && matrix_.get(1, 2) == 0.0 &&
         matrix_.get(2, 2) == 1.0 && matrix_.get(3, 2) == 0.0 &&
         matrix_.get(2, 3) == 0.0;
}

Vector2dF Transform::To2dTranslation() const {
  return gfx::Vector2dF(SkMScalarToFloat(matrix_.get(0, 3)),
                        SkMScalarToFloat(matrix_.get(1, 3)));
}

void Transform::TransformPoint(Point* point) const {
  DCHECK(point);
  TransformPointInternal(matrix_, point);
}

void Transform::TransformPoint(Point3F* point) const {
  DCHECK(point);
  TransformPointInternal(matrix_, point);
}

void Transform::TransformVector(Vector3dF* vector) const {
  DCHECK(vector);
  TransformVectorInternal(matrix_, vector);
}

bool Transform::TransformPointReverse(Point* point) const {
  DCHECK(point);

  // TODO(sad): Try to avoid trying to invert the matrix.
  SkMatrix44 inverse(SkMatrix44::kUninitialized_Constructor);
  if (!matrix_.invert(&inverse))
    return false;

  TransformPointInternal(inverse, point);
  return true;
}

bool Transform::TransformPointReverse(Point3F* point) const {
  DCHECK(point);

  // TODO(sad): Try to avoid trying to invert the matrix.
  SkMatrix44 inverse(SkMatrix44::kUninitialized_Constructor);
  if (!matrix_.invert(&inverse))
    return false;

  TransformPointInternal(inverse, point);
  return true;
}

void Transform::TransformRect(RectF* rect) const {
  if (matrix_.isIdentity())
    return;

  SkRect src = RectFToSkRect(*rect);
  const SkMatrix& matrix = matrix_;
  matrix.mapRect(&src);
  *rect = SkRectToRectF(src);
}

bool Transform::TransformRectReverse(RectF* rect) const {
  if (matrix_.isIdentity())
    return true;

  SkMatrix44 inverse(SkMatrix44::kUninitialized_Constructor);
  if (!matrix_.invert(&inverse))
    return false;

  const SkMatrix& matrix = inverse;
  SkRect src = RectFToSkRect(*rect);
  matrix.mapRect(&src);
  *rect = SkRectToRectF(src);
  return true;
}

void Transform::TransformBox(BoxF* box) const {
  BoxF bounds;
  bool first_point = true;
  for (int corner = 0; corner < 8; ++corner) {
    gfx::Point3F point = box->origin();
    point += gfx::Vector3dF(corner & 1 ? box->width() : 0.f,
                            corner & 2 ? box->height() : 0.f,
                            corner & 4 ? box->depth() : 0.f);
    TransformPoint(&point);
    if (first_point) {
      bounds.set_origin(point);
      first_point = false;
    } else {
      bounds.ExpandTo(point);
    }
  }
  *box = bounds;
}

bool Transform::TransformBoxReverse(BoxF* box) const {
  gfx::Transform inverse = *this;
  if (!GetInverse(&inverse))
    return false;
  inverse.TransformBox(box);
  return true;
}

bool Transform::Blend(const Transform& from, double progress) {
  DecomposedTransform to_decomp;
  DecomposedTransform from_decomp;
  if (!DecomposeTransform(&to_decomp, *this) ||
      !DecomposeTransform(&from_decomp, from))
    return false;

  to_decomp = BlendDecomposedTransforms(to_decomp, from_decomp, progress);

  matrix_ = ComposeTransform(to_decomp).matrix();
  return true;
}

void Transform::RoundTranslationComponents() {
  matrix_.set(0, 3, Round(matrix_.get(0, 3)));
  matrix_.set(1, 3, Round(matrix_.get(1, 3)));
}

void Transform::TransformPointInternal(const SkMatrix44& xform,
                                       Point3F* point) const {
  if (xform.isIdentity())
    return;

  SkMScalar p[4] = {SkFloatToMScalar(point->x()), SkFloatToMScalar(point->y()),
                    SkFloatToMScalar(point->z()), 1};

  xform.mapMScalars(p);

  if (p[3] != SK_MScalar1 && p[3] != 0.f) {
    float w_inverse = SK_MScalar1 / p[3];
    point->SetPoint(p[0] * w_inverse, p[1] * w_inverse, p[2] * w_inverse);
  } else {
    point->SetPoint(p[0], p[1], p[2]);
  }
}

void Transform::TransformVectorInternal(const SkMatrix44& xform,
                                        Vector3dF* vector) const {
  if (xform.isIdentity())
    return;

  SkMScalar p[4] = {SkFloatToMScalar(vector->x()),
                    SkFloatToMScalar(vector->y()),
                    SkFloatToMScalar(vector->z()), 0};

  xform.mapMScalars(p);

  vector->set_x(p[0]);
  vector->set_y(p[1]);
  vector->set_z(p[2]);
}

void Transform::TransformPointInternal(const SkMatrix44& xform,
                                       Point* point) const {
  if (xform.isIdentity())
    return;

  SkMScalar p[4] = {SkIntToMScalar(point->x()), SkIntToMScalar(point->y()),
                    0, 1};

  xform.mapMScalars(p);

  point->SetPoint(ToRoundedInt(p[0]), ToRoundedInt(p[1]));
}

bool Transform::ApproximatelyEqual(const gfx::Transform& transform) const {
  static const float component_tolerance = 0.1f;

  // We may have a larger discrepancy in the scroll components due to snapping
  // (floating point error might round the other way).
  static const float translation_tolerance = 1.f;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      const float delta =
          std::abs(matrix().get(row, col) - transform.matrix().get(row, col));
      const float tolerance =
          col == 3 && row < 3 ? translation_tolerance : component_tolerance;
      if (delta > tolerance)
        return false;
    }
  }

  return true;
}

std::string Transform::ToString() const {
  return base::StringPrintf(
      "[ %+0.4f %+0.4f %+0.4f %+0.4f  \n"
      "  %+0.4f %+0.4f %+0.4f %+0.4f  \n"
      "  %+0.4f %+0.4f %+0.4f %+0.4f  \n"
      "  %+0.4f %+0.4f %+0.4f %+0.4f ]\n",
      matrix_.get(0, 0),
      matrix_.get(0, 1),
      matrix_.get(0, 2),
      matrix_.get(0, 3),
      matrix_.get(1, 0),
      matrix_.get(1, 1),
      matrix_.get(1, 2),
      matrix_.get(1, 3),
      matrix_.get(2, 0),
      matrix_.get(2, 1),
      matrix_.get(2, 2),
      matrix_.get(2, 3),
      matrix_.get(3, 0),
      matrix_.get(3, 1),
      matrix_.get(3, 2),
      matrix_.get(3, 3));
}

}  // namespace gfx

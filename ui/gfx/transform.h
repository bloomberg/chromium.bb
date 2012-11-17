// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_H_
#define UI_GFX_TRANSFORM_H_

#include "base/compiler_specific.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/base/ui_export.h"

namespace gfx {

class RectF;
class Point;
class Point3F;

// 4x4 transformation matrix. Transform is cheap and explicitly allows
// copy/assign.
class UI_EXPORT Transform {
 public:
  Transform();
  ~Transform();

  bool operator==(const Transform& rhs) const;
  bool operator!=(const Transform& rhs) const;

  // NOTE: The 'Set' functions overwrite the previously set transformation
  // parameters. The 'Concat' functions apply a transformation (e.g. rotation,
  // scale, translate) on top of the existing transforms, instead of overwriting
  // them.

  // NOTE: The order of the 'Set' function calls do not matter. However, the
  // order of the 'Concat' function calls do matter, especially when combined
  // with the 'Set' functions.

  // Sets the rotation of the transformation.
  void SetRotate(double degree);

  // Sets the rotation of the transform (about a vector).
  void SetRotateAbout(const Point3F& point, double degree);

  // Sets the scaling parameters.
  void SetScaleX(double x);
  void SetScaleY(double y);
  void SetScaleZ(double z);
  void SetScale(double x, double y);
  void SetScale3d(double x, double y, double z);

  // Sets the translation parameters.
  void SetTranslateX(double x);
  void SetTranslateY(double y);
  void SetTranslateZ(double z);
  void SetTranslate(double x, double y);
  void SetTranslate3d(double x, double y, double z);

  // Sets the skew parameters.
  void SetSkewX(double angle);
  void SetSkewY(double angle);

  // Creates a perspective matrix.
  // Based on the 'perspective' operation from
  // http://www.w3.org/TR/css3-3d-transforms/#transform-functions
  void SetPerspectiveDepth(double depth);

  // Applies a rotation on the current transformation.
  void ConcatRotate(double degree);

  // Applies an axis-angle rotation on the current transformation.
  void ConcatRotateAbout(const Point3F& point, double degree);

  // Applies scaling on current transform.
  void ConcatScale(double x, double y);
  void ConcatScale3d(double x, double y, double z);

  // Applies translation on current transform.
  void ConcatTranslate(double x, double y);
  void ConcatTranslate3d(double x, double y, double z);

  // Applies a perspective on current transform.
  void ConcatPerspectiveDepth(double depth);

  // Applies a skew on the current transform.
  void ConcatSkewX(double angle_x);
  void ConcatSkewY(double angle_y);

  // Applies the current transformation on a rotation and assigns the result
  // to |this|.
  void PreconcatRotate(double degree);

  // Applies the current transformation on an axis-angle rotation and assigns
  // the result to |this|.
  void PreconcatRotateAbout(const Point3F& point, double degree);

  // Applies the current transformation on a scaling and assigns the result
  // to |this|.
  void PreconcatScale(double x, double y);
  void PreconcatScale3d(double x, double y, double z);

  // Applies the current transformation on a translation and assigns the result
  // to |this|.
  void PreconcatTranslate(double x, double y);
  void PreconcatTranslate3d(double x, double y, double z);

  // Applies the current transformation on a skew and assigns the result
  // to |this|.
  void PreconcatSkewX(double angle_x);
  void PreconcatSkewY(double angle_y);

  // Applies the current transformation on a perspective transform and assigns
  // the result to |this|.
  void PreconcatPerspectiveDepth(double depth);

  // Applies a transformation on the current transformation
  // (i.e. 'this = this * transform;').
  void PreconcatTransform(const Transform& transform);

  // Applies a transformation on the current transformation
  // (i.e. 'this = transform * this;').
  void ConcatTransform(const Transform& transform);

  // Returns true if this is the identity matrix.
  bool IsIdentity() const;

  // Returns true if this transform is non-singular.
  bool IsInvertible() const;

  // Inverts the transform which is passed in. Returns true if successful.
  bool GetInverse(Transform* transform) const WARN_UNUSED_RESULT;

  // Transposes this transform in place.
  void Transpose();

  // Applies the transformation on the point. Returns true if the point is
  // transformed successfully.
  void TransformPoint(Point3F& point) const;

  // Applies the transformation on the point. Returns true if the point is
  // transformed successfully. Rounds the result to the nearest point.
  void TransformPoint(Point& point) const;

  // Applies the reverse transformation on the point. Returns true if the
  // transformation can be inverted.
  bool TransformPointReverse(Point3F& point) const;

  // Applies the reverse transformation on the point. Returns true if the
  // transformation can be inverted. Rounds the result to the nearest point.
  bool TransformPointReverse(Point& point) const;

  // Applies transformation on the rectangle. Returns true if the transformed
  // rectangle was axis aligned. If it returns false, rect will be the
  // smallest axis aligned bounding box containing the transformed rect.
  void TransformRect(RectF* rect) const;

  // Applies the reverse transformation on the rectangle. Returns true if
  // the transformed rectangle was axis aligned. If it returns false,
  // rect will be the smallest axis aligned bounding box containing the
  // transformed rect.
  bool TransformRectReverse(RectF* rect) const;

  // Decomposes |this| and |from|, interpolates the decomposed values, and
  // sets |this| to the reconstituted result. Returns false if either matrix
  // can't be decomposed. Uses routines described in this spec:
  // http://www.w3.org/TR/css3-3d-transforms/.
  //
  // Note: this call is expensive since we need to decompose the transform. If
  // you're going to be calling this rapidly (e.g., in an animation) you should
  // decompose once using gfx::DecomposeTransforms and reuse your
  // DecomposedTransform.
  bool Blend(const Transform& from, double progress);

  // Returns the underlying matrix.
  const SkMatrix44& matrix() const { return matrix_; }
  SkMatrix44& matrix() { return matrix_; }

 private:
  void TransformPointInternal(const SkMatrix44& xform,
                              Point& point) const;

  void TransformPointInternal(const SkMatrix44& xform,
                              Point3F& point) const;

  SkMatrix44 matrix_;

  // copy/assign are allowed.
};

}  // namespace gfx

#endif  // UI_GFX_TRANSFORM_H_

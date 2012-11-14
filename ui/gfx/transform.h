// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_H_
#define UI_GFX_TRANSFORM_H_

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
  void SetRotate(float degree);

  // Sets the rotation of the transform (about a vector).
  void SetRotateAbout(const Point3F& point, float degree);

  // Sets the scaling parameters.
  void SetScaleX(float x);
  void SetScaleY(float y);
  void SetScale(float x, float y);

  // Sets the translation parameters.
  void SetTranslateX(float x);
  void SetTranslateY(float y);
  void SetTranslate(float x, float y);

  // Creates a perspective matrix.
  // Based on the 'perspective' operation from
  // http://www.w3.org/TR/css3-3d-transforms/#transform-functions
  void SetPerspectiveDepth(float depth);

  // Applies a rotation on the current transformation.
  void ConcatRotate(float degree);

  // Applies an axis-angle rotation on the current transformation.
  void ConcatRotateAbout(const Point3F& point, float degree);

  // Applies scaling on current transform.
  void ConcatScale(float x, float y);

  // Applies translation on current transform.
  void ConcatTranslate(float x, float y);

  // Applies a perspective on current transform.
  void ConcatPerspectiveDepth(float depth);

  // Applies a transformation on the current transformation
  // (i.e. 'this = this * transform;').
  void PreconcatTransform(const Transform& transform);

  // Applies a transformation on the current transformation
  // (i.e. 'this = transform * this;').
  void ConcatTransform(const Transform& transform);

  // Does the transformation change anything?
  bool HasChange() const;

  // Inverts the transform which is passed in. Returns true if successful.
  bool GetInverse(Transform* transform) const;

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

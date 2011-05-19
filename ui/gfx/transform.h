// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_H_
#define UI_GFX_TRANSFORM_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace gfx {
class Point;
class Rect;
}

namespace ui {

// 3x3 transformation matrix. Transform is cheap and explicitly allows
// copy/assign.
// TODO: make this a 4x4.
class Transform {
 public:
  Transform();
  ~Transform();

  // NOTE: The 'Set' functions overwrite the previously set transformation
  // parameters. The 'Concat' functions apply a transformation (e.g. rotation,
  // scale, translate) on top of the existing transforms, instead of overwriting
  // them.

  // NOTE: The order of the 'Set' function calls do not matter. However, the
  // order of the 'Concat' function calls do matter, especially when combined
  // with the 'Set' functions.

  // Sets the rotation of the transformation.
  void SetRotate(float degree);

  // Sets the scaling parameters.
  void SetScaleX(float x);
  void SetScaleY(float y);
  void SetScale(float x, float y);

  // Sets the translation parameters.
  void SetTranslateX(float x);
  void SetTranslateY(float y);
  void SetTranslate(float x, float y);

  // Applies rotation on the current transformation.
  void ConcatRotate(float degree);

  // Applies scaling on current transform.
  void ConcatScale(float x, float y);

  // Applies translation on current transform.
  void ConcatTranslate(float x, float y);

  // Applies a transformation on the current transformation
  // (i.e. 'this = this * transform;'). Returns true if the result can be
  // represented.
  bool PreconcatTransform(const Transform& transform);

  // Applies a transformation on the current transformation
  // (i.e. 'this = transform * this;'). Returns true if the result can be
  // represented.
  bool ConcatTransform(const Transform& transform);

  // Does the transformation change anything?
  bool HasChange() const;

  // Applies the transformation on the point. Returns true if the point is
  // transformed successfully.
  bool TransformPoint(gfx::Point* point);

  // Applies the reverse transformation on the point. Returns true if the point
  // is transformed successfully.
  bool TransformPointReverse(gfx::Point* point);

  // Applies transformation on the rectangle. Returns true of the rectangle is
  // transformed successfully.
  bool TransformRect(gfx::Rect* rect);

  // Returns the underlying matrix.
  const SkMatrix& matrix() const { return matrix_; }

 private:
  SkMatrix matrix_;

  // copy/assign are allowed.
};

}  // namespace ui

#endif  // UI_GFX_TRANSFORM_H_

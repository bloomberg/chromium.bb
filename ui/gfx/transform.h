// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_H_
#define UI_GFX_TRANSFORM_H_
#pragma once

namespace gfx {
class Point;
class Rect;
}

namespace ui {

// Transformation interface.
// Classes implement this interface to apply transformations (e.g. rotation,
// scaling etc.) on UI components.
class Transform {
 public:
  virtual ~Transform() {}

  // Creates an object that implements this interface (e.g. using skia
  // transformation matrices).
  static Transform* Create();

  // NOTE: The 'Set' functions overwrite the previously set transformation
  // parameters. The 'Concat' functions apply a transformation (e.g. rotation,
  // scale, translate) on top of the existing transforms, instead of overwriting
  // them.

  // NOTE: The order of the 'Set' function calls do not matter. However, the
  // order of the 'Concat' function calls do matter, especially when combined
  // with the 'Set' functions.

  // Sets the rotation of the transformation.
  virtual void SetRotate(float degree) = 0;

  // Sets the scaling parameters.
  virtual void SetScaleX(float x) = 0;
  virtual void SetScaleY(float y) = 0;
  virtual void SetScale(float x, float y) = 0;

  // Sets the translation parameters.
  virtual void SetTranslateX(float x) = 0;
  virtual void SetTranslateY(float y) = 0;
  virtual void SetTranslate(float x, float y) = 0;

  // Applies rotation on the current transformation.
  virtual void ConcatRotate(float degree) = 0;

  // Applies scaling on current transform.
  virtual void ConcatScale(float x, float y) = 0;

  // Applies translation on current transform.
  virtual void ConcatTranslate(float x, float y) = 0;

  // Applies a transformation on the current transformation
  // (i.e. 'this = this * transform;'). Returns true if the result can be
  // represented.
  virtual bool ConcatTransform(const Transform& transform) = 0;

  // Does the transformation change anything?
  virtual bool HasChange() const = 0;

  // Applies the transformation on the point. Returns true if the point is
  // transformed successfully.
  virtual bool TransformPoint(gfx::Point* point) = 0;

  // Applies the reverse transformation on the point. Returns true if the point
  // is transformed successfully.
  virtual bool TransformPointReverse(gfx::Point* point) = 0;

  // Applies transformation on the rectangle. Returns true of the rectangle is
  // transformed successfully.
  virtual bool TransformRect(gfx::Rect* rect) = 0;

  // operator=.
  virtual void Copy(const Transform& transform) = 0;
};

}  // namespace ui

#endif  // UI_GFX_TRANSFORM_H_

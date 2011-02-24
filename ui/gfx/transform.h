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
  // Create an object that implements this interface (e.g. using skia
  // transformation matrices).
  static Transform* Create();

  // Set the rotation of the transformation.
  virtual void SetRotate(float degree) = 0;

  // Set the scaling parameters.
  virtual void SetScaleX(float x) = 0;
  virtual void SetScaleY(float y) = 0;
  virtual void SetScale(float x, float y) = 0;

  // Set the translation parameters.
  virtual void SetTranslateX(float x) = 0;
  virtual void SetTranslateY(float y) = 0;
  virtual void SetTranslate(float x, float y) = 0;

  // Apply rotation on the current transformation.
  virtual void ConcatRotate(float degree) = 0;

  // Apply scaling on current transform.
  virtual void ConcatScale(float x, float y) = 0;

  // Apply translation on current transform.
  virtual void ConcatTranslate(float x, float y) = 0;

  // Apply a transformation on the current transformation
  // (i.e. 'this = this * transform;')
  virtual bool ConcatTransform(const Transform& transform) = 0;

  // Does the transformation change anything?
  virtual bool HasChange() const = 0;

  // Apply the transformation on the point.
  virtual bool TransformPoint(gfx::Point* point) = 0;

  // Apply the reverse transformation on the point.
  virtual bool TransformPointReverse(gfx::Point* point) = 0;

  // Apply transformatino on the rectangle.
  virtual bool TransformRect(gfx::Rect* rect) = 0;
};

}  // namespace ui

#endif  // UI_GFX_TRANSFORM_H_

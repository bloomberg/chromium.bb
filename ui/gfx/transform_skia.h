// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_SKIA_H_
#define UI_GFX_TRANSFORM_SKIA_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_ptr.h"
#include "ui/gfx/transform.h"

class SkMatrix;

namespace gfx {
class CanvasSkia;
}

namespace ui {

// Transformation using skia transformation matrices.
class TransformSkia : public Transform {
 public:
  TransformSkia();
  virtual ~TransformSkia();

  // Overridden from ui::Transform
  virtual void SetRotate(float degree) OVERRIDE;
  virtual void SetScaleX(float x) OVERRIDE;
  virtual void SetScaleY(float y) OVERRIDE;
  virtual void SetScale(float x, float y) OVERRIDE;
  virtual void SetTranslateX(float x) OVERRIDE;
  virtual void SetTranslateY(float y) OVERRIDE;
  virtual void SetTranslate(float x, float y) OVERRIDE;
  virtual void ConcatRotate(float degree) OVERRIDE;
  virtual void ConcatScale(float x, float y) OVERRIDE;
  virtual void ConcatTranslate(float x, float y) OVERRIDE;
  virtual bool ConcatTransform(const Transform& transform) OVERRIDE;
  virtual bool HasChange() const OVERRIDE;
  virtual bool TransformPoint(gfx::Point* point) OVERRIDE;
  virtual bool TransformPointReverse(gfx::Point* point) OVERRIDE;
  virtual bool TransformRect(gfx::Rect* rect) OVERRIDE;

 private:
  friend class gfx::CanvasSkia;
  scoped_ptr<SkMatrix> matrix_;

  DISALLOW_COPY_AND_ASSIGN(TransformSkia);
};

}  // namespace ui

#endif  // UI_GFX_TRANSFORM_SKIA_H_

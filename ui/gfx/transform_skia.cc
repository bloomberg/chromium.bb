// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/transform_skia.h"

#include "third_party/skia/include/core/SkMatrix.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace ui {

// static
Transform* Transform::Create() {
  return new TransformSkia();
}

TransformSkia::TransformSkia() {
  matrix_.reset(new SkMatrix);
  matrix_->reset();
}

void TransformSkia::SetRotate(float degree) {
  matrix_->setRotate(SkFloatToScalar(degree));
}

void TransformSkia::SetScaleX(float x) {
  matrix_->setScaleX(SkFloatToScalar(x));
}

void TransformSkia::SetScaleY(float y) {
  matrix_->setScaleY(SkFloatToScalar(y));
}

void TransformSkia::SetScale(float x, float y) {
  matrix_->setScale(SkFloatToScalar(x), SkFloatToScalar(y));
}

void TransformSkia::SetTranslateX(float x) {
  matrix_->setTranslateX(SkFloatToScalar(x));
}

void TransformSkia::SetTranslateY(float y) {
  matrix_->setTranslateY(SkFloatToScalar(y));
}

void TransformSkia::SetTranslate(float x, float y) {
  matrix_->setTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
}

void TransformSkia::ConcatRotate(float degree) {
  matrix_->postRotate(SkFloatToScalar(degree));
}

void TransformSkia::ConcatScale(float x, float y) {
  matrix_->postScale(SkFloatToScalar(x), SkFloatToScalar(y));
}

void TransformSkia::ConcatTranslate(float x, float y) {
  matrix_->postTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
}

bool TransformSkia::ConcatTransform(const Transform& transform) {
  return matrix_->setConcat(*reinterpret_cast<const TransformSkia&>
      (transform).matrix_.get(), *matrix_.get());
}

bool TransformSkia::HasChange() const {
  return !matrix_->isIdentity();
}

bool TransformSkia::TransformPoint(gfx::Point* point) {
  SkPoint skp;
  matrix_->mapXY(SkIntToScalar(point->x()), SkIntToScalar(point->y()), &skp);
  point->SetPoint(static_cast<int>(skp.fX), static_cast<int>(skp.fY));
  return true;
}

bool TransformSkia::TransformPointReverse(gfx::Point* point) {
  SkMatrix inverse;
  // TODO(sad): Try to avoid trying to invert the matrix.
  if (matrix_->invert(&inverse)) {
    SkPoint skp;
    inverse.mapXY(SkIntToScalar(point->x()), SkIntToScalar(point->y()), &skp);
    point->SetPoint(static_cast<int>(skp.fX), static_cast<int>(skp.fY));
    return true;
  }
  return false;
}

bool TransformSkia::TransformRect(gfx::Rect* rect) {
  SkRect src = gfx::RectToSkRect(*rect);
  if (!matrix_->mapRect(&src))
    return false;
  gfx::Rect xrect = gfx::SkRectToRect(src);
  rect->SetRect(xrect.x(), xrect.y(), xrect.width(), xrect.height());
  return true;
}

}  // namespace ui

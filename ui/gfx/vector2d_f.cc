// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/vector2d_f.h"

#include <cmath>

#include "base/stringprintf.h"

namespace gfx {

Vector2dF::Vector2dF()
    : x_(0),
      y_(0) {
}

Vector2dF::Vector2dF(float x, float y)
    : x_(x),
      y_(y) {
}

std::string Vector2dF::ToString() const {
  return base::StringPrintf("[%f %f]", x_, y_);
}

bool Vector2dF::IsZero() const {
  return x_ == 0 && y_ == 0;
}

void Vector2dF::Add(const Vector2dF& other) {
  x_ += other.x_;
  y_ += other.y_;
}

void Vector2dF::Subtract(const Vector2dF& other) {
  x_ -= other.x_;
  y_ -= other.y_;
}

double Vector2dF::LengthSquared() const {
  return static_cast<double>(x_) * x_ + static_cast<double>(y_) * y_;
}

float Vector2dF::Length() const {
  return static_cast<float>(std::sqrt(LengthSquared()));
}

void Vector2dF::Scale(float x_scale, float y_scale) {
  x_ *= x_scale;
  y_ *= y_scale;
}

}  // namespace gfx

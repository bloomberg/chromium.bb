// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_F_H_
#define UI_GFX_POINT_F_H_
#pragma once

#include <string>

#include "ui/base/ui_export.h"
#include "ui/gfx/point_base.h"

namespace gfx {
class Point;

// A floating version of gfx::Point.
class UI_EXPORT PointF : public PointBase<PointF, float> {
 public:
  PointF();
  PointF(float x, float y);
  explicit PointF(Point& point);
  ~PointF() {}

  Point ToPoint() const;

  // Returns a string representation of point.
  std::string ToString() const;
};

}  // namespace gfx

#endif  // UI_GFX_POINT_F_H_

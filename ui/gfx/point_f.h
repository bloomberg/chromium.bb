// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_F_H_
#define UI_GFX_POINT_F_H_

#include <string>

#include "ui/base/ui_export.h"
#include "ui/gfx/point_base.h"

namespace gfx {

// A floating version of gfx::Point.
class UI_EXPORT PointF : public PointBase<PointF, float> {
 public:
  PointF();
  PointF(float x, float y);
  ~PointF();

  // Returns a string representation of point.
  std::string ToString() const;
};

inline PointF operator+(PointF lhs, PointF rhs) {
  return lhs.Add(rhs);
}

inline PointF operator-(PointF lhs, PointF rhs) {
  return lhs.Subtract(rhs);
}

#if !defined(COMPILER_MSVC)
extern template class PointBase<PointF, float>;
#endif

}  // namespace gfx

#endif  // UI_GFX_POINT_F_H_

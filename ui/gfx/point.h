// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_H_
#define UI_GFX_POINT_H_
#pragma once

#include "ui/base/ui_export.h"
#include "ui/gfx/point_base.h"

#if defined(OS_WIN)
typedef unsigned long DWORD;
typedef struct tagPOINT POINT;
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

// A point has an x and y coordinate.
class UI_EXPORT Point : public PointBase<Point, int> {
 public:
  Point();
  Point(int x, int y);
#if defined(OS_WIN)
  // |point| is a DWORD value that contains a coordinate.  The x-coordinate is
  // the low-order short and the y-coordinate is the high-order short.  This
  // value is commonly acquired from GetMessagePos/GetCursorPos.
  explicit Point(DWORD point);
  explicit Point(const POINT& point);
  Point& operator=(const POINT& point);
#elif defined(OS_MACOSX)
  explicit Point(const CGPoint& point);
#endif

  ~Point() {}

#if defined(OS_WIN)
  POINT ToPOINT() const;
#elif defined(OS_MACOSX)
  CGPoint ToCGPoint() const;
#endif

  // Returns a string representation of point.
  std::string ToString() const;
};

#if !defined(COMPILER_MSVC)
extern template class PointBase<Point, int>;
#endif

}  // namespace gfx

#endif  // UI_GFX_POINT_H_

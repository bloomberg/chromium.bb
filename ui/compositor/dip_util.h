// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_DIP_UTIL_H_
#define UI_COMPOSITOR_DIP_UTIL_H_
#pragma once

#include "ui/compositor/compositor_export.h"
#include "base/basictypes.h"

namespace gfx {
class Point;
class Size;
class Rect;
}  // namespace gfx

namespace ui {
namespace test {

// A scoped object allows you to temporarily enable DIP
// in the unit tests.
class COMPOSITOR_EXPORT ScopedDIPEnablerForTest {
 public:
  ScopedDIPEnablerForTest();
  ~ScopedDIPEnablerForTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedDIPEnablerForTest);
};

}  // namespace test

class Layer;

// True if DIP is enabled.
COMPOSITOR_EXPORT bool IsDIPEnabled();

COMPOSITOR_EXPORT float GetDeviceScaleFactor(const Layer* layer);

// Utility functions that convert point/size/rect between
// DIP and pixel coordinates system.
COMPOSITOR_EXPORT gfx::Point ConvertPointToDIP(
    const Layer* layer,
    const gfx::Point& point_in_pixel);
COMPOSITOR_EXPORT gfx::Size ConvertSizeToDIP(
    const Layer* layer,
    const gfx::Size& size_in_pixel);
COMPOSITOR_EXPORT gfx::Rect ConvertRectToDIP(
    const Layer* layer,
    const gfx::Rect& rect_in_pixel);
COMPOSITOR_EXPORT gfx::Point ConvertPointToPixel(
    const Layer* layer,
    const gfx::Point& point_in_dip);
COMPOSITOR_EXPORT gfx::Size ConvertSizeToPixel(
    const Layer* layer,
    const gfx::Size& size_in_dip);
COMPOSITOR_EXPORT gfx::Rect ConvertRectToPixel(
    const Layer* layer,
    const gfx::Rect& rect_in_dip);

}  // namespace ui

#endif  // UI_COMPOSITOR_DIP_UTIL_H_

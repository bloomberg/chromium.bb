// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/dip_util.h"

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace gfx {

gfx::Point ConvertPointToDIP(float scale_factor,
                             const gfx::Point& point_in_pixel) {
  return gfx::ToFlooredPoint(
      gfx::ScalePoint(point_in_pixel, 1.0f / scale_factor));
}

gfx::PointF ConvertPointToDIP(float scale_factor,
                              const gfx::PointF& point_in_pixel) {
  return gfx::ScalePoint(point_in_pixel, 1.0f / scale_factor);
}

gfx::Size ConvertSizeToDIP(float scale_factor, const gfx::Size& size_in_pixel) {
  return gfx::ToFlooredSize(gfx::ScaleSize(size_in_pixel, 1.0f / scale_factor));
}

gfx::Rect ConvertRectToDIP(float scale_factor, const gfx::Rect& rect_in_pixel) {
  return gfx::ToFlooredRectDeprecated(
      gfx::ScaleRect(rect_in_pixel, 1.0f / scale_factor));
}

gfx::Point ConvertPointToPixel(float scale_factor,
                               const gfx::Point& point_in_dip) {
  return gfx::ToFlooredPoint(gfx::ScalePoint(point_in_dip, scale_factor));
}

gfx::Size ConvertSizeToPixel(float scale_factor, const gfx::Size& size_in_dip) {
  return gfx::ToFlooredSize(gfx::ScaleSize(size_in_dip, scale_factor));
}

gfx::Rect ConvertRectToPixel(float scale_factor, const gfx::Rect& rect_in_dip) {
  // Use ToEnclosingRect() to ensure we paint all the possible pixels
  // touched. ToEnclosingRect() floors the origin, and ceils the max
  // coordinate. To do otherwise (such as flooring the size) potentially
  // results in rounding down and not drawing all the pixels that are
  // touched.
  return gfx::ToEnclosingRect(
      gfx::RectF(gfx::ScalePoint(rect_in_dip.origin(), scale_factor),
                 gfx::ScaleSize(rect_in_dip.size(), scale_factor)));
}

}  // namespace gfx

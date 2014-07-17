// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/dip_util.h"

#include "base/command_line.h"
#include "cc/layers/layer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"

namespace ui {

float GetDeviceScaleFactor(const Layer* layer) {
  return layer->device_scale_factor();
}

gfx::Point ConvertPointToDIP(const Layer* layer,
                             const gfx::Point& point_in_pixel) {
  return gfx::ToFlooredPoint(
      gfx::ScalePoint(point_in_pixel, 1.0f / GetDeviceScaleFactor(layer)));
}

gfx::PointF ConvertPointToDIP(const Layer* layer,
                              const gfx::PointF& point_in_pixel) {
  return gfx::ScalePoint(point_in_pixel, 1.0f / GetDeviceScaleFactor(layer));
}

gfx::Size ConvertSizeToDIP(const Layer* layer,
                           const gfx::Size& size_in_pixel) {
  return gfx::ToFlooredSize(
      gfx::ScaleSize(size_in_pixel, 1.0f / GetDeviceScaleFactor(layer)));
}

gfx::Rect ConvertRectToDIP(const Layer* layer,
                           const gfx::Rect& rect_in_pixel) {
  float scale = 1.0f / GetDeviceScaleFactor(layer);
  return gfx::ToFlooredRectDeprecated(gfx::ScaleRect(rect_in_pixel, scale));
}

gfx::Point ConvertPointToPixel(const Layer* layer,
                               const gfx::Point& point_in_dip) {
  return gfx::ToFlooredPoint(
      gfx::ScalePoint(point_in_dip, GetDeviceScaleFactor(layer)));
}

gfx::Size ConvertSizeToPixel(const Layer* layer,
                             const gfx::Size& size_in_dip) {
  return gfx::ToFlooredSize(
      gfx::ScaleSize(size_in_dip, GetDeviceScaleFactor(layer)));
}

gfx::Rect ConvertRectToPixel(const Layer* layer,
                             const gfx::Rect& rect_in_dip) {
  float scale = GetDeviceScaleFactor(layer);
  // Use ToEnclosingRect() to ensure we paint all the possible pixels
  // touched. ToEnclosingRect() floors the origin, and ceils the max
  // coordinate. To do otherwise (such as flooring the size) potentially
  // results in rounding down and not drawing all the pixels that are
  // touched.
  return gfx::ToEnclosingRect(
      gfx::RectF(gfx::ScalePoint(rect_in_dip.origin(), scale),
                 gfx::ScaleSize(rect_in_dip.size(), scale)));
}

#if !defined(NDEBUG)
namespace {

void CheckSnapped(float snapped_position) {
  const float kEplison = 0.0001f;
  float diff = std::abs(snapped_position - static_cast<int>(snapped_position));
  DCHECK_LT(diff, kEplison);
}

}  // namespace
#endif

void SnapLayerToPhysicalPixelBoundary(ui::Layer* snapped_layer,
                                      ui::Layer* layer_to_snap) {
  DCHECK_NE(snapped_layer, layer_to_snap);
  DCHECK(snapped_layer);
  DCHECK(snapped_layer->Contains(layer_to_snap));

  gfx::Point view_offset_dips = layer_to_snap->GetTargetBounds().origin();
  ui::Layer::ConvertPointToLayer(
      layer_to_snap->parent(), snapped_layer, &view_offset_dips);
  gfx::PointF view_offset = view_offset_dips;

  float scale_factor = GetDeviceScaleFactor(layer_to_snap);
  view_offset.Scale(scale_factor);
  gfx::PointF view_offset_snapped(gfx::ToRoundedInt(view_offset.x()),
                                  gfx::ToRoundedInt(view_offset.y()));

  gfx::Vector2dF fudge = view_offset_snapped - view_offset;
  fudge.Scale(1.0 / scale_factor);
  layer_to_snap->SetSubpixelPositionOffset(fudge);
#if !defined(NDEBUG)
  gfx::Point p;
  Layer::ConvertPointToLayer(layer_to_snap->parent(), snapped_layer, &p);
  cc::Layer* cc_layer = layer_to_snap->cc_layer();
  gfx::PointF origin = cc_layer->position();
  CheckSnapped((p.x() + origin.x()) * scale_factor);
  CheckSnapped((p.y() + origin.y()) * scale_factor);
#endif
}

}  // namespace ui

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// BasePaintedLayerDelegate
//

BasePaintedLayerDelegate::BasePaintedLayerDelegate(SkColor color)
    : color_(color) {}

BasePaintedLayerDelegate::~BasePaintedLayerDelegate() {}

void BasePaintedLayerDelegate::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {}

void BasePaintedLayerDelegate::OnDeviceScaleFactorChanged(
    float device_scale_factor) {}

base::Closure BasePaintedLayerDelegate::PrepareForLayerBoundsChange() {
  return base::Closure();
}

////////////////////////////////////////////////////////////////////////////////
//
// CircleLayerDelegate
//

CircleLayerDelegate::CircleLayerDelegate(SkColor color, int radius)
    : BasePaintedLayerDelegate(color), radius_(radius) {}

CircleLayerDelegate::~CircleLayerDelegate() {}

gfx::PointF CircleLayerDelegate::GetCenterPoint() const {
  return gfx::PointF(radius_, radius_);
}

void CircleLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color());
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, gfx::Size(radius_, radius_));
  gfx::Canvas* canvas = recorder.canvas();

  canvas->DrawCircle(ToRoundedPoint(GetCenterPoint()), radius_, paint);
}

////////////////////////////////////////////////////////////////////////////////
//
// RectangleLayerDelegate
//

RectangleLayerDelegate::RectangleLayerDelegate(SkColor color, gfx::Size size)
    : BasePaintedLayerDelegate(color), size_(size) {}

RectangleLayerDelegate::~RectangleLayerDelegate() {}

gfx::PointF RectangleLayerDelegate::GetCenterPoint() const {
  return gfx::PointF(size_.width() / 2.0f, size_.height() / 2.0f);
}

void RectangleLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color());
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, size_);
  gfx::Canvas* canvas = recorder.canvas();
  canvas->DrawRect(gfx::Rect(size_), paint);
}

////////////////////////////////////////////////////////////////////////////////
//
// RoundedRectangleLayerDelegate
//

RoundedRectangleLayerDelegate::RoundedRectangleLayerDelegate(SkColor color,
                                                             gfx::Size size,
                                                             int corner_radius)
    : BasePaintedLayerDelegate(color),
      size_(size),
      corner_radius_(corner_radius) {}

RoundedRectangleLayerDelegate::~RoundedRectangleLayerDelegate() {}

gfx::PointF RoundedRectangleLayerDelegate::GetCenterPoint() const {
  return gfx::RectF(gfx::SizeF(size_)).CenterPoint();
}

void RoundedRectangleLayerDelegate::OnPaintLayer(
    const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color());
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, size_);
  gfx::Canvas* canvas = recorder.canvas();
  canvas->DrawRoundRect(gfx::Rect(size_), corner_radius_, paint);
}

}  // namespace views

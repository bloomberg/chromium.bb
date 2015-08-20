// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_delegate.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace views {

InkDropDelegate::InkDropDelegate(ui::Layer* layer,
                                 SkColor color,
                                 int circle_radius,
                                 int rounded_rect_corner_radius)
    : layer_(layer),
      color_(color),
      should_render_circle_(true),
      circle_radius_(circle_radius),
      rounded_rect_corner_radius_(rounded_rect_corner_radius) {}

InkDropDelegate::~InkDropDelegate() {}

void InkDropDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color_);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  gfx::Rect bounds = layer_->bounds();

  ui::PaintRecorder recorder(context, layer_->size());
  gfx::Canvas* canvas = recorder.canvas();
  if (should_render_circle()) {
    gfx::Point midpoint(bounds.width() * 0.5f, bounds.height() * 0.5f);
    canvas->DrawCircle(midpoint, circle_radius_, paint);
  } else {
    canvas->DrawRoundRect(bounds, rounded_rect_corner_radius_, paint);
  }
}

void InkDropDelegate::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {}

void InkDropDelegate::OnDeviceScaleFactorChanged(float device_scale_factor) {}

base::Closure InkDropDelegate::PrepareForLayerBoundsChange() {
  return base::Closure();
}

}  // namespace views

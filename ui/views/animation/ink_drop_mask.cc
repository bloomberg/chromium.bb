// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_mask.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace views {

// InkDropMask

InkDropMask::InkDropMask(const gfx::Rect& layer_bounds)
    : layer_(ui::LAYER_TEXTURED) {
  layer_.set_delegate(this);
  layer_.SetBounds(layer_bounds);
  layer_.SetFillsBoundsOpaquely(false);
  layer_.set_name("InkDropMaskLayer");
}

InkDropMask::~InkDropMask() {
  layer_.set_delegate(nullptr);
}

void InkDropMask::OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) {}

void InkDropMask::OnDeviceScaleFactorChanged(float device_scale_factor) {}

// RoundRectInkDropMask

RoundRectInkDropMask::RoundRectInkDropMask(const gfx::Rect& layer_bounds,
                                           const gfx::Rect& mask_bounds,
                                           int corner_radius)
    : InkDropMask(layer_bounds),
      mask_bounds_(mask_bounds),
      corner_radius_(corner_radius) {}

void RoundRectInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawRoundRect(mask_bounds_, corner_radius_, paint);
}

// CircleInkDropMask

CircleInkDropMask::CircleInkDropMask(const gfx::Rect& layer_bounds,
                                     const gfx::Point& mask_center,
                                     int mask_radius)
    : InkDropMask(layer_bounds),
      mask_center_(mask_center),
      mask_radius_(mask_radius) {}

void CircleInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);

  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawCircle(mask_center_, mask_radius_, paint);
}

}  // namespace views

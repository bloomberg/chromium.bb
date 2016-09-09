// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/md_slider.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/slider.h"

namespace views {

// Color of slider at the active and the disabled state, respectively.
const SkColor kActiveColor = SkColorSetARGB(0xFF, 0x42, 0x85, 0xF4);
const SkColor kDisabledColor = SkColorSetARGB(0x42, 0x00, 0x00, 0x00);

// The thickness of the slider.
const int kLineThickness = 2;

// The radius of the thumb of the slider.
const int kThumbRadius = 6;

// The stroke of the thumb when the slider is disabled.
const int kThumbStroke = 2;

MdSlider::MdSlider(SliderListener* listener)
    : Slider(listener), is_active_(true) {
  SchedulePaint();
}

MdSlider::~MdSlider() {}

void MdSlider::OnPaint(gfx::Canvas* canvas) {
  Slider::OnPaint(canvas);

  // Paint the slider.
  const int thumb_size = kThumbRadius * 2;
  const gfx::Rect content = GetContentsBounds();
  const int width = content.width() - thumb_size;
  const int full = GetAnimatingValue() * width;
  const int empty = width - full;
  const int y = content.height() / 2 - kLineThickness / 2;
  canvas->FillRect(gfx::Rect(content.x(), y, full, kLineThickness),
                   is_active_ ? kActiveColor : kDisabledColor);
  canvas->FillRect(
      gfx::Rect(content.x() + full + thumb_size, y, empty, kLineThickness),
      kDisabledColor);

  // Paint the thumb of the slider.
  SkPaint paint;
  paint.setColor(is_active_ ? kActiveColor : kDisabledColor);
  paint.setFlags(SkPaint::kAntiAlias_Flag);

  if (!is_active_) {
    paint.setStrokeWidth(kThumbStroke);
    paint.setStyle(SkPaint::kStroke_Style);
  }
  canvas->DrawCircle(
      gfx::Point(content.x() + full + kThumbRadius, content.height() / 2),
      is_active_ ? kThumbRadius : (kThumbRadius - kThumbStroke / 2), paint);
}

const char* MdSlider::GetClassName() const {
  return "MdSlider";
}

void MdSlider::UpdateState(bool control_on) {
  is_active_ = control_on;
  SchedulePaint();
}

int MdSlider::GetThumbWidth() {
  return kThumbRadius * 2;
}
}  // namespace views

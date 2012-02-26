// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace views {

Slider::Slider(SliderListener* listener, Orientation orientation)
    : listener_(listener),
      orientation_(orientation),
      value_(0.f) {
}

Slider::~Slider() {
}

void Slider::SetValue(float value) {
  if (value < 0.0)
   value = 0.0;
  else if (value > 1.0)
    value = 1.0;
  if (value_ == value)
    return;
  float old_value = value_;
  value_ = value;
  if (listener_)
    listener_->SliderValueChanged(this, value_, old_value);
  SchedulePaint();
}

gfx::Size Slider::GetPreferredSize() {
  const int kSizeMajor = 200;
  const int kSizeMinor = 40;

  if (orientation_ == HORIZONTAL)
    return gfx::Size(kSizeMajor, kSizeMinor);
  return gfx::Size(kSizeMinor, kSizeMajor);
}

void Slider::OnPaint(gfx::Canvas* canvas) {
  // TODO(sad): The painting code should use NativeTheme for various platforms.
  const int kButtonMajor = 15;
  const int kButtonMinor = 4;
  const int kLineThickness = 5;
  const SkColor kFullColor = SkColorSetARGB(125, 0, 0, 0);
  const SkColor kEmptyColor = SkColorSetARGB(50, 0, 0, 0);
  const SkColor kButtonColor = SK_ColorBLACK;

  if (orientation_ == HORIZONTAL) {
    int w = width() - kButtonMinor;
    int full = value_ * w;
    int empty = w - full;
    int y = height() / 2 - kLineThickness / 2;
    canvas->FillRect(gfx::Rect(kButtonMinor / 2, y,
                               std::max(0, full - kButtonMinor / 2),
                               kLineThickness),
                     kFullColor);
    canvas->FillRect(gfx::Rect(full + kButtonMinor / 2, y,
                               std::max(0, empty), kLineThickness),
                     kEmptyColor);
    canvas->FillRect(gfx::Rect(std::max(0, full - kButtonMinor / 2),
                               y - (kButtonMajor - kLineThickness) / 2,
                               kButtonMinor,
                               kButtonMajor),
                     kButtonColor);
  } else {
    int h = height() - kButtonMinor;
    int full = value_ * h;
    int empty = h - full;
    int x = width() / 2 - kLineThickness / 2;
    canvas->FillRect(gfx::Rect(x, kButtonMinor / 2,
                               kLineThickness,
                               std::max(0, empty - kButtonMinor / 2)),
                     kEmptyColor);
    canvas->FillRect(gfx::Rect(x, empty + kButtonMinor / 2,
                               kLineThickness, full),
                     kFullColor);
    canvas->FillRect(gfx::Rect(x - (kButtonMajor - kLineThickness) / 2,
                               std::max(0, empty - kButtonMinor / 2),
                               kButtonMajor,
                               kButtonMinor),
                     kButtonColor);
  }
}

bool Slider::OnMousePressed(const views::MouseEvent& event) {
  return OnMouseDragged(event);
}

bool Slider::OnMouseDragged(const views::MouseEvent& event) {
  if (orientation_ == HORIZONTAL)
    SetValue(static_cast<float>(event.x()) / width());
  else
    SetValue(1.0f - static_cast<float>(event.y()) / height());
  return true;
}

}  // namespace views

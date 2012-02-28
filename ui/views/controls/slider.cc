// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace views {

Slider::Slider(SliderListener* listener, Orientation orientation)
    : listener_(listener),
      orientation_(orientation),
      value_(0.f) {
  EnableCanvasFlippingForRTLUI(true);
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
  const int kButtonRadius = 5;
  const int kLineThickness = 5;
  const SkColor kFullColor = SkColorSetARGB(125, 0, 0, 0);
  const SkColor kEmptyColor = SkColorSetARGB(50, 0, 0, 0);
  const SkColor kButtonColor = SK_ColorBLACK;

  gfx::Rect content = GetContentsBounds();

  int button_cx = 0, button_cy = 0;
  if (orientation_ == HORIZONTAL) {
    int w = content.width() - kButtonRadius * 2;
    int full = value_ * w;
    int empty = w - full;
    int y = content.height() / 2 - kLineThickness / 2;
    canvas->FillRect(gfx::Rect(content.x() + kButtonRadius, y,
                               std::max(0, full - kButtonRadius),
                               kLineThickness),
                     kFullColor);
    canvas->FillRect(gfx::Rect(content.x() + full + 2 * kButtonRadius, y,
                          std::max(0, empty - kButtonRadius), kLineThickness),
                     kEmptyColor);

    button_cx = content.x() + full + kButtonRadius;
    button_cy = y + kLineThickness / 2;
  } else {
    int h = content.height() - kButtonRadius * 2;
    int full = value_ * h;
    int empty = h - full;
    int x = content.width() / 2 - kLineThickness / 2;
    canvas->FillRect(gfx::Rect(x, content.y() + kButtonRadius,
                               kLineThickness,
                               std::max(0, empty - kButtonRadius)),
                     kEmptyColor);
    canvas->FillRect(gfx::Rect(x, content.y() + empty + 2 * kButtonRadius,
                               kLineThickness, full),
                     kFullColor);

    button_cx = x + kLineThickness / 2;
    button_cy = content.y() + empty + kButtonRadius;
  }

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setColor(kButtonColor);
  canvas->GetSkCanvas()->drawCircle(button_cx, button_cy, kButtonRadius, paint);
  View::OnPaint(canvas);
}

bool Slider::OnMousePressed(const views::MouseEvent& event) {
  return OnMouseDragged(event);
}

bool Slider::OnMouseDragged(const views::MouseEvent& event) {
  gfx::Insets inset = GetInsets();
  if (orientation_ == HORIZONTAL) {
    int amount = base::i18n::IsRTL() ? width() - inset.left() - event.x() :
                                       event.x() - inset.left();
    SetValue(static_cast<float>(amount) / (width() - inset.width()));
  } else {
    SetValue(1.0f - static_cast<float>(event.y()) / height());
  }
  return true;
}

}  // namespace views

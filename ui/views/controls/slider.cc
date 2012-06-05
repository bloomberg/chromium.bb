// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

namespace {
const int kSlideValueChangeDurationMS = 150;
}

namespace views {

Slider::Slider(SliderListener* listener, Orientation orientation)
    : listener_(listener),
      orientation_(orientation),
      value_(0.f),
      keyboard_increment_(0.1f),
      animating_value_(0.f),
      value_is_valid_(false),
      accessibility_events_enabled_(true),
      focus_border_color_(0) {
  EnableCanvasFlippingForRTLUI(true);
  set_focusable(true);
}

Slider::~Slider() {
}

void Slider::SetValue(float value) {
  SetValueInternal(value, VALUE_CHANGED_BY_API);
}

void Slider::SetKeyboardIncrement(float increment) {
  keyboard_increment_ = increment;
}

void Slider::SetValueInternal(float value, SliderChangeReason reason) {
  bool old_value_valid = value_is_valid_;

  value_is_valid_ = true;
  if (value < 0.0)
   value = 0.0;
  else if (value > 1.0)
    value = 1.0;
  if (value_ == value)
    return;
  float old_value = value_;
  value_ = value;
  if (listener_)
    listener_->SliderValueChanged(this, value_, old_value, reason);

  if (old_value_valid && MessageLoop::current()) {
    // Do not animate when setting the value of the slider for the first time.
    // There is no message-loop when running tests. So we cannot animate then.
    animating_value_ = old_value;
    move_animation_.reset(new ui::SlideAnimation(this));
    move_animation_->SetSlideDuration(kSlideValueChangeDurationMS);
    move_animation_->Show();
    AnimationProgressed(move_animation_.get());
  } else {
    SchedulePaint();
  }
  if (accessibility_events_enabled_ && GetWidget()) {
    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_VALUE_CHANGED, true);
  }
}

void Slider::MoveButtonTo(const gfx::Point& point) {
  gfx::Insets inset = GetInsets();
  if (orientation_ == HORIZONTAL) {
    int amount = base::i18n::IsRTL() ? width() - inset.left() - point.x() :
                                       point.x() - inset.left();
    SetValueInternal(static_cast<float>(amount) / (width() - inset.width()),
                     VALUE_CHANGED_BY_USER);
  } else {
    SetValueInternal(1.0f - static_cast<float>(point.y()) / height(),
                     VALUE_CHANGED_BY_USER);
  }
}

void Slider::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

gfx::Size Slider::GetPreferredSize() {
  const int kSizeMajor = 200;
  const int kSizeMinor = 40;

  if (orientation_ == HORIZONTAL)
    return gfx::Size(std::max(width(), kSizeMajor), kSizeMinor);
  return gfx::Size(kSizeMinor, std::max(height(), kSizeMajor));
}

void Slider::OnPaint(gfx::Canvas* canvas) {
  // TODO(sad): The painting code should use NativeTheme for various platforms.
  const int kButtonRadius = 5;
  const int kLineThickness = 5;
  const SkColor kFullColor = SkColorSetARGB(125, 0, 0, 0);
  const SkColor kEmptyColor = SkColorSetARGB(50, 0, 0, 0);
  const SkColor kButtonColor = SK_ColorBLACK;

  gfx::Rect content = GetContentsBounds();
  float value = move_animation_.get() && move_animation_->is_animating() ?
      animating_value_ : value_;

  int button_cx = 0, button_cy = 0;
  if (orientation_ == HORIZONTAL) {
    int w = content.width() - kButtonRadius * 2;
    int full = value * w;
    int empty = w - full;
    int y = content.height() / 2 - kLineThickness / 2;
    canvas->FillRect(gfx::Rect(content.x() + kButtonRadius, y,
                               full, kLineThickness),
                     kFullColor);
    canvas->FillRect(gfx::Rect(content.x() + full + 2 * kButtonRadius, y,
                          std::max(0, empty - kButtonRadius), kLineThickness),
                     kEmptyColor);

    button_cx = content.x() + full + kButtonRadius;
    button_cy = y + kLineThickness / 2;
  } else {
    int h = content.height() - kButtonRadius * 2;
    int full = value * h;
    int empty = h - full;
    int x = content.width() / 2 - kLineThickness / 2;
    canvas->FillRect(gfx::Rect(x, content.y() + kButtonRadius,
                               kLineThickness, empty),
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
  canvas->sk_canvas()->drawCircle(button_cx, button_cy, kButtonRadius, paint);
  View::OnPaint(canvas);
}

bool Slider::OnMousePressed(const views::MouseEvent& event) {
  if (listener_)
    listener_->SliderDragStarted(this);
  MoveButtonTo(event.location());
  return true;
}

bool Slider::OnMouseDragged(const views::MouseEvent& event) {
  MoveButtonTo(event.location());
  return true;
}

void Slider::OnMouseReleased(const views::MouseEvent& event) {
  if (listener_)
    listener_->SliderDragEnded(this);
}

bool Slider::OnKeyPressed(const views::KeyEvent& event) {
  if (orientation_ == HORIZONTAL) {
    if (event.key_code() == ui::VKEY_LEFT) {
      SetValueInternal(value_ - keyboard_increment_, VALUE_CHANGED_BY_USER);
      return true;
    } else if (event.key_code() == ui::VKEY_RIGHT) {
      SetValueInternal(value_ + keyboard_increment_, VALUE_CHANGED_BY_USER);
      return true;
    }
  } else {
    if (event.key_code() == ui::VKEY_DOWN) {
      SetValueInternal(value_ - keyboard_increment_, VALUE_CHANGED_BY_USER);
      return true;
    } else if (event.key_code() == ui::VKEY_UP) {
      SetValueInternal(value_ + keyboard_increment_, VALUE_CHANGED_BY_USER);
      return true;
    }
  }
  return false;
}

ui::GestureStatus Slider::OnGestureEvent(const views::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_SCROLL_UPDATE ||
      event.type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event.type() == ui::ET_GESTURE_SCROLL_END ||
      event.type() == ui::ET_GESTURE_TAP_DOWN) {
    MoveButtonTo(event.location());
    return ui::GESTURE_STATUS_CONSUMED;
  }
  return ui::GESTURE_STATUS_UNKNOWN;
}

void Slider::AnimationProgressed(const ui::Animation* animation) {
  animating_value_ = animation->CurrentValueBetween(animating_value_, value_);
  SchedulePaint();
}

void Slider::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_SLIDER;
  state->name = accessible_name_;
  state->value = UTF8ToUTF16(
      base::StringPrintf("%d%%", (int)(value_ * 100 + 0.5)));
}

void Slider::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (!focus_border_color_) {
    View::OnPaintFocusBorder(canvas);
  } else if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(1, 1, width() - 3, height() - 3),
                     focus_border_color_);
  }
}

}  // namespace views

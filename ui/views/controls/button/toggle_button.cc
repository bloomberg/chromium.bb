// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/border.h"

namespace views {

namespace {

// Constants are measured in dip.
const int kTrackHeight = 14;
const int kTrackWidth = 36;
// Margins from edge of track to edge of view.
const int kTrackVerticalMargin = 4;
const int kTrackHorizontalMargin = 1;
// Margin from edge of thumb to closest three edges of view.
const int kThumbMargin = 1;

}  // namespace

ToggleButton::ToggleButton(ButtonListener* listener)
    : CustomButton(listener), is_on_(false), slide_animation_(this) {
  slide_animation_.SetSlideDuration(80 /* ms */);
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  SetBorder(Border::CreateEmptyBorder(
      gfx::Insets(kTrackVerticalMargin, kTrackHorizontalMargin)));
}

ToggleButton::~ToggleButton() {}

void ToggleButton::SetIsOn(bool is_on, bool animate) {
  if (is_on_ == is_on)
    return;

  is_on_ = is_on;
  if (!animate)
    slide_animation_.Reset(is_on_ ? 1.0 : 0.0);
  else if (is_on_)
    slide_animation_.Show();
  else
    slide_animation_.Hide();
}

gfx::Size ToggleButton::GetPreferredSize() const {
  gfx::Rect rect(0, 0, kTrackWidth, kTrackHeight);
  if (border())
    rect.Inset(-border()->GetInsets());
  return rect.size();
}

void ToggleButton::OnPaint(gfx::Canvas* canvas) {
  SkAlpha blend =
      static_cast<SkAlpha>(SK_AlphaOPAQUE * slide_animation_.GetCurrentValue());

  gfx::RectF track_rect(GetContentsBounds());
  SkPaint paint;
  paint.setAntiAlias(true);

  // Track.
  const SkColor track_on_color =
      SkColorSetA(GetNativeTheme()->GetSystemColor(
                      ui::NativeTheme::kColorId_CallToActionColor),
                  0xFF / 2);
  // TODO(estade): get this color from the theme.
  const SkColor track_off_color = SkColorSetA(SK_ColorBLACK, 0x61);
  paint.setColor(
      color_utils::AlphaBlend(track_on_color, track_off_color, blend));
  canvas->DrawRoundRect(track_rect, track_rect.height() / 2, paint);

  // Thumb.
  const SkColor thumb_on_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_CallToActionColor);
  // TODO(estade): get this color from the theme.
  const SkColor thumb_off_color = SkColorSetRGB(0xFA, 0xFA, 0xFA);
  paint.setColor(
      color_utils::AlphaBlend(thumb_on_color, thumb_off_color, blend));
  gfx::Rect thumb_bounds = GetLocalBounds();
  thumb_bounds.Inset(gfx::Insets(kThumbMargin));
  thumb_bounds.set_x(thumb_bounds.x() +
                     slide_animation_.GetCurrentValue() *
                         (thumb_bounds.width() - thumb_bounds.height()));
  thumb_bounds.set_width(thumb_bounds.height());
  thumb_bounds.set_x(GetMirroredXForRect(thumb_bounds));
  canvas->DrawCircle(gfx::RectF(thumb_bounds).CenterPoint(),
                     thumb_bounds.height() / 2.f, paint);
  // TODO(estade): add a shadow to the thumb.
}

void ToggleButton::NotifyClick(const ui::Event& event) {
  SetIsOn(!is_on(), true);
  CustomButton::NotifyClick(event);
}

void ToggleButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  SchedulePaint();
}

void ToggleButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &slide_animation_)
    SchedulePaint();
  else
    CustomButton::AnimationProgressed(animation);
}

}  // namespace views

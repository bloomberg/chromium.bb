// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/border.h"

namespace views {

namespace {

// Constants are measured in dip.
const int kTrackHeight = 14;
const int kTrackWidth = 36;
// Margins from edge of track to edge of view.
const int kTrackVerticalMargin = 6;
const int kTrackHorizontalMargin = 2;
// Margin from edge of thumb to closest edge of view. Note that the thumb
// margins must be sufficiently large to allow space for the shadow.
const int kThumbHorizontalMargin = 2;
// Margin from top/bottom edge of thumb to top/bottom edge of view.
const int kThumbVerticalMargin = 3;

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

  // Track.
  gfx::RectF track_rect(GetContentsBounds());
  SkPaint track_paint;
  track_paint.setAntiAlias(true);
  const SkColor track_on_color =
      SkColorSetA(GetNativeTheme()->GetSystemColor(
                      ui::NativeTheme::kColorId_CallToActionColor),
                  0xFF / 2);
  // TODO(estade): get this color from the theme.
  const SkColor track_off_color = SkColorSetA(SK_ColorBLACK, 0x61);
  track_paint.setColor(
      color_utils::AlphaBlend(track_on_color, track_off_color, blend));
  canvas->DrawRoundRect(track_rect, track_rect.height() / 2, track_paint);

  // Thumb.
  gfx::Rect thumb_bounds = GetLocalBounds();
  thumb_bounds.Inset(gfx::Insets(kThumbVerticalMargin, kThumbHorizontalMargin));
  thumb_bounds.set_x(thumb_bounds.x() +
                     slide_animation_.GetCurrentValue() *
                         (thumb_bounds.width() - thumb_bounds.height()));
  thumb_bounds.set_width(thumb_bounds.height());
  thumb_bounds.set_x(GetMirroredXForRect(thumb_bounds));
  SkPaint thumb_paint;
  std::vector<gfx::ShadowValue> shadows;
  shadows.emplace_back(gfx::Vector2d(0, 1), 5.f,
                       SkColorSetA(SK_ColorBLACK, 0x99));
  thumb_paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadows));
  thumb_paint.setStyle(SkPaint::kStrokeAndFill_Style);
  thumb_paint.setAntiAlias(true);
  const SkColor thumb_on_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_CallToActionColor);
  // TODO(estade): get this color from the theme.
  const SkColor thumb_off_color = SkColorSetRGB(0xFA, 0xFA, 0xFA);
  thumb_paint.setColor(
      color_utils::AlphaBlend(thumb_on_color, thumb_off_color, blend));
  canvas->DrawCircle(gfx::RectF(thumb_bounds).CenterPoint(),
                     thumb_bounds.height() / 2.f, thumb_paint);
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

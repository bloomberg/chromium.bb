// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/border.h"

namespace views {

namespace {

// Constants are measured in dip.
const int kTrackHeight = 12;
const int kTrackWidth = 28;
// Margins from edge of track to edge of view.
const int kTrackVerticalMargin = 5;
const int kTrackHorizontalMargin = 6;
// Margin from edge of thumb to closest edge of view. Note that the thumb
// margins must be sufficiently large to allow space for the shadow.
const int kThumbHorizontalMargin = 4;
// Margin from top/bottom edge of thumb to top/bottom edge of view.
const int kThumbVerticalMargin = 3;

// TODO(estade): get the base color (black) from the theme?
const SkColor kTrackOffColor =
    SkColorSetA(SK_ColorBLACK, gfx::kDisabledControlAlpha);

}  // namespace

ToggleButton::ToggleButton(ButtonListener* listener)
    : CustomButton(listener), is_on_(false), slide_animation_(this) {
  slide_animation_.SetSlideDuration(80 /* ms */);
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  SetBorder(Border::CreateEmptyBorder(
      gfx::Insets(kTrackVerticalMargin, kTrackHorizontalMargin)));
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
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
                      ui::NativeTheme::kColorId_ProminentButtonColor),
                  0xFF / 2);
  track_paint.setColor(
      color_utils::AlphaBlend(track_on_color, kTrackOffColor, blend));
  canvas->DrawRoundRect(track_rect, track_rect.height() / 2, track_paint);

  // Thumb.
  gfx::Rect thumb_bounds = GetThumbBounds();
  SkPaint thumb_paint;
  std::vector<gfx::ShadowValue> shadows;
  shadows.emplace_back(gfx::Vector2d(0, 1), 4.f,
                       SkColorSetA(SK_ColorBLACK, 0x99));
  thumb_paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadows));
  thumb_paint.setStyle(SkPaint::kFill_Style);
  thumb_paint.setAntiAlias(true);
  const SkColor thumb_on_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
  // TODO(estade): get this color from the theme?
  const SkColor thumb_off_color = SK_ColorWHITE;
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

std::unique_ptr<InkDropRipple> ToggleButton::CreateInkDropRipple() const {
  return CreateDefaultInkDropRipple(GetThumbBounds().CenterPoint());
}

SkColor ToggleButton::GetInkDropBaseColor() const {
  return is_on()
             ? GetNativeTheme()->GetSystemColor(
                   ui::NativeTheme::kColorId_ProminentButtonColor)
             : kTrackOffColor;
}

bool ToggleButton::ShouldShowInkDropHighlight() const {
  return false;
}

void ToggleButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &slide_animation_)
    SchedulePaint();
  else
    CustomButton::AnimationProgressed(animation);
}

gfx::Rect ToggleButton::GetThumbBounds() const {
  gfx::Rect thumb_bounds = GetLocalBounds();
  thumb_bounds.Inset(gfx::Insets(kThumbVerticalMargin, kThumbHorizontalMargin));
  thumb_bounds.set_x(thumb_bounds.x() +
                     slide_animation_.GetCurrentValue() *
                         (thumb_bounds.width() - thumb_bounds.height()));
  // The thumb is a circle, so the width should match the height.
  thumb_bounds.set_width(thumb_bounds.height());
  thumb_bounds.set_x(GetMirroredXForRect(thumb_bounds));
  return thumb_bounds;
}

}  // namespace views

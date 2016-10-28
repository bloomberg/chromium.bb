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

// Class representing the thumb. When the thumb is clicked it is separated into
// its own layer and the ink drop layer is made a child of the thumb layer
// allowing the two to animate in sync.
class ToggleButton::ThumbView : public views::View {
 public:
  ThumbView() : color_ratio_(0.) {}
  ~ThumbView() override {}

  void AddInkDropLayer(ui::Layer* ink_drop_layer) {
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->Add(ink_drop_layer);
  }

  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
    layer()->Remove(ink_drop_layer);
    SetPaintToLayer(false);
  }

  void Update(const gfx::Rect& bounds, double color_ratio) {
    SetBoundsRect(bounds);
    color_ratio_ = color_ratio;
    SchedulePaint();
  }

 private:
  // views::View:
  const char* GetClassName() const override {
    return "ToggleButton::ThumbView";
  }

  void OnPaint(gfx::Canvas* canvas) override {
    std::vector<gfx::ShadowValue> shadows;
    shadows.emplace_back(gfx::Vector2d(0, 1), 4.f,
                         SkColorSetA(SK_ColorBLACK, 0x99));
    SkPaint thumb_paint;
    thumb_paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadows));
    thumb_paint.setStyle(SkPaint::kFill_Style);
    thumb_paint.setAntiAlias(true);
    const SkColor thumb_on_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    // TODO(estade): get this color from the theme?
    const SkColor thumb_off_color = SK_ColorWHITE;
    const SkAlpha blend = static_cast<SkAlpha>(SK_AlphaOPAQUE * color_ratio_);
    thumb_paint.setColor(
        color_utils::AlphaBlend(thumb_on_color, thumb_off_color, blend));
    gfx::Rect thumb_bounds = GetLocalBounds();
    thumb_bounds.Inset(gfx::Insets(kThumbVerticalMargin));
    canvas->DrawCircle(gfx::RectF(thumb_bounds).CenterPoint(),
                       thumb_bounds.height() / 2.f, thumb_paint);
  }

  // Color ratio between 0 and 1 that controls the thumb color.
  double color_ratio_;

  DISALLOW_COPY_AND_ASSIGN(ThumbView);
};

// static
const char ToggleButton::kViewClassName[] = "ToggleButton";

ToggleButton::ToggleButton(ButtonListener* listener)
    : CustomButton(listener),
      is_on_(false),
      slide_animation_(this),
      thumb_view_(new ToggleButton::ThumbView()) {
  slide_animation_.SetSlideDuration(80 /* ms */);
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  SetBorder(Border::CreateEmptyBorder(
      gfx::Insets(kTrackVerticalMargin, kTrackHorizontalMargin)));
  AddChildView(thumb_view_.get());
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
}

ToggleButton::~ToggleButton() {
  // Destroying ink drop early allows ink drop layer to be properly removed,
  SetInkDropMode(InkDropMode::OFF);
}

void ToggleButton::SetIsOn(bool is_on, bool animate) {
  if (is_on_ == is_on)
    return;

  is_on_ = is_on;
  if (!animate) {
    slide_animation_.Reset(is_on_ ? 1.0 : 0.0);
    UpdateThumb();
    SchedulePaint();
  } else if (is_on_) {
    slide_animation_.Show();
  } else {
    slide_animation_.Hide();
  }
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

void ToggleButton::UpdateThumb() {
  gfx::Rect thumb_bounds = GetThumbBounds();
  thumb_bounds.Inset(gfx::Insets(-kThumbVerticalMargin));
  thumb_view_->Update(thumb_bounds, slide_animation_.GetCurrentValue());
}

gfx::Size ToggleButton::GetPreferredSize() const {
  gfx::Rect rect(0, 0, kTrackWidth, kTrackHeight);
  if (border())
    rect.Inset(-border()->GetInsets());
  return rect.size();
}

const char* ToggleButton::GetClassName() const {
  return kViewClassName;
}

void ToggleButton::OnPaint(gfx::Canvas* canvas) {
  // Paint the toggle track.
  gfx::RectF track_rect(GetContentsBounds());
  SkPaint track_paint;
  track_paint.setAntiAlias(true);
  const SkColor track_on_color =
      SkColorSetA(GetNativeTheme()->GetSystemColor(
                      ui::NativeTheme::kColorId_ProminentButtonColor),
                  0xFF / 2);
  const double color_ratio = slide_animation_.GetCurrentValue();
  track_paint.setColor(color_utils::AlphaBlend(
      track_on_color, kTrackOffColor,
      static_cast<SkAlpha>(SK_AlphaOPAQUE * color_ratio)));
  canvas->DrawRoundRect(track_rect, track_rect.height() / 2, track_paint);
}

void ToggleButton::NotifyClick(const ui::Event& event) {
  SetIsOn(!is_on(), true);
  CustomButton::NotifyClick(event);
}

void ToggleButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateThumb();
}

void ToggleButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  SchedulePaint();
}

void ToggleButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  thumb_view_->AddInkDropLayer(ink_drop_layer);
  UpdateThumb();
  SchedulePaint();
}

void ToggleButton::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  thumb_view_->RemoveInkDropLayer(ink_drop_layer);
  SchedulePaint();
}

std::unique_ptr<InkDropRipple> ToggleButton::CreateInkDropRipple() const {
  const int radius = (kTrackHeight + kTrackVerticalMargin * 2) / 2;
  return CreateDefaultInkDropRipple(gfx::Point(radius, radius));
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
  if (animation == &slide_animation_) {
    // TODO(varkha, estade): The thumb is using its own view. Investigate if
    // repainting in every animation step to update colors could be avoided.
    UpdateThumb();
    SchedulePaint();
    return;
  }
  CustomButton::AnimationProgressed(animation);
}

}  // namespace views

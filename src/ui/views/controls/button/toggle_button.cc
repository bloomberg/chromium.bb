// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Constants are measured in dip.
constexpr gfx::Size kTrackSize = gfx::Size(28, 12);
// Margins from edge of track to edge of view.
constexpr int kTrackVerticalMargin = 5;
constexpr int kTrackHorizontalMargin = 6;
// Inset from the rounded edge of the thumb to the rounded edge of the track.
constexpr int kThumbInset = 2;

}  // namespace

class ToggleButton::FocusRingHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const ToggleButton*>(view)->GetFocusRingPath();
  }
};

// Class representing the thumb (the circle that slides horizontally).
class ToggleButton::ThumbView : public View {
 public:
  ThumbView() {
    // Make the thumb behave as part of the parent for event handling.
    SetCanProcessEventsWithinSubtree(false);
  }
  ThumbView(const ThumbView&) = delete;
  ThumbView& operator=(const ThumbView&) = delete;
  ~ThumbView() override = default;

  void Update(const gfx::Rect& bounds, float color_ratio) {
    SetBoundsRect(bounds);
    color_ratio_ = color_ratio;
    SchedulePaint();
  }

  // Returns the extra space needed to draw the shadows around the thumb. Since
  // the extra space is around the thumb, the insets will be negative.
  static gfx::Insets GetShadowOutsets() {
    return gfx::Insets(-kShadowBlur) +
           gfx::Vector2d(kShadowOffsetX, kShadowOffsetY);
  }

  void SetThumbColor(bool is_on, const absl::optional<SkColor>& thumb_color) {
    (is_on ? thumb_on_color_ : thumb_off_color_) = thumb_color;
  }

  absl::optional<SkColor> GetThumbColor(bool is_on) const {
    return is_on ? thumb_on_color_ : thumb_off_color_;
  }

 private:
  static constexpr int kShadowOffsetX = 0;
  static constexpr int kShadowOffsetY = 1;
  static constexpr int kShadowBlur = 2;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    const float dsf = canvas->UndoDeviceScaleFactor();
    const ui::ColorProvider* color_provider = GetColorProvider();
    std::vector<gfx::ShadowValue> shadows;
    gfx::ShadowValue shadow(
        gfx::Vector2d(kShadowOffsetX, kShadowOffsetY), 2 * kShadowBlur,
        color_provider->GetColor(ui::kColorToggleButtonShadow));
    shadows.push_back(shadow.Scale(dsf));
    cc::PaintFlags thumb_flags;
    thumb_flags.setLooper(gfx::CreateShadowDrawLooper(shadows));
    thumb_flags.setAntiAlias(true);
    const SkColor thumb_on_color = thumb_on_color_.value_or(
        color_provider->GetColor(ui::kColorToggleButtonThumbOn));
    const SkColor thumb_off_color = thumb_off_color_.value_or(
        color_provider->GetColor(ui::kColorToggleButtonThumbOff));
    thumb_flags.setColor(
        color_utils::AlphaBlend(thumb_on_color, thumb_off_color, color_ratio_));

    // We want the circle to have an integer pixel diameter and to be aligned
    // with pixel boundaries, so we scale dip bounds to pixel bounds and round.
    gfx::RectF thumb_bounds(GetLocalBounds());
    thumb_bounds.Inset(-gfx::InsetsF(GetShadowOutsets()));
    thumb_bounds.Inset(0.5f);
    thumb_bounds.Scale(dsf);
    thumb_bounds = gfx::RectF(gfx::ToEnclosingRect(thumb_bounds));
    canvas->DrawCircle(thumb_bounds.CenterPoint(), thumb_bounds.height() / 2.f,
                       thumb_flags);
  }

  // Colors used for the thumb.
  absl::optional<SkColor> thumb_on_color_;
  absl::optional<SkColor> thumb_off_color_;

  // Color ratio between 0 and 1 that controls the thumb color.
  float color_ratio_ = 0.0f;
};

ToggleButton::ToggleButton(PressedCallback callback)
    : Button(std::move(callback)) {
  slide_animation_.SetSlideDuration(base::Milliseconds(80));
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  thumb_view_ = AddChildView(std::make_unique<ThumbView>());
  InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  // Do not set a clip, allow the ink drop to burst out.
  // TODO(pbos): Consider an explicit InkDrop API to not use a clip rect / mask.
  views::InstallEmptyHighlightPathGenerator(this);
  // InkDrop event triggering is handled in NotifyClick().
  SetHasInkDropActionOnClick(false);
  InkDrop::UseInkDropForSquareRipple(InkDrop::Get(this),
                                     /*highlight_on_hover=*/false);
  InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](ToggleButton* host) {
        gfx::Rect rect = host->thumb_view_->GetLocalBounds();
        rect.Inset(-ThumbView::GetShadowOutsets());
        return InkDrop::Get(host)->CreateSquareRipple(rect.CenterPoint());
      },
      this));
  InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](ToggleButton* host) {
        return host->GetTrackColor(host->GetIsOn() || host->HasFocus());
      },
      this));

  // Even though ToggleButton doesn't paint anything, declare us as flipped in
  // RTL mode so that FocusRing correctly flips as well.
  SetFlipCanvasOnPaintForRTLUI(true);
  SetInstallFocusRingOnFocus(true);
  FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<FocusRingHighlightPathGenerator>());
}

ToggleButton::~ToggleButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerBeneathView(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void ToggleButton::AnimateIsOn(bool is_on) {
  if (GetIsOn() == is_on)
    return;
  if (is_on)
    slide_animation_.Show();
  else
    slide_animation_.Hide();
  OnPropertyChanged(&slide_animation_, kPropertyEffectsNone);
}

void ToggleButton::SetIsOn(bool is_on) {
  if ((GetIsOn() == is_on) && !slide_animation_.is_animating())
    return;
  slide_animation_.Reset(is_on ? 1.0 : 0.0);
  UpdateThumb();
  OnPropertyChanged(&slide_animation_, kPropertyEffectsPaint);
}

bool ToggleButton::GetIsOn() const {
  return slide_animation_.IsShowing();
}

void ToggleButton::SetThumbOnColor(
    const absl::optional<SkColor>& thumb_on_color) {
  thumb_view_->SetThumbColor(true /* is_on */, thumb_on_color);
}

absl::optional<SkColor> ToggleButton::GetThumbOnColor() const {
  return thumb_view_->GetThumbColor(true);
}

void ToggleButton::SetThumbOffColor(
    const absl::optional<SkColor>& thumb_off_color) {
  thumb_view_->SetThumbColor(false /* is_on */, thumb_off_color);
}

absl::optional<SkColor> ToggleButton::GetThumbOffColor() const {
  return thumb_view_->GetThumbColor(false);
}

void ToggleButton::SetTrackOnColor(
    const absl::optional<SkColor>& track_on_color) {
  track_on_color_ = track_on_color;
}

absl::optional<SkColor> ToggleButton::GetTrackOnColor() const {
  return track_on_color_;
}

void ToggleButton::SetTrackOffColor(
    const absl::optional<SkColor>& track_off_color) {
  track_off_color_ = track_off_color;
}

absl::optional<SkColor> ToggleButton::GetTrackOffColor() const {
  return track_off_color_;
}

void ToggleButton::SetAcceptsEvents(bool accepts_events) {
  if (GetAcceptsEvents() == accepts_events)
    return;
  accepts_events_ = accepts_events;
  OnPropertyChanged(&accepts_events_, kPropertyEffectsNone);
}

bool ToggleButton::GetAcceptsEvents() const {
  return accepts_events_;
}

void ToggleButton::AddLayerBeneathView(ui::Layer* layer) {
  // Ink-drop layers should go underneath the ThumbView.
  thumb_view_->AddLayerBeneathView(layer);
}

void ToggleButton::RemoveLayerBeneathView(ui::Layer* layer) {
  thumb_view_->RemoveLayerBeneathView(layer);
}

gfx::Size ToggleButton::CalculatePreferredSize() const {
  gfx::Rect rect(kTrackSize);
  rect.Inset(gfx::Insets::VH(-kTrackVerticalMargin, -kTrackHorizontalMargin));
  rect.Inset(-GetInsets());
  return rect.size();
}

gfx::Rect ToggleButton::GetTrackBounds() const {
  gfx::Rect track_bounds(GetContentsBounds());
  track_bounds.ClampToCenteredSize(kTrackSize);
  return track_bounds;
}

gfx::Rect ToggleButton::GetThumbBounds() const {
  gfx::Rect thumb_bounds(GetTrackBounds());
  thumb_bounds.Inset(gfx::Insets(-kThumbInset));
  thumb_bounds.set_x(thumb_bounds.x() +
                     slide_animation_.GetCurrentValue() *
                         (thumb_bounds.width() - thumb_bounds.height()));
  // The thumb is a circle, so the width should match the height.
  thumb_bounds.set_width(thumb_bounds.height());
  thumb_bounds.Inset(ThumbView::GetShadowOutsets());
  return thumb_bounds;
}

void ToggleButton::UpdateThumb() {
  thumb_view_->Update(GetThumbBounds(),
                      static_cast<float>(slide_animation_.GetCurrentValue()));
  if (FocusRing::Get(this)) {
    // Updating the thumb changes the result of GetFocusRingPath(), make sure
    // the focus ring gets updated to match this new state.
    FocusRing::Get(this)->InvalidateLayout();
    FocusRing::Get(this)->SchedulePaint();
  }
}

SkColor ToggleButton::GetTrackColor(bool is_on) const {
  absl::optional<SkColor> color = is_on ? track_on_color_ : track_off_color_;
  return color.value_or(GetColorProvider()->GetColor(
      is_on ? ui::kColorToggleButtonTrackOn : ui::kColorToggleButtonTrackOff));
}

bool ToggleButton::CanAcceptEvent(const ui::Event& event) {
  return GetAcceptsEvents() && Button::CanAcceptEvent(event);
}

void ToggleButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateThumb();
}

void ToggleButton::OnThemeChanged() {
  Button::OnThemeChanged();
  SchedulePaint();
}

void ToggleButton::NotifyClick(const ui::Event& event) {
  AnimateIsOn(!GetIsOn());

  // Only trigger the action when we don't have focus. This lets the InkDrop
  // remain and match the focus ring.
  // TODO(pbos): Investigate triggering the ripple but returning back to the
  // focused state correctly. This is set up to highlight on focus, but the
  // highlight does not come back after the ripple is triggered. Then remove
  // this and add back SetHasInkDropActionOnClick(true) in the constructor.
  if (!HasFocus()) {
    InkDrop::Get(this)->AnimateToState(InkDropState::ACTION_TRIGGERED,
                                       ui::LocatedEvent::FromIfValid(&event));
  }

  Button::NotifyClick(event);
}

SkPath ToggleButton::GetFocusRingPath() const {
  SkPath path;
  const gfx::Point center = GetThumbBounds().CenterPoint();
  const int kFocusRingRadius = 16;
  path.addCircle(center.x(), center.y(), kFocusRingRadius);
  return path;
}

void ToggleButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);

  node_data->role = ax::mojom::Role::kSwitch;
  node_data->SetCheckedState(GetIsOn() ? ax::mojom::CheckedState::kTrue
                                       : ax::mojom::CheckedState::kFalse);
}

void ToggleButton::OnFocus() {
  Button::OnFocus();
  InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTION_PENDING,
                                     nullptr);
  SchedulePaint();
}

void ToggleButton::OnBlur() {
  Button::OnBlur();

  // The ink drop may have already gone away if the user clicked after focusing.
  if (InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
      views::InkDropState::ACTION_PENDING) {
    InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTION_TRIGGERED,
                                       nullptr);
  }
  SchedulePaint();
}

void ToggleButton::PaintButtonContents(gfx::Canvas* canvas) {
  // Paint the toggle track. To look sharp even at fractional scale factors,
  // round up to pixel boundaries.
  canvas->Save();
  float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF track_rect(GetTrackBounds());
  track_rect.Scale(dsf);
  track_rect = gfx::RectF(gfx::ToEnclosingRect(track_rect));
  cc::PaintFlags track_flags;
  track_flags.setAntiAlias(true);
  const float color_ratio =
      static_cast<float>(slide_animation_.GetCurrentValue());
  track_flags.setColor(color_utils::AlphaBlend(
      GetTrackColor(true), GetTrackColor(false), color_ratio));
  canvas->DrawRoundRect(track_rect, track_rect.height() / 2, track_flags);
  canvas->Restore();
}

void ToggleButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &slide_animation_) {
    // TODO(varkha, estade): The thumb is using its own view. Investigate if
    // repainting in every animation step to update colors could be avoided.
    UpdateThumb();
    SchedulePaint();
    return;
  }
  Button::AnimationProgressed(animation);
}

BEGIN_METADATA(ToggleButton, Button)
ADD_PROPERTY_METADATA(bool, IsOn)
ADD_PROPERTY_METADATA(bool, AcceptsEvents)
ADD_PROPERTY_METADATA(absl::optional<SkColor>, ThumbOnColor)
ADD_PROPERTY_METADATA(absl::optional<SkColor>, ThumbOffColor)
ADD_PROPERTY_METADATA(absl::optional<SkColor>, TrackOnColor)
ADD_PROPERTY_METADATA(absl::optional<SkColor>, TrackOffColor)
END_METADATA

}  // namespace views

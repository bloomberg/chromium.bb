// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_ring_handler.h"

#include "base/command_line.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The default values of the autoclick ring widget size.
const int kAutoclickRingOuterRadius = 30;
const int kAutoclickRingInnerRadius = 20;

// Angles from x-axis at which the outer and inner circles start.
const int kAutoclickRingInnerStartAngle = -90;

const int kAutoclickRingGlowWidth = 20;
// The following is half width to avoid division by 2.
const int kAutoclickRingArcWidth = 2;

// Start and end values for various animations.
const double kAutoclickRingScaleStartValue = 1.0;
const double kAutoclickRingScaleEndValue = 1.0;
const double kAutoclickRingShrinkScaleEndValue = 0.5;

const double kAutoclickRingOpacityStartValue = 0.1;
const double kAutoclickRingOpacityEndValue = 0.5;
const int kAutoclickRingAngleStartValue = -90;
// The sweep angle is a bit greater than 360 to make sure the circle
// completes at the end of the animation.
const int kAutoclickRingAngleEndValue = 360;

// Visual constants.
const SkColor kAutoclickRingArcColor = SkColorSetARGB(255, 0, 255, 0);
const SkColor kAutoclickRingCircleColor = SkColorSetARGB(255, 0, 0, 255);

// Constants for colors in V2, which has different UX.
const SkColor kAutoclickRingArcColorV2 = SkColorSetARGB(255, 255, 255, 255);
const SkColor kAutoclickRingCircleColorV2 = SkColorSetARGB(128, 0, 0, 0);
const SkColor kAutoclickRingUnderArcColor = SkColorSetARGB(100, 128, 134, 139);

void PaintAutoclickRingCircle(gfx::Canvas* canvas,
                              gfx::Point& center,
                              int radius) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2 * kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingCircleColor);
  flags.setAntiAlias(true);

  canvas->DrawCircle(center, radius, flags);
}

void PaintAutoclickRingArc(gfx::Canvas* canvas,
                           const gfx::Point& center,
                           int radius,
                           int start_angle,
                           int end_angle) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2 * kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingArcColor);
  flags.setAntiAlias(true);

  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius, center.y() - radius,
                                   2 * radius, 2 * radius),
                  start_angle, end_angle - start_angle);
  canvas->DrawPath(arc_path, flags);
}

// Paints the full Autoclick ring.
void PaintAutoclickRingV2(gfx::Canvas* canvas,
                          const gfx::Point& center,
                          int radius,
                          int start_angle,
                          int end_angle) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  // Draw the point being selected.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kAutoclickRingArcColorV2);
  canvas->DrawCircle(center, kAutoclickRingArcWidth / 2, flags);

  // Draw the outline of the point being selected.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingCircleColorV2);
  canvas->DrawCircle(center, kAutoclickRingArcWidth * 3 / 2, flags);

  // Draw the outline of the arc.
  flags.setColor(kAutoclickRingCircleColorV2);
  canvas->DrawCircle(center, radius + kAutoclickRingArcWidth, flags);
  canvas->DrawCircle(center, radius - kAutoclickRingArcWidth, flags);

  // Draw the background of the arc.
  flags.setColor(kAutoclickRingUnderArcColor);
  canvas->DrawCircle(center, radius, flags);

  // Draw the arc.
  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius, center.y() - radius,
                                   2 * radius, 2 * radius),
                  start_angle, end_angle - start_angle);
  flags.setStrokeWidth(kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingArcColorV2);

  canvas->DrawPath(arc_path, flags);
}

}  // namespace

// View of the AutoclickRingHandler. Draws the actual contents and updates as
// the animation proceeds. It also maintains the views::Widget that the
// animation is shown in.
class AutoclickRingHandler::AutoclickRingView : public views::View {
 public:
  AutoclickRingView(const gfx::Point& event_location,
                    views::Widget* ring_widget,
                    int outer_radius,
                    int inner_radius)
      : views::View(),
        widget_(ring_widget),
        current_angle_(kAutoclickRingAngleStartValue),
        current_scale_(kAutoclickRingScaleStartValue),
        outer_radius_(outer_radius),
        inner_radius_(inner_radius),
        is_v2_enabled_(base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalAccessibilityAutoclick)) {
    widget_->SetContentsView(this);

    // We are owned by the AutoclickRingHandler.
    set_owned_by_client();
    SetNewLocation(event_location);
  }

  ~AutoclickRingView() override = default;

  void SetNewLocation(const gfx::Point& new_event_location) {
    gfx::Point point = new_event_location;
    widget_->SetBounds(
        gfx::Rect(point.x() - (outer_radius_ + kAutoclickRingGlowWidth),
                  point.y() - (outer_radius_ + kAutoclickRingGlowWidth),
                  GetPreferredSize().width(), GetPreferredSize().height()));
    widget_->Show();
    if (is_v2_enabled_) {
      widget_->GetNativeView()->layer()->SetOpacity(1.0);
    } else {
      widget_->GetNativeView()->layer()->SetOpacity(
          kAutoclickRingOpacityStartValue);
    }
  }

  void UpdateWithGrowAnimation(gfx::Animation* animation) {
    // Update the portion of the circle filled so far and re-draw.
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingInnerStartAngle, kAutoclickRingAngleEndValue);
    if (!is_v2_enabled_) {
      current_scale_ = animation->CurrentValueBetween(
          kAutoclickRingScaleStartValue, kAutoclickRingScaleEndValue);
      widget_->GetNativeView()->layer()->SetOpacity(
          animation->CurrentValueBetween(kAutoclickRingOpacityStartValue,
                                         kAutoclickRingOpacityEndValue));
    }
    SchedulePaint();
  }

  void UpdateWithShrinkAnimation(gfx::Animation* animation) {
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingInnerStartAngle, kAutoclickRingAngleEndValue);
    if (!is_v2_enabled_) {
      current_scale_ = animation->CurrentValueBetween(
          kAutoclickRingScaleEndValue, kAutoclickRingShrinkScaleEndValue);
      widget_->GetNativeView()->layer()->SetOpacity(
          animation->CurrentValueBetween(kAutoclickRingOpacityStartValue,
                                         kAutoclickRingOpacityEndValue));
    }
    SchedulePaint();
  }

  void SetSize(int outer_radius, int inner_radius) {
    outer_radius_ = outer_radius;
    inner_radius_ = inner_radius;
  }

 private:
  // Overridden from views::View.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(2 * (outer_radius_ + kAutoclickRingGlowWidth),
                     2 * (outer_radius_ + kAutoclickRingGlowWidth));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Point center(GetPreferredSize().width() / 2,
                      GetPreferredSize().height() / 2);
    canvas->Save();

    gfx::Transform scale;
    scale.Scale(current_scale_, current_scale_);
    // We want to scale from the center.
    canvas->Translate(center.OffsetFromOrigin());
    canvas->Transform(scale);
    canvas->Translate(-center.OffsetFromOrigin());

    if (is_v2_enabled_) {
      PaintAutoclickRingV2(canvas, center, inner_radius_,
                           kAutoclickRingInnerStartAngle, current_angle_);
    } else {
      // Paint inner circle.
      PaintAutoclickRingArc(canvas, center, inner_radius_,
                            kAutoclickRingInnerStartAngle, current_angle_);
      // Paint outer circle.
      PaintAutoclickRingCircle(canvas, center, outer_radius_);
    }

    canvas->Restore();
  }

  views::Widget* widget_;
  int current_angle_;
  double current_scale_;
  int outer_radius_;
  int inner_radius_;

  // Autoclick UX is being updated. This bool tracks whether the feature flag
  // was set to view the new UX, or whether the old UX should be shown.
  // TODO(crbug.com/894907): Remove this flag and the old UX when launching.
  bool is_v2_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickRingView);
};

////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, public
AutoclickRingHandler::AutoclickRingHandler()
    : gfx::LinearAnimation(nullptr),
      ring_widget_(nullptr),
      current_animation_type_(AnimationType::NONE),
      outer_radius_(kAutoclickRingOuterRadius),
      inner_radius_(kAutoclickRingInnerRadius) {}

AutoclickRingHandler::~AutoclickRingHandler() {
  StopAutoclickRing();
}

void AutoclickRingHandler::StartGesture(
    base::TimeDelta duration,
    const gfx::Point& center_point_in_screen,
    views::Widget* widget) {
  StopAutoclickRing();
  tap_down_location_ = center_point_in_screen;
  ring_widget_ = widget;
  current_animation_type_ = AnimationType::GROW_ANIMATION;
  animation_duration_ = duration;
  StartAnimation(base::TimeDelta());
}

void AutoclickRingHandler::StopGesture() {
  StopAutoclickRing();
}

void AutoclickRingHandler::SetGestureCenter(
    const gfx::Point& center_point_in_screen,
    views::Widget* widget) {
  tap_down_location_ = center_point_in_screen;
  ring_widget_ = widget;
}

void AutoclickRingHandler::SetSize(int outer_radius, int inner_radius) {
  outer_radius_ = outer_radius;
  inner_radius_ = inner_radius;
  if (view_)
    view_->SetSize(outer_radius, inner_radius);
}
////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, private
void AutoclickRingHandler::StartAnimation(base::TimeDelta delay) {
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION: {
      view_.reset(new AutoclickRingView(tap_down_location_, ring_widget_,
                                        outer_radius_, inner_radius_));
      SetDuration(delay);
      Start();
      break;
    }
    case AnimationType::SHRINK_ANIMATION: {
      view_.reset(new AutoclickRingView(tap_down_location_, ring_widget_,
                                        outer_radius_, inner_radius_));
      SetDuration(delay);
      Start();
      break;
    }
    case AnimationType::NONE:
      NOTREACHED();
      break;
  }
}

void AutoclickRingHandler::StopAutoclickRing() {
  // Since, Animation::Stop() calls AnimationStopped(), we need to reset the
  // |current_animation_type_| before Stop(), otherwise AnimationStopped() may
  // start the timer again.
  current_animation_type_ = AnimationType::NONE;
  Stop();
  view_.reset();
}

void AutoclickRingHandler::AnimateToState(double state) {
  DCHECK(view_.get());
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION:
      view_->SetNewLocation(tap_down_location_);
      view_->UpdateWithGrowAnimation(this);
      break;
    case AnimationType::SHRINK_ANIMATION:
      view_->SetNewLocation(tap_down_location_);
      view_->UpdateWithShrinkAnimation(this);
      break;
    case AnimationType::NONE:
      NOTREACHED();
      break;
  }
}

void AutoclickRingHandler::AnimationStopped() {
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION:
      current_animation_type_ = AnimationType::SHRINK_ANIMATION;
      StartAnimation(animation_duration_);
      break;
    case AnimationType::SHRINK_ANIMATION:
      current_animation_type_ = AnimationType::NONE;
      break;
    case AnimationType::NONE:
      // fall through to reset the view.
      view_.reset();
      break;
  }
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Width of the contextual nudge.
constexpr int kBackgroundWidth = 320;

// Radius of the circle in the middle of the contextual nudge.
constexpr int kCircleRadius = 20;

// Color of the circle in the middle of the contextual nudge.
constexpr SkColor kCircleColor = SK_ColorWHITE;

// Width of the circle that inside the screen at the beginning.
constexpr int kCircleInsideScreenWidth = 12;

// Padding between the circle and the label.
constexpr int kPaddingBetweenCircleAndLabel = 8;

// Color of the label.
constexpr SkColor kLabelColor = gfx::kGoogleGrey200;

// Color of the label background.
constexpr SkColor kLabelBackgroundColor = SkColorSetA(SK_ColorBLACK, 0xDE);

// Line height of the label.
constexpr int kLabelLineHeight = 18;

// Corner radius for the label's background.
constexpr int kLabelCornerRadius = 16;

// Top and bottom inset of the label.
constexpr int kLabelTopBottomInset = 6;

// Shadow values for the back nudge circle.
constexpr int kBackNudgeShadowOffsetY1 = 1;
constexpr int kBackNudgeShadowBlurRadius1 = 2;
constexpr SkColor kBackNudgeShadowColor1 = SkColorSetA(SK_ColorBLACK, 0x4D);
constexpr int kBackNudgeShadowOffsetY2 = 2;
constexpr int kBackNudgeShadowBlurRadius2 = 6;
constexpr SkColor kBackNudgeShadowColor2 = SkColorSetA(SK_ColorBLACK, 0x26);

// Duration of the pause before sliding in to show the nudge.
constexpr base::TimeDelta kPauseBeforeShowAnimationDuration =
    base::TimeDelta::FromSeconds(10);

// Duration for the animation to show the nudge.
constexpr base::TimeDelta kNudgeShowAnimationDuration =
    base::TimeDelta::FromMilliseconds(600);

// Duration for the animation to hide the nudge.
constexpr base::TimeDelta kNudgeHideAnimationDuration =
    base::TimeDelta::FromMilliseconds(400);

// Duration for the animation to fade out the suggestion label and circle when
// the back nudge showing animation is interrupted and should be dismissed.
constexpr base::TimeDelta kSuggestionDismissDuration =
    base::TimeDelta::FromMilliseconds(100);

// Duration for the animation of the suggestion part of the nudge.
constexpr base::TimeDelta kSuggestionBounceAnimationDuration =
    base::TimeDelta::FromMilliseconds(600);

// Repeat bouncing times of the suggestion animation.
constexpr int kSuggestionAnimationRepeatTimes = 4;

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "BackGestureContextualNudge";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_OverlayContainer);
  widget->Init(std::move(params));

  // TODO(crbug.com/1009005): Get the bounds of the display that should show the
  // nudge, which may based on the conditions to show the nudge.
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  gfx::Rect widget_bounds;
  if (base::i18n::IsRTL()) {
    widget_bounds = gfx::Rect(display_bounds.right(), display_bounds.y(),
                              kBackgroundWidth, display_bounds.height());
  } else {
    widget_bounds =
        gfx::Rect(display_bounds.x() - kBackgroundWidth, display_bounds.y(),
                  kBackgroundWidth, display_bounds.height());
  }
  widget->SetBounds(widget_bounds);
  return widget;
}

}  // namespace

class BackGestureContextualNudge::ContextualNudgeView
    : public views::View,
      public ui::ImplicitAnimationObserver {
 public:
  explicit ContextualNudgeView(base::OnceClosure callback)
      : callback_(std::move(callback)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    suggestion_view_ = AddChildView(std::make_unique<SuggestionView>(this));
    show_timer_.Start(
        FROM_HERE, kPauseBeforeShowAnimationDuration, this,
        &ContextualNudgeView::ScheduleOffScreenToStartPositionAnimation);
  }
  ContextualNudgeView(const ContextualNudgeView&) = delete;
  ContextualNudgeView& operator=(const ContextualNudgeView&) = delete;

  ~ContextualNudgeView() override { StopObservingImplicitAnimations(); }

  // Cancel in-waiting animation or in-progress animation.
  void CancelAnimationOrFadeOutToHide() {
    if (animation_stage_ == AnimationStage::kWaitingCancelled ||
        animation_stage_ == AnimationStage::kFadingOut) {
      return;
    }

    if (animation_stage_ == AnimationStage::kWaiting) {
      // Cancel the animation if it's waiting to be shown.
      animation_stage_ = AnimationStage::kWaitingCancelled;
      DCHECK(show_timer_.IsRunning());
      show_timer_.AbandonAndStop();
      std::move(callback_).Run();
    } else if (animation_stage_ == AnimationStage::kSlidingIn ||
               animation_stage_ == AnimationStage::kBouncing ||
               animation_stage_ == AnimationStage::kSlidingOut) {
      // Cancel previous animations and fade out the widget if it's animating.
      layer()->GetAnimator()->AbortAllAnimations();
      suggestion_view_->layer()->GetAnimator()->AbortAllAnimations();

      animation_stage_ = AnimationStage::kFadingOut;
      suggestion_view_->FadeOutForDismiss();
    }
  }

  bool count_as_shown() const { return count_as_shown_; }

 private:
  enum class AnimationStage {
    kWaiting,     // Animation hasn't been started.
    kSlidingIn,   // Sliding in animation to show the affordance and label.
    kBouncing,    // Bouncing the affordance and label animation.
    kSlidingOut,  // Sliding out animation to hide the affordance and label.
    kWaitingCancelled,  // The in-waiting animation is cancelled.
    kFadingOut,  // Previous in-progress animations are cancelled, fading out
                 // the affordance and label.
  };

  // Used to show the suggestion information of the nudge, which includes the
  // affordance and a label.
  class SuggestionView : public views::View,
                         public ui::ImplicitAnimationObserver {
   public:
    explicit SuggestionView(ContextualNudgeView* nudge_view)
        : nudge_view_(nudge_view) {
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);

      label_ = AddChildView(std::make_unique<views::Label>());
      label_->SetBackgroundColor(SK_ColorTRANSPARENT);
      label_->SetEnabledColor(kLabelColor);
      label_->SetText(l10n_util::GetStringUTF16(
          base::i18n::IsRTL() ? IDS_ASH_BACK_GESTURE_CONTEXTUAL_NUDGE_RTL
                              : IDS_ASH_BACK_GESTURE_CONTEXTUAL_NUDGE));
      label_->SetLineHeight(kLabelLineHeight);
      label_->SetFontList(
          gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    }
    SuggestionView(const SuggestionView&) = delete;
    SuggestionView& operator=(const SuggestionView&) = delete;

    ~SuggestionView() override { StopObservingImplicitAnimations(); }

    void ScheduleBounceAnimation() {
      const bool is_rtl = base::i18n::IsRTL();
      gfx::Transform transform;
      const int x_offset = kCircleRadius - kCircleInsideScreenWidth;
      transform.Translate(is_rtl ? x_offset : -x_offset, 0);
      ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
      animation.AddObserver(this);
      animation.SetTransitionDuration(kSuggestionBounceAnimationDuration);
      animation.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
      animation.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      layer()->SetTransform(transform);

      transform.Translate(is_rtl ? -x_offset : x_offset, 0);
      animation.SetTransitionDuration(kSuggestionBounceAnimationDuration);
      animation.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
      animation.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      layer()->SetTransform(transform);
    }

    // Called when the in-progress animation is cancelled. The suggestion view
    // will fade out.
    void FadeOutForDismiss() {
      ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
      animation.SetTransitionDuration(kSuggestionDismissDuration);
      animation.SetTweenType(gfx::Tween::LINEAR);
      animation.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      animation.AddObserver(nudge_view_);
      layer()->SetOpacity(0.f);
    }

   private:
    // views::View:
    void Layout() override {
      const gfx::Rect bounds = GetLocalBounds();
      gfx::Rect label_rect(bounds);
      label_rect.ClampToCenteredSize(label_->GetPreferredSize());
      label_rect.set_x(bounds.x() + 2 * kCircleRadius +
                       kPaddingBetweenCircleAndLabel + kLabelCornerRadius);
      label_->SetBoundsRect(label_rect);
    }

    // views::View:
    void OnPaint(gfx::Canvas* canvas) override {
      // Draw the circle.
      cc::PaintFlags circle_flags;
      circle_flags.setAntiAlias(true);
      circle_flags.setStyle(cc::PaintFlags::kFill_Style);
      circle_flags.setColor(kCircleColor);
      gfx::ShadowValues shadows;
      shadows.push_back(gfx::ShadowValue(
          gfx::Vector2d(0, kBackNudgeShadowOffsetY1),
          kBackNudgeShadowBlurRadius1, kBackNudgeShadowColor1));
      shadows.push_back(gfx::ShadowValue(
          gfx::Vector2d(0, kBackNudgeShadowOffsetY2),
          kBackNudgeShadowBlurRadius2, kBackNudgeShadowColor2));
      circle_flags.setLooper(gfx::CreateShadowDrawLooper(shadows));
      if (base::i18n::IsRTL()) {
        const gfx::Point right_center = GetLocalBounds().right_center();
        canvas->DrawCircle(
            gfx::Point(right_center.x() - kCircleRadius, right_center.y()),
            kCircleRadius, circle_flags);
      } else {
        const gfx::Point left_center = GetLocalBounds().left_center();
        canvas->DrawCircle(
            gfx::Point(left_center.x() + kCircleRadius, left_center.y()),
            kCircleRadius, circle_flags);
      }

      // Draw the black round rectangle around the text.
      cc::PaintFlags round_rect_flags;
      round_rect_flags.setStyle(cc::PaintFlags::kFill_Style);
      round_rect_flags.setAntiAlias(true);
      round_rect_flags.setColor(kLabelBackgroundColor);
      gfx::Rect label_bounds(label_->GetMirroredBounds());
      label_bounds.Inset(/*horizontal=*/-kLabelCornerRadius,
                         /*vertical=*/-kLabelTopBottomInset);
      canvas->DrawRoundRect(label_bounds, kLabelCornerRadius, round_rect_flags);
    }

    // ui::ImplicitAnimationObserver:
    void OnImplicitAnimationsCompleted() override {
      // Do not do the following animation if the bouncing animation is aborted.
      if (WasAnimationAbortedForProperty(ui::LayerAnimationElement::TRANSFORM))
        return;

      if (current_animation_times_ < (kSuggestionAnimationRepeatTimes - 1)) {
        current_animation_times_++;
        ScheduleBounceAnimation();
      } else {
        nudge_view_->ScheduleStartPositionToOffScreenAnimation();
      }
    }

    views::Label* label_ = nullptr;
    int current_animation_times_ = 0;
    ContextualNudgeView* nudge_view_ = nullptr;  // Not owned.
  };

  // Showing contextual nudge from off screen to its start position.
  void ScheduleOffScreenToStartPositionAnimation() {
    animation_stage_ = AnimationStage::kSlidingIn;
    gfx::Transform transform;
    transform.Translate(base::i18n::IsRTL() ? -kBackgroundWidth + kCircleRadius
                                            : kBackgroundWidth - kCircleRadius,
                        0);
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.AddObserver(this);
    animation.SetTransitionDuration(kNudgeShowAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer()->SetTransform(transform);
  }

  // Hiding the contextual nudge from its current position to off screen.
  void ScheduleStartPositionToOffScreenAnimation() {
    animation_stage_ = AnimationStage::kSlidingOut;
    gfx::Transform transform;
    transform.Translate(base::i18n::IsRTL() ? kBackgroundWidth - kCircleRadius
                                            : -kBackgroundWidth + kCircleRadius,
                        0);
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(kNudgeHideAnimationDuration);
    animation.SetTweenType(gfx::Tween::EASE_OUT_2);
    animation.AddObserver(this);
    layer()->SetTransform(transform);
  }

  // views::View:
  void Layout() override { suggestion_view_->SetBoundsRect(GetLocalBounds()); }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (animation_stage_ == AnimationStage::kFadingOut ||
        (animation_stage_ == AnimationStage::kSlidingOut &&
         !WasAnimationAbortedForProperty(
             ui::LayerAnimationElement::TRANSFORM))) {
      std::move(callback_).Run();
      return;
    }

    if (animation_stage_ == AnimationStage::kSlidingIn &&
        !WasAnimationAbortedForProperty(ui::LayerAnimationElement::TRANSFORM)) {
      count_as_shown_ = true;
      animation_stage_ = AnimationStage::kBouncing;
      suggestion_view_->ScheduleBounceAnimation();
    }
  }

  // Created by ContextualNudgeView. Owned by views hierarchy.
  SuggestionView* suggestion_view_ = nullptr;

  // Timer to start show the sliding in animation.
  base::OneShotTimer show_timer_;

  // Current animation stage;
  AnimationStage animation_stage_ = AnimationStage::kWaiting;

  // The nudge should be counted as shown if the nudge has finished its sliding-
  // in animation no matter whether its following animations get cancelled or
  // not.
  bool count_as_shown_ = false;

  // Callback function to be called after animation is cancelled or completed.
  // Count the nudge as shown successfully if |success| is true.
  base::OnceClosure callback_;
};

BackGestureContextualNudge::BackGestureContextualNudge(
    base::OnceClosure callback) {
  widget_ = CreateWidget();
  nudge_view_ = new ContextualNudgeView(std::move(callback));
  widget_->SetContentsView(nudge_view_);
  widget_->Show();
}

BackGestureContextualNudge::~BackGestureContextualNudge() = default;

void BackGestureContextualNudge::CancelAnimationOrFadeOutToHide() {
  nudge_view_->CancelAnimationOrFadeOutToHide();
}

bool BackGestureContextualNudge::ShouldNudgeCountAsShown() const {
  return nudge_view_->count_as_shown();
}

}  // namespace ash

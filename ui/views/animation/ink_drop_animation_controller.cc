// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller.h"

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace {

// Animation constants
const float kMinimumScale = 0.1f;
const float kMinimumScaleCenteringOffset = 0.5f - kMinimumScale / 2.0f;

const int kHideAnimationDurationFastMs = 100;
const int kHideAnimationDurationSlowMs = 1000;

const int kShowInkDropAnimationDurationFastMs = 250;
const int kShowInkDropAnimationDurationSlowMs = 750;

const int kShowLongPressAnimationDurationFastMs = 250;
const int kShowLongPressAnimationDurationSlowMs = 2500;

const int kRoundedRectCorners = 5;
const int kCircleRadius = 30;

const SkColor kInkDropColor = SK_ColorLTGRAY;
const SkColor kLongPressColor = SkColorSetRGB(182, 182, 182);

// Checks CommandLine switches to determine if the visual feedback should be
// circular.
bool UseCircularFeedback() {
  static bool circular =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          (::switches::kMaterialDesignInkDrop)) !=
      ::switches::kMaterialDesignInkDropSquare;
  return circular;
}

// Checks CommandLine switches to determine if the visual feedback should have
// a fast animations speed.
bool UseFastAnimations() {
  static bool fast =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          (::switches::kMaterialDesignInkDropAnimationSpeed)) !=
      ::switches::kMaterialDesignInkDropAnimationSpeedSlow;
  return fast;
}

}  // namespace

namespace views {

// An animation observer that should be set on animations of the provided
// ui::Layer. Can be used to either start a hide animation, or to trigger one
// upon completion of the current animation.
//
// Sequential animations with PreemptionStrategy::ENQUEUE_NEW_ANIMATION cannot
// be used as the observed animation can complete before user input is received
// which determines if the hide animation should run.
class AppearAnimationObserver : public ui::LayerAnimationObserver {
 public:
  // Will automatically start a hide animation of |layer| if |hide| is true.
  // Otherwise StartHideAnimation() or HideNowIfDoneOrOnceCompleted() must be
  // called.
  AppearAnimationObserver(ui::Layer* layer, bool hide);
  ~AppearAnimationObserver() override;

  // Returns true during both the appearing animation, and the hiding animation.
  bool IsAnimationActive();

  // Starts a hide animation, preempting any current animations on |layer_|.
  void StartHideAnimation();

  // Starts a hide animation if |layer_| is no longer animating. Otherwise the
  // hide animation will be started once the current animation is completed.
  void HideNowIfDoneOrOnceCompleted();

  // Hides |background_layer| (without animation) after the current animation
  // completes.
  void SetBackgroundToHide(ui::Layer* background_layer);

 private:
  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  bool RequiresNotificationWhenAnimatorDestroyed() const override;

  // The ui::Layer being observed, which hide animations will be set on.
  ui::Layer* layer_;

  // Optional ui::Layer which will be hidden upon the completion of animating
  // |layer_|
  ui::Layer* background_layer_;

  // If true the hide animation will immediately be scheduled upon completion of
  // the observed animation.
  bool hide_;

  DISALLOW_COPY_AND_ASSIGN(AppearAnimationObserver);
};

AppearAnimationObserver::AppearAnimationObserver(ui::Layer* layer, bool hide)
    : layer_(layer), background_layer_(nullptr), hide_(hide) {
}

AppearAnimationObserver::~AppearAnimationObserver() {
  StopObserving();
}

bool AppearAnimationObserver::IsAnimationActive() {
  // Initial animation ongoing
  if (!attached_sequences().empty())
    return true;
  // Maintain the animation until told to hide.
  if (!hide_)
    return true;

  // Check the state of the triggered hide animation
  return layer_->GetAnimator()->IsAnimatingProperty(
             ui::LayerAnimationElement::OPACITY) &&
         layer_->GetTargetOpacity() == 0.0f &&
         layer_->GetAnimator()->IsAnimatingProperty(
             ui::LayerAnimationElement::VISIBILITY) &&
         !layer_->GetTargetVisibility();
}

void AppearAnimationObserver::StartHideAnimation() {
  if (background_layer_)
    background_layer_->SetVisible(false);
  if (!layer_->GetTargetVisibility())
    return;

  ui::ScopedLayerAnimationSettings animation(layer_->GetAnimator());
  animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      UseFastAnimations() ? kHideAnimationDurationFastMs
                          : kHideAnimationDurationSlowMs));
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer_->SetOpacity(0.0f);
  layer_->SetVisible(false);
}

void AppearAnimationObserver::HideNowIfDoneOrOnceCompleted() {
  hide_ = true;
  if (attached_sequences().empty())
    StartHideAnimation();
}

void AppearAnimationObserver::SetBackgroundToHide(ui::Layer* background_layer) {
  background_layer_ = background_layer;
}

void AppearAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (hide_)
    StartHideAnimation();
}

void AppearAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  if (hide_)
    StartHideAnimation();
}

bool AppearAnimationObserver::RequiresNotificationWhenAnimatorDestroyed()
    const {
  // Ensures that OnImplicitAnimationsCompleted is called even if the observed
  // animation is deleted. Allows for setting the proper state on |layer_|.
  return true;
}

// Renders the visual feedback. Will render a circle if |circle_| is true,
// otherwise renders a rounded rectangle.
class InkDropDelegate : public ui::LayerDelegate {
 public:
  InkDropDelegate(ui::Layer* layer, SkColor color);
  ~InkDropDelegate() override;

  // Sets the visual style of the feedback.
  void set_should_render_circle(bool should_render_circle) {
    should_render_circle_ = should_render_circle;
  }

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  base::Closure PrepareForLayerBoundsChange() override;

 private:
  // The ui::Layer being rendered to.
  ui::Layer* layer_;

  // The color to paint.
  SkColor color_;

  // When true renders a circle, otherwise renders a rounded rectangle.
  bool should_render_circle_;

  DISALLOW_COPY_AND_ASSIGN(InkDropDelegate);
};

InkDropDelegate::InkDropDelegate(ui::Layer* layer, SkColor color)
    : layer_(layer), color_(color), should_render_circle_(true) {
}

InkDropDelegate::~InkDropDelegate() {
}

void InkDropDelegate::OnPaintLayer(const ui::PaintContext& context) {
  SkPaint paint;
  paint.setColor(color_);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);

  gfx::Rect bounds = layer_->bounds();

  ui::PaintRecorder recorder(context, layer_->size());
  gfx::Canvas* canvas = recorder.canvas();
  if (should_render_circle_) {
    gfx::Point midpoint(bounds.width() * 0.5f, bounds.height() * 0.5f);
    canvas->DrawCircle(midpoint, kCircleRadius, paint);
  } else {
    canvas->DrawRoundRect(bounds, kRoundedRectCorners, paint);
  }
}

void InkDropDelegate::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
}

void InkDropDelegate::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

base::Closure InkDropDelegate::PrepareForLayerBoundsChange() {
  return base::Closure();
}

InkDropAnimationController::InkDropAnimationController(views::View* view)
    : ink_drop_layer_(new ui::Layer()),
      ink_drop_delegate_(
          new InkDropDelegate(ink_drop_layer_.get(), kInkDropColor)),
      appear_animation_observer_(nullptr),
      long_press_layer_(new ui::Layer()),
      long_press_delegate_(
          new InkDropDelegate(long_press_layer_.get(), kLongPressColor)),
      long_press_animation_observer_(nullptr),
      view_(view) {
  view_->SetPaintToLayer(true);
  view_->AddPreTargetHandler(this);
  ui::Layer* layer = view_->layer();
  layer->SetMasksToBounds(!UseCircularFeedback());
  SetupAnimationLayer(layer, long_press_layer_.get(),
                      long_press_delegate_.get());
  SetupAnimationLayer(layer, ink_drop_layer_.get(), ink_drop_delegate_.get());
  ink_drop_delegate_->set_should_render_circle(UseCircularFeedback());
}

InkDropAnimationController::~InkDropAnimationController() {
  view_->RemovePreTargetHandler(this);
}

void InkDropAnimationController::AnimateTapDown() {
  if ((appear_animation_observer_ &&
       appear_animation_observer_->IsAnimationActive()) ||
      (long_press_animation_observer_ &&
       long_press_animation_observer_->IsAnimationActive())) {
    // Only one animation at a time. Subsequent tap downs are ignored until the
    // current animation completes.
    return;
  }
  appear_animation_observer_.reset(
      new AppearAnimationObserver(ink_drop_layer_.get(), false));
  AnimateShow(ink_drop_layer_.get(), appear_animation_observer_.get(),
              UseCircularFeedback(),
              base::TimeDelta::FromMilliseconds(
                  (UseFastAnimations() ? kShowInkDropAnimationDurationFastMs
                                       : kShowInkDropAnimationDurationSlowMs)));
}

void InkDropAnimationController::AnimateHide() {
  if (appear_animation_observer_)
    appear_animation_observer_->HideNowIfDoneOrOnceCompleted();
}

void InkDropAnimationController::AnimateLongPress() {
  // Only one animation at a time. Subsequent long presses are ignored until the
  // current animation completes.
  if (long_press_animation_observer_ &&
      long_press_animation_observer_->IsAnimationActive()) {
    return;
  }
  appear_animation_observer_.reset();
  long_press_animation_observer_.reset(
      new AppearAnimationObserver(long_press_layer_.get(), true));
  long_press_animation_observer_->SetBackgroundToHide(ink_drop_layer_.get());
  AnimateShow(long_press_layer_.get(), long_press_animation_observer_.get(),
              true,
              base::TimeDelta::FromMilliseconds(
                  UseFastAnimations() ? kShowLongPressAnimationDurationFastMs
                                      : kShowLongPressAnimationDurationSlowMs));
}

void InkDropAnimationController::AnimateShow(ui::Layer* layer,
                                             AppearAnimationObserver* observer,
                                             bool circle,
                                             base::TimeDelta duration) {
  SetLayerBounds(layer, circle, view_->width(), view_->height());
  layer->SetVisible(true);
  layer->SetOpacity(1.0f);

  float start_x = layer->bounds().width() * kMinimumScaleCenteringOffset;
  float start_y = layer->bounds().height() * kMinimumScaleCenteringOffset;

  gfx::Transform initial_transform;
  initial_transform.Translate(start_x, start_y);
  initial_transform.Scale(kMinimumScale, kMinimumScale);
  layer->SetTransform(initial_transform);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  gfx::Transform identity_transform;
  ui::LayerAnimationElement* element =
      ui::LayerAnimationElement::CreateTransformElement(identity_transform,
                                                        duration);
  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(element);
  sequence->AddObserver(observer);
  animator->StartAnimation(sequence);
}

void InkDropAnimationController::SetLayerBounds(ui::Layer* layer,
                                                bool circle,
                                                int width,
                                                int height) {
  float circle_width = circle ? 2.0f * kCircleRadius : width;
  float circle_height = circle ? 2.0f * kCircleRadius : height;
  float circle_x = circle ? (width - circle_width) * 0.5f : 0;
  float circle_y = circle ? (height - circle_height) * 0.5f : 0;
  layer->SetBounds(gfx::Rect(circle_x, circle_y, circle_width, circle_height));
}

void InkDropAnimationController::SetupAnimationLayer(
    ui::Layer* parent,
    ui::Layer* layer,
    InkDropDelegate* delegate) {
  layer->SetFillsBoundsOpaquely(false);
  layer->set_delegate(delegate);
  layer->SetVisible(false);
  layer->SetBounds(gfx::Rect());
  parent->Add(layer);
  parent->StackAtBottom(layer);
}

void InkDropAnimationController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->target() != view_)
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      AnimateTapDown();
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      AnimateLongPress();
      break;
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_TAP:
      AnimateHide();
      break;
    default:
      break;
  }
}

}  // namespace views

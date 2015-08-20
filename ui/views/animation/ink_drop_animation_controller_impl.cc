// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_delegate.h"
#include "ui/views/animation/ink_drop_host.h"
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
    : layer_(layer), background_layer_(nullptr), hide_(hide) {}

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

InkDropAnimationControllerImpl::InkDropAnimationControllerImpl(
    InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
      ink_drop_layer_(new ui::Layer()),
      appear_animation_observer_(nullptr),
      long_press_layer_(new ui::Layer()),
      long_press_animation_observer_(nullptr),
      ink_drop_bounds_(0, 0, 0, 0) {
  ink_drop_delegate_.reset(new InkDropDelegate(ink_drop_layer_.get(),
                                               kInkDropColor, kCircleRadius,
                                               kRoundedRectCorners));
  long_press_delegate_.reset(new InkDropDelegate(long_press_layer_.get(),
                                                 kLongPressColor, kCircleRadius,
                                                 kRoundedRectCorners));

  SetupAnimationLayer(long_press_layer_.get(), long_press_delegate_.get());
  SetupAnimationLayer(ink_drop_layer_.get(), ink_drop_delegate_.get());

  root_layer_->Add(ink_drop_layer_.get());
  root_layer_->Add(long_press_layer_.get());

  // TODO(bruthig): Change this to only be called before the ink drop becomes
  // active. See www.crbug.com/522175.
  ink_drop_host_->AddInkDropLayer(root_layer_.get());
}

InkDropAnimationControllerImpl::~InkDropAnimationControllerImpl() {
  // TODO(bruthig): Change this to be called when the ink drop becomes hidden.
  // See www.crbug.com/522175.
  ink_drop_host_->RemoveInkDropLayer(root_layer_.get());
}

void InkDropAnimationControllerImpl::AnimateToState(InkDropState state) {
  // TODO(bruthig): Do not transition if we are already in |state| and restrict
  // any state transition that don't make sense or wouldn't look visually
  // appealing.
  switch (state) {
    case InkDropState::HIDDEN:
      AnimateHide();
      break;
    case InkDropState::ACTION_PENDING:
      AnimateTapDown();
      break;
    case InkDropState::QUICK_ACTION:
      AnimateTapDown();
      AnimateHide();
      break;
    case InkDropState::SLOW_ACTION:
      AnimateLongPress();
      break;
    case InkDropState::ACTIVATED:
      AnimateLongPress();
      break;
  }
}

void InkDropAnimationControllerImpl::SetInkDropSize(const gfx::Size& size) {
  SetInkDropBounds(gfx::Rect(ink_drop_bounds_.origin(), size));
}

gfx::Rect InkDropAnimationControllerImpl::GetInkDropBounds() const {
  return ink_drop_bounds_;
}

void InkDropAnimationControllerImpl::SetInkDropBounds(const gfx::Rect& bounds) {
  ink_drop_bounds_ = bounds;
  SetLayerBounds(ink_drop_layer_.get());
  SetLayerBounds(long_press_layer_.get());
}

void InkDropAnimationControllerImpl::AnimateTapDown() {
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
              base::TimeDelta::FromMilliseconds(
                  (UseFastAnimations() ? kShowInkDropAnimationDurationFastMs
                                       : kShowInkDropAnimationDurationSlowMs)));
}

void InkDropAnimationControllerImpl::AnimateHide() {
  if (appear_animation_observer_ &&
      appear_animation_observer_->IsAnimationActive()) {
    appear_animation_observer_->HideNowIfDoneOrOnceCompleted();
  } else if (long_press_animation_observer_) {
    long_press_animation_observer_->HideNowIfDoneOrOnceCompleted();
  }
}

void InkDropAnimationControllerImpl::AnimateLongPress() {
  // Only one animation at a time. Subsequent long presses are ignored until the
  // current animation completes.
  if (long_press_animation_observer_ &&
      long_press_animation_observer_->IsAnimationActive()) {
    return;
  }
  appear_animation_observer_.reset();
  long_press_animation_observer_.reset(
      new AppearAnimationObserver(long_press_layer_.get(), false));
  long_press_animation_observer_->SetBackgroundToHide(ink_drop_layer_.get());
  AnimateShow(long_press_layer_.get(), long_press_animation_observer_.get(),
              base::TimeDelta::FromMilliseconds(
                  UseFastAnimations() ? kShowLongPressAnimationDurationFastMs
                                      : kShowLongPressAnimationDurationSlowMs));
}

void InkDropAnimationControllerImpl::AnimateShow(
    ui::Layer* layer,
    AppearAnimationObserver* observer,
    base::TimeDelta duration) {
  layer->SetVisible(true);
  layer->SetOpacity(1.0f);

  float start_x = ink_drop_bounds_.x() +
                  layer->bounds().width() * kMinimumScaleCenteringOffset;
  float start_y = ink_drop_bounds_.y() +
                  layer->bounds().height() * kMinimumScaleCenteringOffset;

  gfx::Transform initial_transform;
  initial_transform.Translate(start_x, start_y);
  initial_transform.Scale(kMinimumScale, kMinimumScale);
  layer->SetTransform(initial_transform);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  gfx::Transform target_transform;
  target_transform.Translate(ink_drop_bounds_.x(), ink_drop_bounds_.y());
  ui::LayerAnimationElement* element =
      ui::LayerAnimationElement::CreateTransformElement(target_transform,
                                                        duration);
  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(element);
  sequence->AddObserver(observer);
  animator->StartAnimation(sequence);
}

void InkDropAnimationControllerImpl::SetLayerBounds(ui::Layer* layer) {
  bool circle = UseCircularFeedback();
  gfx::Size size = ink_drop_bounds_.size();
  float circle_width = circle ? 2.0f * kCircleRadius : size.width();
  float circle_height = circle ? 2.0f * kCircleRadius : size.height();
  float circle_x = circle ? (size.width() - circle_width) * 0.5f : 0;
  float circle_y = circle ? (size.height() - circle_height) * 0.5f : 0;
  layer->SetBounds(gfx::Rect(circle_x, circle_y, circle_width, circle_height));
}

void InkDropAnimationControllerImpl::SetupAnimationLayer(
    ui::Layer* layer,
    InkDropDelegate* delegate) {
  layer->SetFillsBoundsOpaquely(false);
  layer->set_delegate(delegate);
  layer->SetVisible(false);
  layer->SetBounds(gfx::Rect());
  delegate->set_should_render_circle(UseCircularFeedback());
}

}  // namespace views

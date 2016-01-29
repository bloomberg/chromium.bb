// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/view.h"

namespace {

// The minimum scale factor to use when scaling rectangle layers. Smaller values
// were causing visual anomalies.
const float kMinimumRectScale = 0.0001f;

// The minimum scale factor to use when scaling circle layers. Smaller values
// were causing visual anomalies.
const float kMinimumCircleScale = 0.001f;

// The ink drop color.
const SkColor kInkDropColor = SK_ColorBLACK;

// All the sub animations that are used to animate each of the InkDropStates.
// These are used to get time durations with
// GetAnimationDuration(InkDropSubAnimations). Note that in general a sub
// animation defines the duration for either a transformation animation or an
// opacity animation but there are some exceptions where an entire InkDropState
// animation consists of only 1 sub animation and it defines the duration for
// both the transformation and opacity animations.
enum InkDropSubAnimations {
  // HIDDEN sub animations.

  // The HIDDEN sub animation that is fading out to a hidden opacity.
  HIDDEN_FADE_OUT,

  // The HIDDEN sub animation that transforms the shape to a |small_size_|
  // circle.
  HIDDEN_TRANSFORM,

  // ACTION_PENDING sub animations.

  // The ACTION_PENDING sub animation that fades in to the visible opacity.
  ACTION_PENDING_FADE_IN,

  // The ACTION_PENDING sub animation that transforms the shape to a
  // |large_size_| circle.
  ACTION_PENDING_TRANSFORM,

  // QUICK_ACTION sub animations.

  // The QUICK_ACTION sub animation that is fading out to a hidden opacity.
  QUICK_ACTION_FADE_OUT,

  // The QUICK_ACTION sub animation that transforms the shape to a |large_size_|
  // circle.
  QUICK_ACTION_TRANSFORM,

  // SLOW_ACTION_PENDING sub animations.

  // The SLOW_ACTION_PENDING animation has only one sub animation which animates
  // to a |small_size_| rounded rectangle at visible opacity.
  SLOW_ACTION_PENDING,

  // SLOW_ACTION sub animations.

  // The SLOW_ACTION sub animation that is fading out to a hidden opacity.
  SLOW_ACTION_FADE_OUT,

  // The SLOW_ACTION sub animation that transforms the shape to a |large_size_|
  // rounded rectangle.
  SLOW_ACTION_TRANSFORM,

  // ACTIVATED sub animations.

  // The ACTIVATED sub animation that transforms the shape to a |large_size_|
  // circle. This is used when the ink drop is in a HIDDEN state prior to
  // animating to the ACTIVATED state.
  ACTIVATED_CIRCLE_TRANSFORM,

  // The ACTIVATED sub animation that transforms the shape to a |small_size_|
  // rounded rectangle.
  ACTIVATED_RECT_TRANSFORM,

  // DEACTIVATED sub animations.

  // The DEACTIVATED sub animation that is fading out to a hidden opacity.
  DEACTIVATED_FADE_OUT,

  // The DEACTIVATED sub animation that transforms the shape to a |large_size_|
  // rounded rectangle.
  DEACTIVATED_TRANSFORM,
};

// The scale factor used to burst the QUICK_ACTION bubble as it fades out.
const float kQuickActionBurstScale = 1.3f;

// A multiplicative factor used to slow down InkDropState animations.
const int kSlowAnimationDurationFactor = 3;

// Checks CommandLine switches to determine if the visual feedback should have
// a fast animations speed.
bool UseFastAnimations() {
  static bool fast =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          (::switches::kMaterialDesignInkDropAnimationSpeed)) !=
      ::switches::kMaterialDesignInkDropAnimationSpeedSlow;
  return fast;
}

// Duration constants for InkDropStateSubAnimations. See the
// InkDropStateSubAnimations enum documentation for more info.
int kAnimationDurationInMs[] = {
    150,  // HIDDEN_FADE_OUT
    200,  // HIDDEN_TRANSFORM
    0,    // ACTION_PENDING_FADE_IN
    160,  // ACTION_PENDING_TRANSFORM
    150,  // QUICK_ACTION_FADE_OUT
    160,  // QUICK_ACTION_TRANSFORM
    200,  // SLOW_ACTION_PENDING
    150,  // SLOW_ACTION_FADE_OUT
    200,  // SLOW_ACTION_TRANSFORM
    200,  // ACTIVATED_CIRCLE_TRANSFORM
    160,  // ACTIVATED_RECT_TRANSFORM
    150,  // DEACTIVATED_FADE_OUT
    200,  // DEACTIVATED_TRANSFORM
};

// Returns the InkDropState sub animation duration for the given |state|.
base::TimeDelta GetAnimationDuration(InkDropSubAnimations state) {
  return base::TimeDelta::FromMilliseconds(
      (UseFastAnimations() ? 1 : kSlowAnimationDurationFactor) *
      kAnimationDurationInMs[state]);
}

// Calculates a Transform for a circle layer. The transform will be set up to
// translate the |drawn_center_point| to the origin, scale, and then translate
// to the target point defined by |target_center_x| and |target_center_y|.
gfx::Transform CalculateCircleTransform(const gfx::Point& drawn_center_point,
                                        float scale,
                                        float target_center_x,
                                        float target_center_y) {
  gfx::Transform transform;
  transform.Translate(target_center_x, target_center_y);
  transform.Scale(scale, scale);
  transform.Translate(-drawn_center_point.x(), -drawn_center_point.y());
  return transform;
}

// Calculates a Transform for a rectangle layer. The transform will be set up to
// translate the |drawn_center_point| to the origin and then scale by the
// |x_scale| and |y_scale| factors.
gfx::Transform CalculateRectTransform(const gfx::Point& drawn_center_point,
                                      float x_scale,
                                      float y_scale) {
  gfx::Transform transform;
  transform.Scale(x_scale, y_scale);
  transform.Translate(-drawn_center_point.x(), -drawn_center_point.y());
  return transform;
}

}  // namespace

namespace views {

const float InkDropAnimation::kVisibleOpacity = 0.11f;
const float InkDropAnimation::kHiddenOpacity = 0.0f;

InkDropAnimation::InkDropAnimation(const gfx::Size& large_size,
                                   int large_corner_radius,
                                   const gfx::Size& small_size,
                                   int small_corner_radius)
    : large_size_(large_size),
      large_corner_radius_(large_corner_radius),
      small_size_(small_size),
      small_corner_radius_(small_corner_radius),
      circle_layer_delegate_(new CircleLayerDelegate(
          kInkDropColor,
          std::min(large_size_.width(), large_size_.height()) / 2)),
      rect_layer_delegate_(
          new RectangleLayerDelegate(kInkDropColor, large_size_)),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
      ink_drop_state_(InkDropState::HIDDEN) {
  root_layer_->set_name("InkDropAnimation:ROOT_LAYER");

  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    AddPaintLayer(static_cast<PaintedShape>(i));

  root_layer_->SetMasksToBounds(false);
  root_layer_->SetBounds(gfx::Rect(large_size_));

  SetStateToHidden();
}

InkDropAnimation::~InkDropAnimation() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  AbortAllAnimations();
}

void InkDropAnimation::AddObserver(InkDropAnimationObserver* observer) {
  observers_.AddObserver(observer);
}

void InkDropAnimation::RemoveObserver(InkDropAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InkDropAnimation::AnimateToState(InkDropState ink_drop_state) {
  // |animation_observer| will be deleted when AnimationEndedCallback() returns
  // true.
  ui::CallbackLayerAnimationObserver* animation_observer =
      new ui::CallbackLayerAnimationObserver(
          base::Bind(&InkDropAnimation::AnimationStartedCallback,
                     base::Unretained(this), ink_drop_state),
          base::Bind(&InkDropAnimation::AnimationEndedCallback,
                     base::Unretained(this), ink_drop_state));
  AnimateToStateInternal(ink_drop_state, animation_observer);
  animation_observer->SetActive();
}

void InkDropAnimation::SetCenterPoint(const gfx::Point& center_point) {
  gfx::Transform transform;
  transform.Translate(center_point.x(), center_point.y());
  root_layer_->SetTransform(transform);
}

void InkDropAnimation::HideImmediately() {
  AbortAllAnimations();
  SetStateToHidden();
  ink_drop_state_ = InkDropState::HIDDEN;
}

std::string InkDropAnimation::ToLayerName(PaintedShape painted_shape) {
  switch (painted_shape) {
    case TOP_LEFT_CIRCLE:
      return "TOP_LEFT_CIRCLE";
    case TOP_RIGHT_CIRCLE:
      return "TOP_RIGHT_CIRCLE";
    case BOTTOM_RIGHT_CIRCLE:
      return "BOTTOM_RIGHT_CIRCLE";
    case BOTTOM_LEFT_CIRCLE:
      return "BOTTOM_LEFT_CIRCLE";
    case HORIZONTAL_RECT:
      return "HORIZONTAL_RECT";
    case VERTICAL_RECT:
      return "VERTICAL_RECT";
    case PAINTED_SHAPE_COUNT:
      NOTREACHED() << "The PAINTED_SHAPE_COUNT value should never be used.";
      return "PAINTED_SHAPE_COUNT";
  }
  return "UNKNOWN";
}

void InkDropAnimation::AnimateToStateInternal(
    InkDropState ink_drop_state,
    ui::LayerAnimationObserver* animation_observer) {
  InkDropState previous_ink_drop_state = ink_drop_state_;
  ink_drop_state_ = ink_drop_state;

  InkDropTransforms transforms;
  root_layer_->SetVisible(true);

  switch (ink_drop_state_) {
    case InkDropState::HIDDEN:
      if (GetCurrentOpacity() == kHiddenOpacity) {
        AbortAllAnimations();
        break;
      } else {
        AnimateToOpacity(kHiddenOpacity, GetAnimationDuration(HIDDEN_FADE_OUT),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
        CalculateCircleTransforms(small_size_, &transforms);
        AnimateToTransforms(
            transforms, GetAnimationDuration(HIDDEN_TRANSFORM),
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
            gfx::Tween::EASE_IN_OUT, animation_observer);
      }
      break;
    case InkDropState::ACTION_PENDING:
      DCHECK(previous_ink_drop_state == InkDropState::HIDDEN);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ACTION_PENDING_FADE_IN),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ACTION_PENDING_TRANSFORM),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      CalculateCircleTransforms(large_size_, &transforms);
      AnimateToTransforms(transforms,
                          GetAnimationDuration(ACTION_PENDING_TRANSFORM),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    case InkDropState::QUICK_ACTION: {
      DCHECK(previous_ink_drop_state == InkDropState::HIDDEN ||
             previous_ink_drop_state == InkDropState::ACTION_PENDING);
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(QUICK_ACTION_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      gfx::Size s = ScaleToRoundedSize(large_size_, kQuickActionBurstScale);
      CalculateCircleTransforms(s, &transforms);
      AnimateToTransforms(transforms,
                          GetAnimationDuration(QUICK_ACTION_TRANSFORM),
                          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                          gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::SLOW_ACTION_PENDING:
      DCHECK(previous_ink_drop_state == InkDropState::ACTION_PENDING);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(SLOW_ACTION_PENDING),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      CalculateRectTransforms(small_size_, small_corner_radius_, &transforms);
      AnimateToTransforms(transforms, GetAnimationDuration(SLOW_ACTION_PENDING),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    case InkDropState::SLOW_ACTION: {
      DCHECK(previous_ink_drop_state == InkDropState::SLOW_ACTION_PENDING);
      base::TimeDelta visible_duration =
          GetAnimationDuration(SLOW_ACTION_TRANSFORM) -
          GetAnimationDuration(SLOW_ACTION_FADE_OUT);
      AnimateToOpacity(kVisibleOpacity, visible_duration,
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(SLOW_ACTION_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      CalculateRectTransforms(large_size_, large_corner_radius_, &transforms);
      AnimateToTransforms(transforms,
                          GetAnimationDuration(SLOW_ACTION_TRANSFORM),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::ACTIVATED: {
      // Animate the opacity so that it cancels any opacity animations already
      // in progress.
      AnimateToOpacity(kVisibleOpacity, base::TimeDelta(),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN_OUT, animation_observer);

      ui::LayerAnimator::PreemptionStrategy rect_transform_preemption_strategy =
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
      if (previous_ink_drop_state == InkDropState::HIDDEN) {
        rect_transform_preemption_strategy =
            ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
        CalculateCircleTransforms(large_size_, &transforms);
        AnimateToTransforms(
            transforms, GetAnimationDuration(ACTIVATED_CIRCLE_TRANSFORM),
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
            gfx::Tween::EASE_IN_OUT, animation_observer);
      } else if (previous_ink_drop_state == InkDropState::ACTION_PENDING) {
        rect_transform_preemption_strategy =
            ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      }

      CalculateRectTransforms(small_size_, small_corner_radius_, &transforms);
      AnimateToTransforms(transforms,
                          GetAnimationDuration(ACTIVATED_RECT_TRANSFORM),
                          rect_transform_preemption_strategy,
                          gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::DEACTIVATED: {
      base::TimeDelta visible_duration =
          GetAnimationDuration(DEACTIVATED_TRANSFORM) -
          GetAnimationDuration(DEACTIVATED_FADE_OUT);
      AnimateToOpacity(kVisibleOpacity, visible_duration,
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      CalculateRectTransforms(large_size_, large_corner_radius_, &transforms);
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(DEACTIVATED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      CalculateRectTransforms(large_size_, large_corner_radius_, &transforms);
      AnimateToTransforms(transforms,
                          GetAnimationDuration(DEACTIVATED_TRANSFORM),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
  }
}

void InkDropAnimation::AnimateToTransforms(
    const InkDropTransforms transforms,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
    ui::LayerAnimator* animator = painted_layers_[i]->GetAnimator();
    ui::ScopedLayerAnimationSettings animation(animator);
    animation.SetPreemptionStrategy(preemption_strategy);
    animation.SetTweenType(tween);
    ui::LayerAnimationElement* element =
        ui::LayerAnimationElement::CreateTransformElement(transforms[i],
                                                          duration);
    ui::LayerAnimationSequence* sequence =
        new ui::LayerAnimationSequence(element);

    if (animation_observer)
      sequence->AddObserver(animation_observer);

    animator->StartAnimation(sequence);
  }
}

void InkDropAnimation::SetStateToHidden() {
  InkDropTransforms transforms;
  // Using a size of 0x0 creates visual anomalies.
  CalculateCircleTransforms(gfx::Size(1, 1), &transforms);
  SetTransforms(transforms);
  SetOpacity(kHiddenOpacity);
  root_layer_->SetVisible(false);
}

void InkDropAnimation::SetTransforms(const InkDropTransforms transforms) {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    painted_layers_[i]->SetTransform(transforms[i]);
}

float InkDropAnimation::GetCurrentOpacity() const {
  return root_layer_->opacity();
}

void InkDropAnimation::SetOpacity(float opacity) {
  root_layer_->SetOpacity(opacity);
}

void InkDropAnimation::AnimateToOpacity(
    float opacity,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = root_layer_->GetAnimator();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  animation_settings.SetPreemptionStrategy(preemption_strategy);
  animation_settings.SetTweenType(tween);
  ui::LayerAnimationElement* animation_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  ui::LayerAnimationSequence* animation_sequence =
      new ui::LayerAnimationSequence(animation_element);

  if (animation_observer)
    animation_sequence->AddObserver(animation_observer);

  animator->StartAnimation(animation_sequence);
}

void InkDropAnimation::CalculateCircleTransforms(
    const gfx::Size& size,
    InkDropTransforms* transforms_out) const {
  CalculateRectTransforms(size, std::min(size.width(), size.height()) / 2.0f,
                          transforms_out);
}

void InkDropAnimation::CalculateRectTransforms(
    const gfx::Size& size,
    float corner_radius,
    InkDropTransforms* transforms_out) const {
  DCHECK_GE(size.width() / 2.0f, corner_radius)
      << "The circle's diameter should not be greater than the total width.";
  DCHECK_GE(size.height() / 2.0f, corner_radius)
      << "The circle's diameter should not be greater than the total height.";

  // The shapes are drawn such that their center points are not at the origin.
  // Thus we use the CalculateCircleTransform() and CalculateRectTransform()
  // methods to calculate the complex Transforms.

  const float circle_scale = std::max(
      kMinimumCircleScale,
      corner_radius / static_cast<float>(circle_layer_delegate_->radius()));

  const float circle_target_x_offset = size.width() / 2.0f - corner_radius;
  const float circle_target_y_offset = size.height() / 2.0f - corner_radius;

  (*transforms_out)[TOP_LEFT_CIRCLE] = CalculateCircleTransform(
      ToRoundedPoint(circle_layer_delegate_->GetCenterPoint()), circle_scale,
      -circle_target_x_offset, -circle_target_y_offset);

  (*transforms_out)[TOP_RIGHT_CIRCLE] = CalculateCircleTransform(
      ToRoundedPoint(circle_layer_delegate_->GetCenterPoint()), circle_scale,
      circle_target_x_offset, -circle_target_y_offset);

  (*transforms_out)[BOTTOM_RIGHT_CIRCLE] = CalculateCircleTransform(
      ToRoundedPoint(circle_layer_delegate_->GetCenterPoint()), circle_scale,
      circle_target_x_offset, circle_target_y_offset);

  (*transforms_out)[BOTTOM_LEFT_CIRCLE] = CalculateCircleTransform(
      ToRoundedPoint(circle_layer_delegate_->GetCenterPoint()), circle_scale,
      -circle_target_x_offset, circle_target_y_offset);

  const float rect_delegate_width =
      static_cast<float>(rect_layer_delegate_->size().width());
  const float rect_delegate_height =
      static_cast<float>(rect_layer_delegate_->size().height());

  (*transforms_out)[HORIZONTAL_RECT] = CalculateRectTransform(
      ToRoundedPoint(rect_layer_delegate_->GetCenterPoint()),
      std::max(kMinimumRectScale, size.width() / rect_delegate_width),
      std::max(kMinimumRectScale,
               (size.height() - 2.0f * corner_radius) / rect_delegate_height));

  (*transforms_out)[VERTICAL_RECT] = CalculateRectTransform(
      ToRoundedPoint(rect_layer_delegate_->GetCenterPoint()),
      std::max(kMinimumRectScale,
               (size.width() - 2.0f * corner_radius) / rect_delegate_width),
      std::max(kMinimumRectScale, size.height() / rect_delegate_height));
}

void InkDropAnimation::GetCurrentTransforms(
    InkDropTransforms* transforms_out) const {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    (*transforms_out)[i] = painted_layers_[i]->transform();
}

void InkDropAnimation::AddPaintLayer(PaintedShape painted_shape) {
  ui::LayerDelegate* delegate = nullptr;
  switch (painted_shape) {
    case TOP_LEFT_CIRCLE:
    case TOP_RIGHT_CIRCLE:
    case BOTTOM_RIGHT_CIRCLE:
    case BOTTOM_LEFT_CIRCLE:
      delegate = circle_layer_delegate_.get();
      break;
    case HORIZONTAL_RECT:
    case VERTICAL_RECT:
      delegate = rect_layer_delegate_.get();
      break;
    case PAINTED_SHAPE_COUNT:
      NOTREACHED() << "PAINTED_SHAPE_COUNT is not an actual shape type.";
      break;
  }

  ui::Layer* layer = new ui::Layer();
  root_layer_->Add(layer);

  layer->SetBounds(gfx::Rect(large_size_));
  layer->SetFillsBoundsOpaquely(false);
  layer->set_delegate(delegate);
  layer->SetVisible(true);
  layer->SetOpacity(1.0);
  layer->SetMasksToBounds(false);
  layer->set_name("PAINTED_SHAPE_COUNT:" + ToLayerName(painted_shape));

  painted_layers_[painted_shape].reset(layer);
}

void InkDropAnimation::AbortAllAnimations() {
  root_layer_->GetAnimator()->AbortAllAnimations();
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    painted_layers_[i]->GetAnimator()->AbortAllAnimations();
}

void InkDropAnimation::AnimationStartedCallback(
    InkDropState ink_drop_state,
    const ui::CallbackLayerAnimationObserver& observer) {
  FOR_EACH_OBSERVER(InkDropAnimationObserver, observers_,
                    InkDropAnimationStarted(ink_drop_state));
}

bool InkDropAnimation::AnimationEndedCallback(
    InkDropState ink_drop_state,
    const ui::CallbackLayerAnimationObserver& observer) {
  if (ink_drop_state == InkDropState::HIDDEN)
    SetStateToHidden();

  FOR_EACH_OBSERVER(
      InkDropAnimationObserver, observers_,
      InkDropAnimationEnded(ink_drop_state,
                            observer.aborted_count()
                                ? InkDropAnimationObserver::PRE_EMPTED
                                : InkDropAnimationObserver::SUCCESS));
  return true;
}

}  // namespace views

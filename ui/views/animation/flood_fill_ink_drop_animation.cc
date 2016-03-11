// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_animation.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace {

// The minimum radius to use when scaling the painted layers. Smaller values
// were causing visual anomalies.
const float kMinRadius = 0.01f;

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

  // The HIDDEN sub animation that transform the circle to a small one.
  HIDDEN_TRANSFORM,

  // ACTION_PENDING sub animations.

  // The ACTION_PENDING sub animation that fades in to the visible opacity.
  ACTION_PENDING_FADE_IN,

  // The ACTION_PENDING sub animation that transforms the circle to fill the
  // bounds.
  ACTION_PENDING_TRANSFORM,

  // QUICK_ACTION sub animations.

  // The QUICK_ACTION sub animation that is fading out to a hidden opacity.
  QUICK_ACTION_FADE_OUT,

  // SLOW_ACTION_PENDING sub animations.

  // The SLOW_ACTION_PENDING animation has only one sub animation which animates
  // the circleto fill the bounds at visible opacity.
  SLOW_ACTION_PENDING,

  // SLOW_ACTION sub animations.

  // The SLOW_ACTION sub animation that is fading out to a hidden opacity.
  SLOW_ACTION_FADE_OUT,

  // ACTIVATED sub animations.

  // The ACTIVATED sub animation that is fading in to the visible opacity.
  ACTIVATED_FADE_IN,

  // The ACTIVATED sub animation that transforms the circle to fill the entire
  // bounds.
  ACTIVATED_TRANSFORM,

  // DEACTIVATED sub animations.

  // The DEACTIVATED sub animation that is fading out to a hidden opacity.
  DEACTIVATED_FADE_OUT,
};

// Duration constants for InkDropStateSubAnimations. See the
// InkDropStateSubAnimations enum documentation for more info.
int kAnimationDurationInMs[] = {
    200,  // HIDDEN_FADE_OUT
    300,  // HIDDEN_TRANSFORM
    0,    // ACTION_PENDING_FADE_IN
    240,  // ACTION_PENDING_TRANSFORM
    300,  // QUICK_ACTION_FADE_OUT
    200,  // SLOW_ACTION_PENDING
    300,  // SLOW_ACTION_FADE_OUT
    150,  // ACTIVATED_FADE_IN
    200,  // ACTIVATED_TRANSFORM
    300,  // DEACTIVATED_FADE_OUT
};

// Returns the InkDropState sub animation duration for the given |state|.
base::TimeDelta GetAnimationDuration(InkDropSubAnimations state) {
  return base::TimeDelta::FromMilliseconds(
      (views::InkDropAnimation::UseFastAnimations()
           ? 1
           : views::InkDropAnimation::kSlowAnimationDurationFactor) *
      kAnimationDurationInMs[state]);
}

// Calculates the largest distance from |point| to the corners of a rectangle
// with origin (0, 0) and the given |size|.
float CalculateLargestDistanceToCorners(const gfx::Size& size,
                                        const gfx::Point& point) {
  const float top_left_distance = gfx::Vector2dF(point.x(), point.y()).Length();
  const float top_right_distance =
      gfx::Vector2dF(size.width() - point.x(), point.y()).Length();
  const float bottom_left_distance =
      gfx::Vector2dF(point.x(), size.height() - point.y()).Length();
  const float bottom_right_distance =
      gfx::Vector2dF(size.width() - point.x(), size.height() - point.y())
          .Length();

  float largest_distance = std::max(top_left_distance, top_right_distance);
  largest_distance = std::max(largest_distance, bottom_left_distance);
  largest_distance = std::max(largest_distance, bottom_right_distance);
  return largest_distance;
}

}  // namespace

namespace views {

FloodFillInkDropAnimation::FloodFillInkDropAnimation(
    const gfx::Size& size,
    const gfx::Point& center_point,
    SkColor color)
    : size_(size),
      center_point_(center_point),
      root_layer_(ui::LAYER_NOT_DRAWN),
      circle_layer_delegate_(color,
                             std::max(size_.width(), size_.height()) / 2.f),
      ink_drop_state_(InkDropState::HIDDEN) {
  root_layer_.set_name("FloodFillInkDropAnimation:ROOT_LAYER");
  root_layer_.SetMasksToBounds(true);
  root_layer_.SetBounds(gfx::Rect(size_));

  const int painted_size_length = 2 * std::max(size_.width(), size_.height());

  painted_layer_.SetBounds(gfx::Rect(painted_size_length, painted_size_length));
  painted_layer_.SetFillsBoundsOpaquely(false);
  painted_layer_.set_delegate(&circle_layer_delegate_);
  painted_layer_.SetVisible(true);
  painted_layer_.SetOpacity(1.0);
  painted_layer_.SetMasksToBounds(false);
  painted_layer_.set_name("FloodFillInkDropAnimation:PAINTED_LAYER");

  root_layer_.Add(&painted_layer_);

  SetStateToHidden();
}

FloodFillInkDropAnimation::~FloodFillInkDropAnimation() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  AbortAllAnimations();
}

void FloodFillInkDropAnimation::SnapToActivated() {
  InkDropAnimation::SnapToActivated();
  SetOpacity(kVisibleOpacity);
  painted_layer_.SetTransform(GetActivatedTargetTransform());
}

ui::Layer* FloodFillInkDropAnimation::GetRootLayer() {
  return &root_layer_;
}

bool FloodFillInkDropAnimation::IsVisible() const {
  return root_layer_.visible();
}

void FloodFillInkDropAnimation::AnimateStateChange(
    InkDropState old_ink_drop_state,
    InkDropState new_ink_drop_state,
    ui::LayerAnimationObserver* animation_observer) {
  switch (new_ink_drop_state) {
    case InkDropState::HIDDEN:
      if (!IsVisible()) {
        SetStateToHidden();
        break;
      } else {
        AnimateToOpacity(kHiddenOpacity, GetAnimationDuration(HIDDEN_FADE_OUT),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
        const gfx::Transform transform = CalculateTransform(kMinRadius);
        AnimateToTransform(transform, GetAnimationDuration(HIDDEN_TRANSFORM),
                           ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                           gfx::Tween::EASE_IN_OUT, animation_observer);
      }
      break;
    case InkDropState::ACTION_PENDING: {
      DCHECK(old_ink_drop_state == InkDropState::HIDDEN);

      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ACTION_PENDING_FADE_IN),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ACTION_PENDING_TRANSFORM) -
                           GetAnimationDuration(ACTION_PENDING_FADE_IN),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN, animation_observer);

      const gfx::Transform transform = CalculateTransform(
          CalculateLargestDistanceToCorners(size_, center_point_));
      AnimateToTransform(transform,
                         GetAnimationDuration(ACTION_PENDING_TRANSFORM),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::FAST_OUT_SLOW_IN, animation_observer);
      break;
    }
    case InkDropState::QUICK_ACTION: {
      DCHECK(old_ink_drop_state == InkDropState::HIDDEN ||
             old_ink_drop_state == InkDropState::ACTION_PENDING);
      if (old_ink_drop_state == InkDropState::HIDDEN) {
        AnimateStateChange(old_ink_drop_state, InkDropState::ACTION_PENDING,
                           animation_observer);
      }
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(QUICK_ACTION_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::SLOW_ACTION_PENDING: {
      DCHECK(old_ink_drop_state == InkDropState::ACTION_PENDING);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(SLOW_ACTION_PENDING),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      const gfx::Transform transform = CalculateTransform(
          CalculateLargestDistanceToCorners(size_, center_point_));
      AnimateToTransform(transform, GetAnimationDuration(SLOW_ACTION_PENDING),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::SLOW_ACTION:
      DCHECK(old_ink_drop_state == InkDropState::SLOW_ACTION_PENDING);
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(SLOW_ACTION_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    case InkDropState::ACTIVATED: {
      AnimateToOpacity(kVisibleOpacity, GetAnimationDuration(ACTIVATED_FADE_IN),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToTransform(GetActivatedTargetTransform(),
                         GetAnimationDuration(ACTIVATED_TRANSFORM),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::DEACTIVATED:
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(DEACTIVATED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
  }
}

void FloodFillInkDropAnimation::SetStateToHidden() {
  gfx::Transform transform = CalculateTransform(kMinRadius);
  painted_layer_.SetTransform(transform);
  root_layer_.SetOpacity(InkDropAnimation::kHiddenOpacity);
  root_layer_.SetVisible(false);
}

void FloodFillInkDropAnimation::AbortAllAnimations() {
  root_layer_.GetAnimator()->AbortAllAnimations();
  painted_layer_.GetAnimator()->AbortAllAnimations();
}

void FloodFillInkDropAnimation::AnimateToTransform(
    const gfx::Transform& transform,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = painted_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(preemption_strategy);
  animation.SetTweenType(tween);
  ui::LayerAnimationElement* element =
      ui::LayerAnimationElement::CreateTransformElement(transform, duration);
  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(element);

  if (animation_observer)
    sequence->AddObserver(animation_observer);

  animator->StartAnimation(sequence);
}

void FloodFillInkDropAnimation::SetOpacity(float opacity) {
  root_layer_.SetOpacity(opacity);
}

void FloodFillInkDropAnimation::AnimateToOpacity(
    float opacity,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = root_layer_.GetAnimator();
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

gfx::Transform FloodFillInkDropAnimation::CalculateTransform(
    float target_radius) const {
  const float target_scale = target_radius / circle_layer_delegate_.radius();
  const gfx::Point drawn_center_point =
      ToRoundedPoint(circle_layer_delegate_.GetCenterPoint());

  gfx::Transform transform = gfx::Transform();
  transform.Translate(center_point_.x(), center_point_.y());
  transform.Scale(target_scale, target_scale);
  transform.Translate(-drawn_center_point.x(), -drawn_center_point.y());

  return transform;
}

gfx::Transform FloodFillInkDropAnimation::GetActivatedTargetTransform() const {
  return CalculateTransform(
      CalculateLargestDistanceToCorners(size_, center_point_));
}

}  // namespace views

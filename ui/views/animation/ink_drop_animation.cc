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
#include "ui/gfx/geometry/point_conversions.h"
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

// The opacity of the ink drop when it is visible.
const float kVisibleOpacity = 0.14f;

// The opacity of the ink drop when it is not visible.
const float kHiddenOpacity = 0.0f;

// Durations for the different InkDropState animations in milliseconds.
const int kHiddenStateAnimationDurationMs = 1;
const int kActionPendingStateAnimationDurationMs = 500;
const int kQuickActionStateAnimationDurationMs = 250;
const int kSlowActionPendingStateAnimationDurationMs = 500;
const int kSlowActionStateAnimationDurationMs = 250;
const int kActivatedStateAnimationDurationMs = 125;
const int kDeactivatedStateAnimationDurationMs = 250;

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

// Returns the InkDropState animation duration for the given |state|.
base::TimeDelta GetAnimationDuration(views::InkDropState state) {
  int duration = 0;
  switch (state) {
    case views::InkDropState::HIDDEN:
      duration = kHiddenStateAnimationDurationMs;
      break;
    case views::InkDropState::ACTION_PENDING:
      duration = kActionPendingStateAnimationDurationMs;
      break;
    case views::InkDropState::QUICK_ACTION:
      duration = kQuickActionStateAnimationDurationMs;
      break;
    case views::InkDropState::SLOW_ACTION_PENDING:
      duration = kSlowActionPendingStateAnimationDurationMs;
      break;
    case views::InkDropState::SLOW_ACTION:
      duration = kSlowActionStateAnimationDurationMs;
      break;
    case views::InkDropState::ACTIVATED:
      duration = kActivatedStateAnimationDurationMs;
      break;
    case views::InkDropState::DEACTIVATED:
      duration = kDeactivatedStateAnimationDurationMs;
      break;
  }

  return base::TimeDelta::FromMilliseconds(
      (UseFastAnimations() ? 1 : kSlowAnimationDurationFactor) * duration);
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
  ink_drop_state_ = ink_drop_state;

  if (ink_drop_state_ == InkDropState::HIDDEN) {
    // Animating to the HIDDEN state doesn't actually use any
    // LayerAnimationSequences so we need to explicitly abort any running ones
    // so that observers receive an InkDropAnimationEnded() event for the
    // running animation prior to receiving an InkDropAnimationStarted() event
    // for the HIDDEN 'animation'.
    AbortAllAnimations();
    root_layer_->SetVisible(false);
    SetStateToHidden();
    return;
  }

  InkDropTransforms transforms;
  root_layer_->SetVisible(true);

  switch (ink_drop_state_) {
    case InkDropState::HIDDEN:
      // This case is handled above in a short circuit return.
      break;
    case InkDropState::ACTION_PENDING:
      CalculateCircleTransforms(large_size_, &transforms);
      AnimateToTransforms(transforms, kVisibleOpacity,
                          GetAnimationDuration(InkDropState::ACTION_PENDING),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          animation_observer);
      break;
    case InkDropState::QUICK_ACTION:
      CalculateCircleTransforms(large_size_, &transforms);
      AnimateToTransforms(transforms, kHiddenOpacity,
                          GetAnimationDuration(InkDropState::QUICK_ACTION),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          animation_observer);
      break;
    case InkDropState::SLOW_ACTION_PENDING:
      CalculateRectTransforms(small_size_, small_corner_radius_, &transforms);
      AnimateToTransforms(
          transforms, kVisibleOpacity,
          GetAnimationDuration(InkDropState::SLOW_ACTION_PENDING),
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
          animation_observer);
      break;
    case InkDropState::SLOW_ACTION:
      CalculateRectTransforms(large_size_, large_corner_radius_, &transforms);
      AnimateToTransforms(transforms, kHiddenOpacity,
                          GetAnimationDuration(InkDropState::SLOW_ACTION),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          animation_observer);
      break;
    case InkDropState::ACTIVATED:
      CalculateRectTransforms(small_size_, small_corner_radius_, &transforms);
      AnimateToTransforms(transforms, kVisibleOpacity,
                          GetAnimationDuration(InkDropState::ACTIVATED),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          animation_observer);
      break;
    case InkDropState::DEACTIVATED:
      CalculateRectTransforms(large_size_, large_corner_radius_, &transforms);
      AnimateToTransforms(transforms, kHiddenOpacity,
                          GetAnimationDuration(InkDropState::DEACTIVATED),
                          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                          animation_observer);
      break;
  }
}

void InkDropAnimation::AnimateToTransforms(
    const InkDropTransforms transforms,
    float opacity,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* root_animator = root_layer_->GetAnimator();
  ui::ScopedLayerAnimationSettings root_animation(root_animator);
  root_animation.SetPreemptionStrategy(preemption_strategy);
  ui::LayerAnimationElement* root_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  ui::LayerAnimationSequence* root_sequence =
      new ui::LayerAnimationSequence(root_element);

  if (animation_observer)
    root_sequence->AddObserver(animation_observer);

  root_animator->StartAnimation(root_sequence);

  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
    ui::LayerAnimator* animator = painted_layers_[i]->GetAnimator();
    ui::ScopedLayerAnimationSettings animation(animator);
    animation.SetPreemptionStrategy(preemption_strategy);
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
}

void InkDropAnimation::SetTransforms(const InkDropTransforms transforms) {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    painted_layers_[i]->SetTransform(transforms[i]);
}

void InkDropAnimation::SetOpacity(float opacity) {
  root_layer_->SetOpacity(opacity);
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

void InkDropAnimation::GetCurrentTansforms(
    InkDropTransforms* transforms_out) const {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    (*transforms_out)[i] = painted_layers_[i]->GetTargetTransform();
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
  FOR_EACH_OBSERVER(
      InkDropAnimationObserver, observers_,
      InkDropAnimationEnded(ink_drop_state,
                            observer.aborted_count()
                                ? InkDropAnimationObserver::PRE_EMPTED
                                : InkDropAnimationObserver::SUCCESS));
  return true;
}

}  // namespace views

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_highlight.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/animation/ink_drop_highlight_observer.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/animation/ink_drop_util.h"

namespace views {

namespace {

// The opacity of the highlight when it is not visible.
constexpr float kHiddenOpacity = 0.0f;

}  // namespace

std::string ToString(InkDropHighlight::AnimationType animation_type) {
  switch (animation_type) {
    case InkDropHighlight::AnimationType::kFadeIn:
      return std::string("FADE_IN");
    case InkDropHighlight::AnimationType::kFadeOut:
      return std::string("FADE_OUT");
  }
}

InkDropHighlight::InkDropHighlight(
    const gfx::PointF& center_point,
    std::unique_ptr<BasePaintedLayerDelegate> layer_delegate)
    : center_point_(center_point),
      layer_delegate_(std::move(layer_delegate)),
      layer_(std::make_unique<ui::Layer>()) {
  const gfx::RectF painted_bounds = layer_delegate_->GetPaintedBounds();
  size_ = painted_bounds.size();

  layer_->SetBounds(gfx::ToEnclosingRect(painted_bounds));
  layer_->SetFillsBoundsOpaquely(false);
  layer_->set_delegate(layer_delegate_.get());
  layer_->SetVisible(false);
  layer_->SetMasksToBounds(false);
  layer_->SetName("InkDropHighlight:layer");
}

InkDropHighlight::InkDropHighlight(const gfx::SizeF& size,
                                   int corner_radius,
                                   const gfx::PointF& center_point,
                                   SkColor color)
    : InkDropHighlight(
          center_point,
          std::make_unique<RoundedRectangleLayerDelegate>(color,
                                                          size,
                                                          corner_radius)) {
  layer_->SetOpacity(visible_opacity_);
}

InkDropHighlight::InkDropHighlight(const gfx::Size& size,
                                   int corner_radius,
                                   const gfx::PointF& center_point,
                                   SkColor color)
    : InkDropHighlight(gfx::SizeF(size), corner_radius, center_point, color) {}

InkDropHighlight::InkDropHighlight(const gfx::SizeF& size, SkColor base_color)
    : size_(size), layer_(std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR)) {
  layer_->SetColor(base_color);
  layer_->SetBounds(gfx::Rect(gfx::ToRoundedSize(size)));
  layer_->SetVisible(false);
  layer_->SetMasksToBounds(false);
  layer_->SetOpacity(visible_opacity_);
  layer_->SetName("InkDropHighlight:solid_color_layer");
}

InkDropHighlight::~InkDropHighlight() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  layer_->GetAnimator()->AbortAllAnimations();
}

bool InkDropHighlight::IsFadingInOrVisible() const {
  return last_animation_initiated_was_fade_in_;
}

void InkDropHighlight::FadeIn(const base::TimeDelta& duration) {
  layer_->SetOpacity(kHiddenOpacity);
  layer_->SetVisible(true);
  AnimateFade(AnimationType::kFadeIn, duration);
}

void InkDropHighlight::FadeOut(const base::TimeDelta& duration) {
  AnimateFade(AnimationType::kFadeOut, duration);
}

test::InkDropHighlightTestApi* InkDropHighlight::GetTestApi() {
  return nullptr;
}

void InkDropHighlight::AnimateFade(AnimationType animation_type,
                                   const base::TimeDelta& duration) {
  const base::TimeDelta effective_duration =
      gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                  : base::TimeDelta();
  last_animation_initiated_was_fade_in_ =
      animation_type == AnimationType::kFadeIn;

  layer_->SetTransform(CalculateTransform());

  // The |animation_observer| will be destroyed when the
  // AnimationStartedCallback() returns true.
  ui::CallbackLayerAnimationObserver* animation_observer =
      new ui::CallbackLayerAnimationObserver(
          base::BindRepeating(&InkDropHighlight::AnimationStartedCallback,
                              base::Unretained(this), animation_type),
          base::BindRepeating(&InkDropHighlight::AnimationEndedCallback,
                              base::Unretained(this), animation_type));

  ui::LayerAnimator* animator = layer_->GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  std::unique_ptr<ui::LayerAnimationElement> opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(
          animation_type == AnimationType::kFadeIn ? visible_opacity_
                                                   : kHiddenOpacity,
          effective_duration);
  ui::LayerAnimationSequence* opacity_sequence =
      new ui::LayerAnimationSequence(std::move(opacity_element));
  opacity_sequence->AddObserver(animation_observer);
  animator->StartAnimation(opacity_sequence);

  animation_observer->SetActive();
}

gfx::Transform InkDropHighlight::CalculateTransform() const {
  gfx::Transform transform;
  // No transform needed for a solid color layer.
  if (!layer_delegate_)
    return transform;

  transform.Translate(center_point_.x(), center_point_.y());
  gfx::Vector2dF layer_offset = layer_delegate_->GetCenteringOffset();
  transform.Translate(-layer_offset.x(), -layer_offset.y());

  // Add subpixel correction to the transform.
  transform.ConcatTransform(
      GetTransformSubpixelCorrection(transform, layer_->device_scale_factor()));

  return transform;
}

void InkDropHighlight::AnimationStartedCallback(
    AnimationType animation_type,
    const ui::CallbackLayerAnimationObserver& observer) {
  if (observer_)
    observer_->AnimationStarted(animation_type);
}

bool InkDropHighlight::AnimationEndedCallback(
    AnimationType animation_type,
    const ui::CallbackLayerAnimationObserver& observer) {
  // AnimationEndedCallback() may be invoked when this is being destroyed and
  // |layer_| may be null.
  if (animation_type == AnimationType::kFadeOut && layer_)
    layer_->SetVisible(false);

  if (observer_) {
    observer_->AnimationEnded(animation_type,
                              observer.aborted_count()
                                  ? InkDropAnimationEndedReason::PRE_EMPTED
                                  : InkDropAnimationEndedReason::SUCCESS);
  }
  return true;
}

}  // namespace views

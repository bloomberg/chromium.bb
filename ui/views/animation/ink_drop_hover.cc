// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_hover.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

namespace views {

namespace {

// The opacity of the hover when it is visible.
const float kHoverVisibleOpacity = 0.128f;

// The opacity of the hover when it is not visible.
const float kHiddenOpacity = 0.0f;

}  // namespace

InkDropHover::InkDropHover(const gfx::Size& size,
                           int corner_radius,
                           const gfx::Point& center_point,
                           SkColor color)
    : last_animation_initiated_was_fade_in_(false),
      layer_delegate_(
          new RoundedRectangleLayerDelegate(color, size, corner_radius)),
      layer_(new ui::Layer()) {
  layer_->SetBounds(gfx::Rect(size));
  layer_->SetFillsBoundsOpaquely(false);
  layer_->set_delegate(layer_delegate_.get());
  layer_->SetVisible(false);
  layer_->SetOpacity(kHoverVisibleOpacity);
  layer_->SetMasksToBounds(false);
  layer_->set_name("InkDropHover:layer");

  gfx::Transform transform;
  transform.Translate(center_point.x() - layer_->bounds().CenterPoint().x(),
                      center_point.y() - layer_->bounds().CenterPoint().y());
  layer_->SetTransform(transform);
}

InkDropHover::~InkDropHover() {}

bool InkDropHover::IsFadingInOrVisible() const {
  return last_animation_initiated_was_fade_in_;
}

void InkDropHover::FadeIn(const base::TimeDelta& duration) {
  layer_->SetOpacity(kHiddenOpacity);
  layer_->SetVisible(true);
  AnimateFade(FADE_IN, duration);
}

void InkDropHover::FadeOut(const base::TimeDelta& duration) {
  AnimateFade(FADE_OUT, duration);
}

void InkDropHover::AnimateFade(HoverAnimationType animation_type,
                               const base::TimeDelta& duration) {
  last_animation_initiated_was_fade_in_ = animation_type == FADE_IN;

  // The |animation_observer| will be destroyed when the
  // AnimationStartedCallback() returns true.
  ui::CallbackLayerAnimationObserver* animation_observer =
      new ui::CallbackLayerAnimationObserver(
          base::Bind(&InkDropHover::AnimationEndedCallback,
                     base::Unretained(this), animation_type));

  ui::LayerAnimator* animator = layer_->GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  ui::LayerAnimationElement* animation_element =
      ui::LayerAnimationElement::CreateOpacityElement(
          animation_type == FADE_IN ? kHoverVisibleOpacity : kHiddenOpacity,
          duration);
  ui::LayerAnimationSequence* animation_sequence =
      new ui::LayerAnimationSequence(animation_element);
  animation_sequence->AddObserver(animation_observer);

  animator->StartAnimation(animation_sequence);

  animation_observer->SetActive();
}

bool InkDropHover::AnimationEndedCallback(
    HoverAnimationType animation_type,
    const ui::CallbackLayerAnimationObserver& observer) {
  // AnimationEndedCallback() may be invoked when this is being destroyed and
  // |layer_| may be null.
  if (animation_type == FADE_OUT && layer_)
    layer_->SetVisible(false);
  return true;
}

}  // namespace views

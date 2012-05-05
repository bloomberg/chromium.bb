// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_element.h"

#include "base/compiler_specific.h"
#include "ui/base/animation/tween.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/interpolated_transform.h"

namespace ui {

namespace {

// Pause -----------------------------------------------------------------------
class Pause : public LayerAnimationElement {
 public:
  Pause(const AnimatableProperties& properties, base::TimeDelta duration)
      : LayerAnimationElement(properties, duration) {
  }
  virtual ~Pause() {}

 private:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {}
  virtual bool OnProgress(double t,
                          LayerAnimationDelegate* delegate) OVERRIDE {
    return false;
  }
  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {}
  virtual void OnAbort() OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(Pause);
};

// TransformTransition ---------------------------------------------------------

class TransformTransition : public LayerAnimationElement {
 public:
  TransformTransition(const Transform& target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        target_(target) {
  }
  virtual ~TransformTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetTransformForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(
        Tween::ValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::TRANSFORM);
    return properties;
  }

  Transform start_;
  const Transform target_;

  DISALLOW_COPY_AND_ASSIGN(TransformTransition);
};

// InterpolatedTransformTransition ---------------------------------------------

class InterpolatedTransformTransition : public LayerAnimationElement {
 public:
  InterpolatedTransformTransition(InterpolatedTransform* interpolated_transform,
                                  base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        interpolated_transform_(interpolated_transform) {
  }
  virtual ~InterpolatedTransformTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(
        interpolated_transform_->Interpolate(static_cast<float>(t)));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = interpolated_transform_->Interpolate(1.0f);
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::TRANSFORM);
    return properties;
  }

  scoped_ptr<InterpolatedTransform> interpolated_transform_;

  DISALLOW_COPY_AND_ASSIGN(InterpolatedTransformTransition);
};

// BoundsTransition ------------------------------------------------------------

class BoundsTransition : public LayerAnimationElement {
 public:
  BoundsTransition(const gfx::Rect& target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        target_(target) {
  }
  virtual ~BoundsTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetBoundsForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetBoundsFromAnimation(Tween::ValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->bounds = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::BOUNDS);
    return properties;
  }

  gfx::Rect start_;
  const gfx::Rect target_;

  DISALLOW_COPY_AND_ASSIGN(BoundsTransition);
};

// OpacityTransition -----------------------------------------------------------

class OpacityTransition : public LayerAnimationElement {
 public:
  OpacityTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~OpacityTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetOpacityForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetOpacityFromAnimation(Tween::ValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->opacity = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::OPACITY);
    return properties;
  }

  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(OpacityTransition);
};

// VisibilityTransition --------------------------------------------------------

class VisibilityTransition : public LayerAnimationElement {
 public:
  VisibilityTransition(bool target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        start_(false),
        target_(target) {
  }
  virtual ~VisibilityTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetVisibilityForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetVisibilityFromAnimation(t == 1.0 ? target_ : start_);
    return t == 1.0;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->visibility = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::VISIBILITY);
    return properties;
  }

  bool start_;
  const bool target_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityTransition);
};

}  // namespace

// LayerAnimationElement::TargetValue ------------------------------------------

LayerAnimationElement::TargetValue::TargetValue()
    : opacity(0.0f),
      visibility(false) {
}

LayerAnimationElement::TargetValue::TargetValue(
    const LayerAnimationDelegate* delegate)
    : bounds(delegate ? delegate->GetBoundsForAnimation() : gfx::Rect()),
      transform(delegate ? delegate->GetTransformForAnimation() : Transform()),
      opacity(delegate ? delegate->GetOpacityForAnimation() : 0.0f),
      visibility(delegate ? delegate->GetVisibilityForAnimation() : false) {
}

// LayerAnimationElement -------------------------------------------------------

LayerAnimationElement::LayerAnimationElement(
    const AnimatableProperties& properties,
    base::TimeDelta duration)
    : first_frame_(true),
      properties_(properties),
      duration_(LayerAnimator::disable_animations_for_test()
          ? base::TimeDelta() : duration),
      tween_type_(Tween::LINEAR) {
}

LayerAnimationElement::~LayerAnimationElement() {
}

bool LayerAnimationElement::Progress(double t,
                                     LayerAnimationDelegate* delegate) {
  if (first_frame_)
    OnStart(delegate);
  bool need_draw = OnProgress(Tween::CalculateValue(tween_type_, t), delegate);
  first_frame_ = t == 1.0;
  return need_draw;
}

void LayerAnimationElement::GetTargetValue(TargetValue* target) const {
  OnGetTarget(target);
}

void LayerAnimationElement::Abort() {
  first_frame_ = true;
  OnAbort();
}

// static
LayerAnimationElement* LayerAnimationElement::CreateTransformElement(
    const Transform& transform, base::TimeDelta duration) {
  return new TransformTransition(transform, duration);
}

// static
LayerAnimationElement*
LayerAnimationElement::CreateInterpolatedTransformElement(
    InterpolatedTransform* interpolated_transform, base::TimeDelta duration) {
  return new InterpolatedTransformTransition(interpolated_transform, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateBoundsElement(
    const gfx::Rect& bounds, base::TimeDelta duration) {
  return new BoundsTransition(bounds, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateOpacityElement(
    float opacity, base::TimeDelta duration) {
  return new OpacityTransition(opacity, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateVisibilityElement(
    bool visibility, base::TimeDelta duration) {
  return new VisibilityTransition(visibility, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreatePauseElement(
    const AnimatableProperties& properties, base::TimeDelta duration) {
  return new Pause(properties, duration);
}

}  // namespace ui

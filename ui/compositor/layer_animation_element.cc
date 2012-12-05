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
    TransformTransition(const gfx::Transform& target, base::TimeDelta duration)
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

  gfx::Transform start_;
  const gfx::Transform target_;

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

// BrightnessTransition --------------------------------------------------------

class BrightnessTransition : public LayerAnimationElement {
 public:
  BrightnessTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~BrightnessTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetBrightnessForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetBrightnessFromAnimation(
        Tween::ValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->brightness = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::BRIGHTNESS);
    return properties;
  }

  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessTransition);
};

// GrayscaleTransition ---------------------------------------------------------

class GrayscaleTransition : public LayerAnimationElement {
 public:
  GrayscaleTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~GrayscaleTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetGrayscaleForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetGrayscaleFromAnimation(
        Tween::ValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->grayscale = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::GRAYSCALE);
    return properties;
  }

  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(GrayscaleTransition);
};

// ColorTransition -------------------------------------------------------------

class ColorTransition : public LayerAnimationElement {
 public:
  ColorTransition(SkColor target, base::TimeDelta duration)
      : LayerAnimationElement(GetProperties(), duration),
        start_(SK_ColorBLACK),
        target_(target) {
  }
  virtual ~ColorTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetColorForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetColorFromAnimation(
        SkColorSetARGB(
            Tween::ValueBetween(t,
                                static_cast<int>(SkColorGetA(start_)),
                                static_cast<int>(SkColorGetA(target_))),
            Tween::ValueBetween(t,
                                static_cast<int>(SkColorGetR(start_)),
                                static_cast<int>(SkColorGetR(target_))),
            Tween::ValueBetween(t,
                                static_cast<int>(SkColorGetG(start_)),
                                static_cast<int>(SkColorGetG(target_))),
            Tween::ValueBetween(t,
                                static_cast<int>(SkColorGetB(start_)),
                                static_cast<int>(SkColorGetB(target_)))));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->color = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static AnimatableProperties GetProperties() {
    AnimatableProperties properties;
    properties.insert(LayerAnimationElement::COLOR);
    return properties;
  }

  SkColor start_;
  const SkColor target_;

  DISALLOW_COPY_AND_ASSIGN(ColorTransition);
};

}  // namespace

// LayerAnimationElement::TargetValue ------------------------------------------

LayerAnimationElement::TargetValue::TargetValue()
    : opacity(0.0f),
      visibility(false),
      brightness(0.0f),
      grayscale(0.0f),
      color(SK_ColorBLACK) {
}

LayerAnimationElement::TargetValue::TargetValue(
    const LayerAnimationDelegate* delegate)
    : bounds(delegate ? delegate->GetBoundsForAnimation() : gfx::Rect()),
      transform(delegate ?
                delegate->GetTransformForAnimation() : gfx::Transform()),
      opacity(delegate ? delegate->GetOpacityForAnimation() : 0.0f),
      visibility(delegate ? delegate->GetVisibilityForAnimation() : false),
      brightness(delegate ? delegate->GetBrightnessForAnimation() : 0.0f),
      grayscale(delegate ? delegate->GetGrayscaleForAnimation() : 0.0f),
      color(delegate ? delegate->GetColorForAnimation() : 0.0f) {
}

// LayerAnimationElement -------------------------------------------------------

LayerAnimationElement::LayerAnimationElement(
    const AnimatableProperties& properties,
    base::TimeDelta duration)
    : first_frame_(true),
      properties_(properties),
      duration_(GetEffectiveDuration(duration)),
      tween_type_(Tween::LINEAR) {
}

LayerAnimationElement::~LayerAnimationElement() {
}

bool LayerAnimationElement::Progress(base::TimeDelta elapsed,
                                     LayerAnimationDelegate* delegate) {
  if (first_frame_)
    OnStart(delegate);
  double t = 1.0;
  if ((duration_ > base::TimeDelta()) && (elapsed < duration_))
    t = elapsed.InMillisecondsF() / duration_.InMillisecondsF();
  bool need_draw = OnProgress(Tween::CalculateValue(tween_type_, t), delegate);
  first_frame_ = t == 1.0;
  return need_draw;
}

bool LayerAnimationElement::IsFinished(base::TimeDelta elapsed,
                                       base::TimeDelta* total_duration) {
  if (elapsed >= duration_) {
    *total_duration = duration_;
    return true;
  }
  return false;
}

bool LayerAnimationElement::ProgressToEnd(LayerAnimationDelegate* delegate) {
  return Progress(duration_, delegate);
}

void LayerAnimationElement::GetTargetValue(TargetValue* target) const {
  OnGetTarget(target);
}

void LayerAnimationElement::Abort() {
  first_frame_ = true;
  OnAbort();
}

// static
base::TimeDelta LayerAnimationElement::GetEffectiveDuration(
    const base::TimeDelta& duration) {
  if (LayerAnimator::disable_animations_for_test())
    return base::TimeDelta();

  if (LayerAnimator::slow_animation_mode())
    return duration * LayerAnimator::slow_animation_scale_factor();

  return duration;
}

// static
LayerAnimationElement* LayerAnimationElement::CreateTransformElement(
    const gfx::Transform& transform,
    base::TimeDelta duration) {
  return new TransformTransition(transform, duration);
}

// static
LayerAnimationElement*
LayerAnimationElement::CreateInterpolatedTransformElement(
    InterpolatedTransform* interpolated_transform,
    base::TimeDelta duration) {
  return new InterpolatedTransformTransition(interpolated_transform, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateBoundsElement(
    const gfx::Rect& bounds,
    base::TimeDelta duration) {
  return new BoundsTransition(bounds, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateOpacityElement(
    float opacity,
    base::TimeDelta duration) {
  return new OpacityTransition(opacity, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateVisibilityElement(
    bool visibility,
    base::TimeDelta duration) {
  return new VisibilityTransition(visibility, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateBrightnessElement(
    float brightness,
    base::TimeDelta duration) {
  return new BrightnessTransition(brightness, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateGrayscaleElement(
    float grayscale,
    base::TimeDelta duration) {
  return new GrayscaleTransition(grayscale, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreatePauseElement(
    const AnimatableProperties& properties,
    base::TimeDelta duration) {
  return new Pause(properties, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateColorElement(
    SkColor color,
    base::TimeDelta duration) {
  return new ColorTransition(color, duration);
}

}  // namespace ui

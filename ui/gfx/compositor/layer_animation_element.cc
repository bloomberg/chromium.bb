// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer_animation_element.h"

#include "base/compiler_specific.h"
#include "ui/base/animation/tween.h"
#include "ui/gfx/compositor/layer_animation_delegate.h"

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
  virtual void OnProgress(double t,
                          LayerAnimationDelegate* delegate) OVERRIDE {}
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

  virtual void OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(
        Tween::ValueBetween(t, start_, target_));
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static const AnimatableProperties& GetProperties() {
    static AnimatableProperties properties;
    if (properties.size() == 0)
      properties.insert(LayerAnimationElement::TRANSFORM);
    return properties;
  }

  Transform start_;
  const Transform target_;

  DISALLOW_COPY_AND_ASSIGN(TransformTransition);
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

  virtual void OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetBoundsFromAnimation(Tween::ValueBetween(t, start_, target_));
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->bounds = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static const AnimatableProperties& GetProperties() {
    static AnimatableProperties properties;
    if (properties.size() == 0)
      properties.insert(LayerAnimationElement::BOUNDS);
    return properties;
  }

  gfx::Rect start_;
  const gfx::Rect target_;

  DISALLOW_COPY_AND_ASSIGN(BoundsTransition);
};

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

  virtual void OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetOpacityFromAnimation(Tween::ValueBetween(t, start_, target_));
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->opacity = target_;
  }

  virtual void OnAbort() OVERRIDE {}

 private:
  static const AnimatableProperties& GetProperties() {
    static AnimatableProperties properties;
    if (properties.size() == 0)
      properties.insert(LayerAnimationElement::OPACITY);
    return properties;
  }

  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(OpacityTransition);
};

}  // namespace

// LayerAnimationElement::TargetValue ------------------------------------------

LayerAnimationElement::TargetValue::TargetValue()
    : opacity(0.0f) {
}

LayerAnimationElement::TargetValue::TargetValue(
    const LayerAnimationDelegate* delegate)
    : bounds(delegate ? delegate->GetBoundsForAnimation() : gfx::Rect()),
      transform(delegate ? delegate->GetTransformForAnimation() : Transform()),
      opacity(delegate ? delegate->GetOpacityForAnimation() : 0.0f) {
}

// LayerAnimationElement -------------------------------------------------------

LayerAnimationElement::LayerAnimationElement(
    const AnimatableProperties& properties,
    base::TimeDelta duration)
    : first_frame_(true),
      properties_(properties),
      duration_(duration) {
}

LayerAnimationElement::~LayerAnimationElement() {
}

void LayerAnimationElement::Progress(double t,
                                     LayerAnimationDelegate* delegate) {
  if (first_frame_)
    OnStart(delegate);
  OnProgress(t, delegate);
  delegate->ScheduleDrawForAnimation();
  first_frame_ = t == 1.0;
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
LayerAnimationElement* LayerAnimationElement::CreatePauseElement(
    const AnimatableProperties& properties, base::TimeDelta duration) {
  return new Pause(properties, duration);
}

}  // namespace ui

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_layer_animation_settings.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"

namespace {

const int kDefaultTransitionDurationMs = 200;

}  // namespace

namespace ui {

// InvertingObserver -----------------------------------------------------------
class InvertingObserver : public ImplicitAnimationObserver {
  public:
    InvertingObserver()
      : base_layer_(NULL) {
    }

    virtual ~InvertingObserver() {}

    void SetLayer(Layer* base_layer) { base_layer_ = base_layer; }

    Layer* layer() { return base_layer_; }

    void AddInverselyAnimatedLayer(Layer* inverse_layer) {
      inverse_layers_.push_back(inverse_layer);
    }

    virtual void OnImplicitAnimationsCompleted() OVERRIDE {}

    virtual void OnLayerAnimationScheduled(
        LayerAnimationSequence* sequence) OVERRIDE {
      DCHECK(base_layer_  != NULL)
        << "Must set base layer with ScopedLayerAnimationSettings::"
        << "SetInverslyAnimatedBaseLayer";
      gfx::Transform base_transform = base_layer_->transform();
      scoped_ptr<LayerAnimationElement> inverse = GetInverseElement(sequence,
          base_transform);

      for (std::vector<Layer*>::const_iterator i =
          inverse_layers_.begin(); i != inverse_layers_.end(); ++i) {
        (*i)->GetAnimator()->StartAnimation(new LayerAnimationSequence(
            LayerAnimationElement::CloneInverseTransformElement(
                inverse.get())));
      }
    }
  private:
    scoped_ptr<LayerAnimationElement> GetInverseElement(
        LayerAnimationSequence* sequence,
        gfx::Transform base) const {
      const size_t expected_size = 1;
      DCHECK_EQ(expected_size, sequence->size()) <<
        "Inverse supported only for single element sequences.";

      LayerAnimationElement* element = sequence->FirstElement();
      LayerAnimationElement::AnimatableProperties transform_property;
      transform_property.insert(LayerAnimationElement::TRANSFORM);
      DCHECK(transform_property == element->properties())
        << "Only transform animations are currently invertible.";

      scoped_ptr<LayerAnimationElement> to_return(
          LayerAnimationElement::CreateInverseTransformElement(base, element));
      return to_return.Pass();
    }

    Layer* base_layer_;
    // child layers
    std::vector<Layer*> inverse_layers_;
};


// ScoperLayerAnimationSettings ------------------------------------------------
ScopedLayerAnimationSettings::ScopedLayerAnimationSettings(
    LayerAnimator* animator)
    : animator_(animator),
      old_transition_duration_(animator->transition_duration_),
      old_tween_type_(animator->tween_type()),
      old_preemption_strategy_(animator->preemption_strategy()),
      inverse_observer_(new InvertingObserver()) {
  SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kDefaultTransitionDurationMs));
}

ScopedLayerAnimationSettings::~ScopedLayerAnimationSettings() {
  animator_->transition_duration_ = old_transition_duration_;
  animator_->set_tween_type(old_tween_type_);
  animator_->set_preemption_strategy(old_preemption_strategy_);

  for (std::set<ImplicitAnimationObserver*>::const_iterator i =
       observers_.begin(); i != observers_.end(); ++i) {
    animator_->observers_.RemoveObserver(*i);
    (*i)->SetActive(true);
  }

  if (inverse_observer_->layer()) {
    animator_->observers_.RemoveObserver(inverse_observer_.get());
  }
}

void ScopedLayerAnimationSettings::AddObserver(
    ImplicitAnimationObserver* observer) {
  observers_.insert(observer);
  animator_->AddObserver(observer);
}

void ScopedLayerAnimationSettings::SetTransitionDuration(
    base::TimeDelta duration) {
  animator_->transition_duration_ = duration;
}

base::TimeDelta ScopedLayerAnimationSettings::GetTransitionDuration() const {
  return animator_->transition_duration_;
}

void ScopedLayerAnimationSettings::SetTweenType(gfx::Tween::Type tween_type) {
  animator_->set_tween_type(tween_type);
}

gfx::Tween::Type ScopedLayerAnimationSettings::GetTweenType() const {
  return animator_->tween_type();
}

void ScopedLayerAnimationSettings::SetPreemptionStrategy(
    LayerAnimator::PreemptionStrategy strategy) {
  animator_->set_preemption_strategy(strategy);
}

LayerAnimator::PreemptionStrategy
ScopedLayerAnimationSettings::GetPreemptionStrategy() const {
  return animator_->preemption_strategy();
}

void ScopedLayerAnimationSettings::SetInverselyAnimatedBaseLayer(Layer* base) {
  if (inverse_observer_->layer() && !base) {
      animator_->RemoveObserver(inverse_observer_.get());
  } else if (base && !(inverse_observer_->layer())) {
      animator_->AddObserver(inverse_observer_.get());
  }
  inverse_observer_->SetLayer(base);
}

void ScopedLayerAnimationSettings::AddInverselyAnimatedLayer(
    Layer* inverse_layer) {
  inverse_observer_->AddInverselyAnimatedLayer(inverse_layer);
}

}  // namespace ui

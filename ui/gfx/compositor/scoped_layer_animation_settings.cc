// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/scoped_layer_animation_settings.h"

#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/compositor/layer_animator.h"

namespace {

static const base::TimeDelta kDefaultTransitionDuration =
    base::TimeDelta::FromMilliseconds(200);

} // namespace;

namespace ui {

ScopedLayerAnimationSettings::ScopedLayerAnimationSettings(
    LayerAnimator* animator)
    : animator_(animator),
      old_transition_duration_(animator->transition_duration_) {
  SetTransitionDuration(kDefaultTransitionDuration);
}

ScopedLayerAnimationSettings::~ScopedLayerAnimationSettings() {
  animator_->transition_duration_ = old_transition_duration_;

  for (std::set<LayerAnimationObserver*>::const_iterator i =
       observers_.begin(); i != observers_.end(); ++i)
      animator_->observers_.RemoveObserver(*i);

  for (std::set<ImplicitAnimationObserver*>::const_iterator i =
       implicit_observers_.begin(); i != implicit_observers_.end(); ++i)
      (*i)->SetActive(true);
}

void ScopedLayerAnimationSettings::AddObserver(
    LayerAnimationObserver* observer) {
  observers_.insert(observer);
  animator_->AddObserver(observer);
}

void ScopedLayerAnimationSettings::AddImplicitObserver(
    ImplicitAnimationObserver* observer) {
  implicit_observers_.insert(observer);
  AddObserver(observer);
}

void ScopedLayerAnimationSettings::SetTransitionDuration(
    base::TimeDelta duration) {
  animator_->transition_duration_ = duration;
}

}  // namespace ui


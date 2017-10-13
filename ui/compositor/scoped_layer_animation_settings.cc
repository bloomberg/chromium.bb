// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_layer_animation_settings.h"

#include <stddef.h>

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"

namespace {

const int kDefaultTransitionDurationMs = 200;

class CacheRenderSurfaceObserver : public ui::ImplicitAnimationObserver,
                                   public ui::LayerObserver {
 public:
  CacheRenderSurfaceObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    layer_->AddCacheRenderSurfaceRequest();
  }
  ~CacheRenderSurfaceObserver() override {
    if (layer_)
      layer_->RemoveObserver(this);
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override {
    // If animation finishes before |layer_| is destoyed, we will reset the
    // cache and remove |this| from the |layer_| observer list when deleting
    // |this|.
    if (layer_) {
      layer_->RemoveCacheRenderSurfaceRequest();
      layer_->GetAnimator()->RemoveAndDestroyOwnedObserver(this);
    }
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    // If the animation is still going past layer destruction then we want the
    // layer too keep being cached until the animation has finished. We will
    // defer deleting |this| until the animation finishes.
    layer_->RemoveObserver(this);
    layer_ = nullptr;
  }

 private:
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(CacheRenderSurfaceObserver);
};

class DeferredPaintObserver : public ui::ImplicitAnimationObserver,
                              public ui::LayerObserver {
 public:
  DeferredPaintObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    layer_->AddDeferredPaintRequest();
  }
  ~DeferredPaintObserver() override {
    if (layer_)
      layer_->RemoveObserver(this);
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override {
    // If animation finishes before |layer_| is destoyed, we will remove the
    // request of deferred painting and remove |this| from the |layer_|
    // observer list when deleting |this|.
    if (layer_) {
      layer_->RemoveDeferredPaintRequest();
      layer_->GetAnimator()->RemoveAndDestroyOwnedObserver(this);
    }
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    // If the animation is still going past layer destruction then we want the
    // layer too keep being deferred paint until the animation has finished. We
    // will defer deleting |this| until the animation finishes.
    layer_->RemoveObserver(this);
    layer_ = nullptr;
  }

 private:
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(DeferredPaintObserver);
};

class TrilinearFilteringObserver : public ui::ImplicitAnimationObserver,
                                   public ui::LayerObserver {
 public:
  TrilinearFilteringObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    layer_->AddTrilinearFilteringRequest();
  }
  ~TrilinearFilteringObserver() override {
    if (layer_)
      layer_->RemoveObserver(this);
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override {
    // If animation finishes before |layer_| is destoyed, we will remove the
    // trilinear filtering request and remove |this| from the |layer_| observer
    // list when deleting |this|.
    if (layer_) {
      layer_->RemoveTrilinearFilteringRequest();
      layer_->GetAnimator()->RemoveAndDestroyOwnedObserver(this);
    }
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    // If the animation is still going past layer destruction then we want the
    // layer too keep being filtered until the animation has finished. We will
    // defer deleting |this| until the animation finishes.
    layer_->RemoveObserver(this);
    layer_ = nullptr;
  }

 private:
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(TrilinearFilteringObserver);
};

void AddDeferredPaintObserverRecursive(
    ui::Layer* layer,
    ui::ScopedLayerAnimationSettings* settings) {
  auto observer = base::MakeUnique<DeferredPaintObserver>(layer);
  settings->AddObserver(observer.get());
  settings->GetAnimator()->AddOwnedObserver(std::move(observer));
  for (auto* child : layer->children())
    AddDeferredPaintObserverRecursive(child, settings);
}

}  // namespace

namespace ui {

// ScopedLayerAnimationSettings ------------------------------------------------
ScopedLayerAnimationSettings::ScopedLayerAnimationSettings(
    scoped_refptr<LayerAnimator> animator)
    : animator_(animator),
      old_is_transition_duration_locked_(
          animator->is_transition_duration_locked_),
      old_transition_duration_(animator->GetTransitionDuration()),
      old_tween_type_(animator->tween_type()),
      old_preemption_strategy_(animator->preemption_strategy()) {
  SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kDefaultTransitionDurationMs));
}

ScopedLayerAnimationSettings::~ScopedLayerAnimationSettings() {
  animator_->set_animation_metrics_reporter(nullptr);
  animator_->is_transition_duration_locked_ =
      old_is_transition_duration_locked_;
  animator_->SetTransitionDuration(old_transition_duration_);
  animator_->set_tween_type(old_tween_type_);
  animator_->set_preemption_strategy(old_preemption_strategy_);

  for (std::set<ImplicitAnimationObserver*>::const_iterator i =
       observers_.begin(); i != observers_.end(); ++i) {
    animator_->observers_.RemoveObserver(*i);
    (*i)->SetActive(true);
  }
}

void ScopedLayerAnimationSettings::AddObserver(
    ImplicitAnimationObserver* observer) {
  observers_.insert(observer);
  animator_->AddObserver(observer);
}

void ScopedLayerAnimationSettings::SetAnimationMetricsReporter(
    AnimationMetricsReporter* reporter) {
  animator_->set_animation_metrics_reporter(reporter);
}

void ScopedLayerAnimationSettings::SetTransitionDuration(
    base::TimeDelta duration) {
  animator_->SetTransitionDuration(duration);
}

base::TimeDelta ScopedLayerAnimationSettings::GetTransitionDuration() const {
  return animator_->GetTransitionDuration();
}

void ScopedLayerAnimationSettings::LockTransitionDuration() {
  animator_->is_transition_duration_locked_ = true;
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

void ScopedLayerAnimationSettings::CacheRenderSurface() {
  auto observer = base::MakeUnique<CacheRenderSurfaceObserver>(
      animator_->delegate()->GetLayer());
  AddObserver(observer.get());
  animator_->AddOwnedObserver(std::move(observer));
}

void ScopedLayerAnimationSettings::DeferPaint() {
  AddDeferredPaintObserverRecursive(animator_->delegate()->GetLayer(), this);
}

void ScopedLayerAnimationSettings::TrilinearFiltering() {
  auto observer = base::MakeUnique<TrilinearFilteringObserver>(
      animator_->delegate()->GetLayer());
  AddObserver(observer.get());
  animator_->AddOwnedObserver(std::move(observer));
}

}  // namespace ui

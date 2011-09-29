// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer_animator.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/base/animation/animation_container.h"
#include "ui/base/animation/animation.h"
#include "ui/base/animation/tween.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator_delegate.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/rect.h"

namespace {

void SetMatrixElement(SkMatrix44& matrix, int index, SkMScalar value) {
  int row = index / 4;
  int col = index % 4;
  matrix.set(row, col, value);
}

SkMScalar GetMatrixElement(const SkMatrix44& matrix, int index) {
  int row = index / 4;
  int col = index % 4;
  return matrix.get(row, col);
}

} // anonymous namespace

namespace ui {

LayerAnimator::LayerAnimator(Layer* layer)
    : layer_(layer),
      got_initial_tick_(false) {
}

LayerAnimator::~LayerAnimator() {
}

void LayerAnimator::SetAnimation(Animation* animation) {
  animation_.reset(animation);
  if (animation_.get()) {
    static ui::AnimationContainer* container = NULL;
    if (!container) {
      container = new AnimationContainer;
      container->AddRef();
    }
    animation_->set_delegate(this);
    animation_->SetContainer(container);
    got_initial_tick_ = false;
  }
}

void LayerAnimator::AnimateToPoint(const gfx::Point& target) {
  StopAnimating(LOCATION);
  const gfx::Rect& layer_bounds = layer_->bounds();
  if (target == layer_bounds.origin())
    return;  // Already there.

  Params& element = elements_[LOCATION];
  element.location.target_x = target.x();
  element.location.target_y = target.y();
  element.location.start_x = layer_bounds.origin().x();
  element.location.start_y = layer_bounds.origin().y();
}

void LayerAnimator::AnimateTransform(const Transform& transform) {
  StopAnimating(TRANSFORM);
  const Transform& layer_transform = layer_->transform();
  if (transform == layer_transform)
    return;  // Already there.

  Params& element = elements_[TRANSFORM];
  for (int i = 0; i < 16; ++i) {
    element.transform.start[i] =
        GetMatrixElement(layer_transform.matrix(), i);
    element.transform.target[i] =
        GetMatrixElement(transform.matrix(), i);
  }
}

void LayerAnimator::AnimateOpacity(float target_opacity) {
  StopAnimating(OPACITY);
  if (layer_->opacity() == target_opacity)
    return;

  Params& element = elements_[OPACITY];
  element.opacity.start = layer_->opacity();
  element.opacity.target = target_opacity;
}

gfx::Point LayerAnimator::GetTargetPoint() {
  return IsAnimating(LOCATION) ?
      gfx::Point(elements_[LOCATION].location.target_x,
                 elements_[LOCATION].location.target_y) :
      layer_->bounds().origin();
}

float LayerAnimator::GetTargetOpacity() {
  return IsAnimating(OPACITY) ?
      elements_[OPACITY].opacity.target : layer_->opacity();
}

ui::Transform LayerAnimator::GetTargetTransform() {
  if (IsAnimating(TRANSFORM)) {
    Transform transform;
    for (int i = 0; i < 16; ++i) {
      SetMatrixElement(transform.matrix(), i,
                       elements_[TRANSFORM].transform.target[i]);
    }
    return transform;
  }
  return layer_->transform();
}

bool LayerAnimator::IsAnimating(AnimationProperty property) const {
  return elements_.count(property) > 0;
}

bool LayerAnimator::IsRunning() const {
  return animation_.get() && animation_->is_animating();
}

void LayerAnimator::AnimationProgressed(const ui::Animation* animation) {
  got_initial_tick_ = true;
  for (Elements::const_iterator i = elements_.begin(); i != elements_.end();
       ++i) {
    switch (i->first) {
      case LOCATION: {
        const gfx::Rect& current_bounds(layer_->bounds());
        gfx::Rect new_bounds = animation_->CurrentValueBetween(
            gfx::Rect(gfx::Point(i->second.location.start_x,
                                 i->second.location.start_y),
                      current_bounds.size()),
            gfx::Rect(gfx::Point(i->second.location.target_x,
                                 i->second.location.target_y),
                      current_bounds.size()));
        delegate()->SetBoundsFromAnimator(new_bounds);
        break;
      }

      case TRANSFORM: {
        Transform transform;
        for (int j = 0; j < 16; ++j) {
          SkMScalar value = animation_->CurrentValueBetween(
              i->second.transform.start[j],
              i->second.transform.target[j]);
          SetMatrixElement(transform.matrix(), j, value);
        }
        delegate()->SetTransformFromAnimator(transform);
        break;
      }

      case OPACITY: {
        delegate()->SetOpacityFromAnimator(animation_->CurrentValueBetween(
            i->second.opacity.start, i->second.opacity.target));
        break;
      }

      default:
        NOTREACHED();
    }
  }
  layer_->GetCompositor()->SchedulePaint();
}

void LayerAnimator::AnimationEnded(const ui::Animation* animation) {
  AnimationProgressed(animation);
}

void LayerAnimator::StopAnimating(AnimationProperty property) {
  if (!IsAnimating(property))
    return;

  elements_.erase(property);
}

LayerAnimatorDelegate* LayerAnimator::delegate() {
  return static_cast<LayerAnimatorDelegate*>(layer_);
}

}  // namespace ui

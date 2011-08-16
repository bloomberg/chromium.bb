// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/tween.h"
#include "ui/gfx/point.h"

namespace ui {

class Layer;
class MultiAnimation;
class Transform;

// LayerAnimator manages animating various properties of a Layer.
class LayerAnimator : public ui::AnimationDelegate {
 public:
  explicit LayerAnimator(Layer* layer);
  virtual ~LayerAnimator();

  ui::Layer* layer() { return layer_; }

  // Sets the duration (in ms) and type of animation. This does not effect
  // existing animations, only newly created animations.
  void SetAnimationDurationAndType(int duration, ui::Tween::Type tween_type);

  // Animates the layer to the specified point. The point is relative to the
  // parent layer.
  void AnimateToPoint(const gfx::Point& target);
  void StopAnimatingToPoint() {
    StopAnimating(LOCATION);
  }

  // Animates the transform from from the current transform to |transform|.
  void AnimateTransform(const Transform& transform);
  void StopAnimatingTransform() {
    StopAnimating(TRANSFORM);
  }

  // AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const Animation* animation) OVERRIDE;

 private:
  // Types of properties that can be animated.
  enum AnimationProperty {
    LOCATION,
    TRANSFORM
  };

  // Parameters used when animating the location.
  struct LocationParams {
    int start_x;
    int start_y;
    int target_x;
    int target_y;
  };

  // Parameters used whe animating the transform.
  struct TransformParams {
    // TODO: make 4x4 whe Transform is updated.
    SkMScalar start[16];
    SkMScalar target[16];
  };

  union Params {
    LocationParams location;
    TransformParams transform;
  };

  // Used for tracking the animation of a particular property.
  struct Element {
    Params params;
    ui::MultiAnimation* animation;
  };

  typedef std::map<AnimationProperty, Element> Elements;

  // Stops animating the specified property. This does not set the property
  // being animated to its final value.
  void StopAnimating(AnimationProperty property);

  // Creates an animation.
  ui::MultiAnimation* CreateAndStartAnimation();

  // Returns an iterator into |elements_| that matches the specified animation.
  Elements::iterator GetElementByAnimation(const ui::MultiAnimation* animation);

  // The layer.
  Layer* layer_;

  // Properties being animated.
  Elements elements_;

  // Duration in ms for newly created animations.
  int duration_in_ms_;

  // Type of animation for newly created animations.
  ui::Tween::Type animation_type_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimator);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_

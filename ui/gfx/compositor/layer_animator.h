// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/compositor/compositor_export.h"

namespace gfx {
class Point;
}

namespace ui {

class Animation;
class Layer;
class LayerAnimatorDelegate;
class Transform;

// LayerAnimator manages animating various properties of a Layer.
class COMPOSITOR_EXPORT LayerAnimator : public ui::AnimationDelegate {
 public:
  // Types of properties that can be animated.
  enum AnimationProperty {
    LOCATION,
    OPACITY,
    TRANSFORM,
  };

  explicit LayerAnimator(Layer* layer);
  virtual ~LayerAnimator();

  // Sets the animation to use. LayerAnimator takes ownership of the animation.
  void SetAnimation(Animation* animation);

  ui::Layer* layer() { return layer_; }

  // Animates the layer to the specified point. The point is relative to the
  // parent layer.
  void AnimateToPoint(const gfx::Point& target);

  // Animates the transform from the current transform to |transform|.
  void AnimateTransform(const Transform& transform);

  // Animates the opacity from the current opacity to |target_opacity|.
  void AnimateOpacity(float target_opacity);

  // Returns the target value for the specified type. If the specified property
  // is not animating, the current value is returned.
  gfx::Point GetTargetPoint();
  float GetTargetOpacity();
  ui::Transform GetTargetTransform();

  // Returns true if animating |property|.
  bool IsAnimating(AnimationProperty property) const;

  // Returns true if the animation is running.
  bool IsRunning() const;

  // Returns true if the animation has progressed at least once since
  // SetAnimation() was invoked.
  bool got_initial_tick() const { return got_initial_tick_; }

  // AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const Animation* animation) OVERRIDE;

 private:
  // Parameters used when animating the location.
  struct LocationParams {
    int start_x;
    int start_y;
    int target_x;
    int target_y;
  };

  // Parameters used when animating the transform.
  struct TransformParams {
    SkMScalar start[16];
    SkMScalar target[16];
  };

  // Parameters used when animating the opacity.
  struct OpacityParams {
    float start;
    float target;
  };

  union Params {
    LocationParams location;
    OpacityParams opacity;
    TransformParams transform;
  };

  typedef std::map<AnimationProperty, Params> Elements;

  // Stops animating the specified property. This does not set the property
  // being animated to its final value.
  void StopAnimating(AnimationProperty property);

  LayerAnimatorDelegate* delegate();

  // The layer.
  Layer* layer_;

  // Properties being animated.
  Elements elements_;

  scoped_ptr<ui::Animation> animation_;

  bool got_initial_tick_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimator);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_

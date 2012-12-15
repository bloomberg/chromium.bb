// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATION_ELEMENT_H_
#define UI_COMPOSITOR_LAYER_ANIMATION_ELEMENT_H_

#include <set>

#include "base/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/tween.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

class InterpolatedTransform;
class LayerAnimationDelegate;

// LayerAnimationElements represent one segment of an animation between two
// keyframes. They know how to update a LayerAnimationDelegate given a value
// between 0 and 1 (0 for initial, and 1 for final).
class COMPOSITOR_EXPORT LayerAnimationElement {
 public:
  enum AnimatableProperty {
    TRANSFORM = 0,
    BOUNDS,
    OPACITY,
    VISIBILITY,
    BRIGHTNESS,
    GRAYSCALE,
    COLOR,
  };

  struct COMPOSITOR_EXPORT TargetValue {
    TargetValue();
    // Initializes the target value to match the delegate. NULL may be supplied.
    explicit TargetValue(const LayerAnimationDelegate* delegate);

    gfx::Rect bounds;
    gfx::Transform transform;
    float opacity;
    bool visibility;
    float brightness;
    float grayscale;
    SkColor color;
  };

  typedef std::set<AnimatableProperty> AnimatableProperties;

  LayerAnimationElement(const AnimatableProperties& properties,
                        base::TimeDelta duration);
  virtual ~LayerAnimationElement();

  // Creates an element that transitions to the given transform. The caller owns
  // the return value.
  static LayerAnimationElement* CreateTransformElement(
      const gfx::Transform& transform,
      base::TimeDelta duration);

  // Creates an element that transitions to another in a way determined by an
  // interpolated transform. The element accepts ownership of the interpolated
  // transform. NB: at every step, the interpolated transform clobbers the
  // existing transform. That is, it does not interpolate between the existing
  // transform and the last value the interpolated transform will assume. It is
  // therefore important that the value of the interpolated at time 0 matches
  // the current transform.
  static LayerAnimationElement* CreateInterpolatedTransformElement(
      InterpolatedTransform* interpolated_transform,
      base::TimeDelta duration);

  // Creates an element that transitions to the given bounds. The caller owns
  // the return value.
  static LayerAnimationElement* CreateBoundsElement(
      const gfx::Rect& bounds,
      base::TimeDelta duration);

  // Creates an element that transitions to the given opacity. The caller owns
  // the return value.
  static LayerAnimationElement* CreateOpacityElement(
      float opacity,
      base::TimeDelta duration);

  // Creates an element that sets visibily following a delay. The caller owns
  // the return value.
  static LayerAnimationElement* CreateVisibilityElement(
      bool visibility,
      base::TimeDelta duration);

  // Creates an element that transitions to the given brightness.
  // The caller owns the return value.
  static LayerAnimationElement* CreateBrightnessElement(
      float brightness,
      base::TimeDelta duration);

  // Creates an element that transitions to the given grayscale value.
  // The caller owns the return value.
  static LayerAnimationElement* CreateGrayscaleElement(
      float grayscale,
      base::TimeDelta duration);

  // Creates an element that pauses the given properties. The caller owns the
  // return value.
  static LayerAnimationElement* CreatePauseElement(
      const AnimatableProperties& properties,
      base::TimeDelta duration);

  // Creates an element that transitions to the given color. The caller owns the
  // return value.
  static LayerAnimationElement* CreateColorElement(
      SkColor color,
      base::TimeDelta duration);

  // Sets the start time for the animation. This must be called before the first
  // call to {Progress, IsFinished}. Once the animation is finished, this must
  // be called again in order to restart the animation.
  void set_start_time(base::TimeTicks start_time) { start_time_ = start_time; }
  base::TimeTicks start_time() const { return start_time_; }

  // Updates the delegate to the appropriate value for |now|. Returns true
  // if a redraw is required.
  bool Progress(base::TimeTicks now, LayerAnimationDelegate* delegate);

  // If calling Progress now, with the given time, will finish the animation,
  // returns true and sets |end_duration| to the actual duration for this
  // animation, incuding any queueing delays.
  bool IsFinished(base::TimeTicks time, base::TimeDelta* total_duration);

  // Updates the delegate to the end of the animation. Returns true if a
  // redraw is required.
  bool ProgressToEnd(LayerAnimationDelegate* delegate);

  // Called if the animation is not allowed to complete. This may be called
  // before OnStarted or Progress.
  void Abort();

  // Assigns the target value to |target|.
  void GetTargetValue(TargetValue* target) const;

  // The properties that the element modifies.
  const AnimatableProperties& properties() const { return properties_; }

  Tween::Type tween_type() const { return tween_type_; }
  void set_tween_type(Tween::Type tween_type) { tween_type_ = tween_type; }

 protected:
  // Called once each time the animation element is run before any call to
  // OnProgress.
  virtual void OnStart(LayerAnimationDelegate* delegate) = 0;
  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) = 0;
  virtual void OnGetTarget(TargetValue* target) const = 0;
  virtual void OnAbort() = 0;

 private:
  // For debugging purposes, we sometimes alter the duration we actually use.
  // For example, during tests we often set duration = 0, and it is sometimes
  // useful to slow animations down to see them more clearly.
  base::TimeDelta GetEffectiveDuration(const base::TimeDelta& delta);

  bool first_frame_;
  const AnimatableProperties properties_;
  base::TimeTicks start_time_;
  const base::TimeDelta duration_;
  Tween::Type tween_type_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimationElement);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATION_ELEMENT_H_

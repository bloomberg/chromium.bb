// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_

#include <memory>

#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

class TimingFunction;

// ScrollOffsetAnimationCurve computes scroll offset as a function of time
// during a scroll offset animation.
//
// Scroll offset animations can run either in Blink or in cc, in response to
// user input or programmatic scroll operations.  For more information about
// scheduling and servicing scroll animations, see blink::ScrollAnimator and
// blink::ProgrammaticScrollAnimator.

class CC_ANIMATION_EXPORT ScrollOffsetAnimationCurve : public AnimationCurve {
 public:
  // Indicates how the animation duration should be computed for Ease-in-out
  // style scroll animation curves.
  enum class DurationBehavior {
    // Duration proportional to scroll delta; used for programmatic scrolls.
    DELTA_BASED,
    // Constant duration; used for keyboard scrolls.
    CONSTANT,
    // Duration inversely proportional to scroll delta within certain bounds.
    // Used for mouse wheels, makes fast wheel flings feel "snappy" while
    // preserving smoothness of slow wheel movements.
    INVERSE_DELTA
  };

  // There is inherent delay in input processing; it may take many milliseconds
  // from the time of user input to when when we're actually able to handle it
  // here. This delay is represented by the |delayed_by| value. The way we have
  // decided to factor this in is by reducing the duration of the resulting
  // animation by this delayed amount. This also applies to
  // LinearSegmentDuration and ImpulseSegmentDuration.
  static base::TimeDelta EaseInOutSegmentDuration(
      const gfx::Vector2dF& delta,
      DurationBehavior duration_behavior,
      base::TimeDelta delayed_by);

  static base::TimeDelta LinearSegmentDuration(const gfx::Vector2dF& delta,
                                               base::TimeDelta delayed_by,
                                               float velocity);

  static base::TimeDelta ImpulseSegmentDuration(const gfx::Vector2dF& delta,
                                                base::TimeDelta delayed_by);

  ScrollOffsetAnimationCurve(const ScrollOffsetAnimationCurve&) = delete;
  ~ScrollOffsetAnimationCurve() override;

  ScrollOffsetAnimationCurve& operator=(const ScrollOffsetAnimationCurve&) =
      delete;

  // Sets the initial offset and velocity (in pixels per second).
  void SetInitialValue(const gfx::ScrollOffset& initial_value,
                       base::TimeDelta delayed_by = base::TimeDelta(),
                       float velocity = 0);
  bool HasSetInitialValue() const;
  gfx::ScrollOffset GetValue(base::TimeDelta t) const;
  gfx::ScrollOffset target_value() const { return target_value_; }

  // Updates the current curve to aim at a new target, starting at time t
  // relative to the start of the animation. The duration is recomputed based
  // on the animation type the curve was constructed with. The timing function
  // is modified to preserve velocity at t.
  void UpdateTarget(base::TimeDelta t, const gfx::ScrollOffset& new_target);

  // Shifts the entire curve by a delta without affecting its shape or timing.
  // Used for scroll anchoring adjustments that happen during scroll animations
  // (see blink::ScrollAnimator::AdjustAnimationAndSetScrollOffset).
  void ApplyAdjustment(const gfx::Vector2dF& adjustment);

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  CurveType Type() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  std::unique_ptr<ScrollOffsetAnimationCurve>
  CloneToScrollOffsetAnimationCurve() const;

  static void SetAnimationDurationForTesting(base::TimeDelta duration);

 private:
  friend class ScrollOffsetAnimationCurveFactory;
  enum class AnimationType { kLinear, kEaseInOut, kImpulse };

  // |duration_behavior| should be provided if (and only if) |animation_type| is
  // kEaseInOut.
  ScrollOffsetAnimationCurve(
      const gfx::ScrollOffset& target_value,
      AnimationType animation_type,
      base::Optional<DurationBehavior> duration_behavior = base::nullopt);
  ScrollOffsetAnimationCurve(
      const gfx::ScrollOffset& target_value,
      std::unique_ptr<TimingFunction> timing_function,
      AnimationType animation_type,
      base::Optional<DurationBehavior> duration_behavior);

  base::TimeDelta SegmentDuration(
      const gfx::Vector2dF& delta,
      base::TimeDelta delayed_by,
      base::Optional<double> velocity = base::nullopt);

  base::TimeDelta EaseInOutBoundedSegmentDuration(
      const gfx::Vector2dF& new_delta,
      base::TimeDelta t,
      base::TimeDelta delayed_by);

  // Returns the velocity at time t in units of pixels per second.
  double CalculateVelocity(base::TimeDelta t);

  gfx::ScrollOffset initial_value_;
  gfx::ScrollOffset target_value_;
  base::TimeDelta total_animation_duration_;

  // Time from animation start to most recent UpdateTarget.
  base::TimeDelta last_retarget_;

  std::unique_ptr<TimingFunction> timing_function_;
  AnimationType animation_type_;

  // Only valid when |animation_type_| is EASE_IN_OUT.
  base::Optional<DurationBehavior> duration_behavior_;

  bool has_set_initial_value_;

  static base::Optional<double> animation_duration_for_testing_;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_

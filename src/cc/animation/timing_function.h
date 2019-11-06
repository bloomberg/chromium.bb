// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TIMING_FUNCTION_H_
#define CC_ANIMATION_TIMING_FUNCTION_H_

#include <memory>

#include "cc/animation/animation_export.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace cc {

// See http://www.w3.org/TR/css3-transitions/.
class CC_ANIMATION_EXPORT TimingFunction {
 public:
  virtual ~TimingFunction();

  TimingFunction& operator=(const TimingFunction&) = delete;

  // Note that LINEAR is a nullptr TimingFunction (for now).
  enum class Type { LINEAR, CUBIC_BEZIER, STEPS };

  // Which limit to apply at a discontinuous boundary.
  enum class LimitDirection { LEFT, RIGHT };

  virtual Type GetType() const = 0;
  virtual double GetValue(double t) const = 0;
  virtual double Velocity(double time) const = 0;
  virtual std::unique_ptr<TimingFunction> Clone() const = 0;

 protected:
  TimingFunction();
};

class CC_ANIMATION_EXPORT CubicBezierTimingFunction : public TimingFunction {
 public:
  enum class EaseType { EASE, EASE_IN, EASE_OUT, EASE_IN_OUT, CUSTOM };

  static std::unique_ptr<CubicBezierTimingFunction> CreatePreset(
      EaseType ease_type);
  static std::unique_ptr<CubicBezierTimingFunction> Create(double x1,
                                                           double y1,
                                                           double x2,
                                                           double y2);
  ~CubicBezierTimingFunction() override;

  CubicBezierTimingFunction& operator=(const CubicBezierTimingFunction&) =
      delete;

  // TimingFunction implementation.
  Type GetType() const override;
  double GetValue(double time) const override;
  double Velocity(double time) const override;
  std::unique_ptr<TimingFunction> Clone() const override;

  EaseType ease_type() const { return ease_type_; }
  const gfx::CubicBezier& bezier() const { return bezier_; }

 private:
  CubicBezierTimingFunction(EaseType ease_type,
                            double x1,
                            double y1,
                            double x2,
                            double y2);

  gfx::CubicBezier bezier_;
  EaseType ease_type_;
};

class CC_ANIMATION_EXPORT StepsTimingFunction : public TimingFunction {
 public:
  // step-timing-function values
  // https://drafts.csswg.org/css-easing-1/#typedef-step-timing-function
  enum class StepPosition { START, END };

  static std::unique_ptr<StepsTimingFunction> Create(
      int steps,
      StepPosition step_position);
  ~StepsTimingFunction() override;

  StepsTimingFunction& operator=(const StepsTimingFunction&) = delete;

  // TimingFunction implementation.
  Type GetType() const override;
  double GetValue(double t) const override;
  std::unique_ptr<TimingFunction> Clone() const override;
  double Velocity(double time) const override;

  int steps() const { return steps_; }
  StepPosition step_position() const { return step_position_; }
  double GetPreciseValue(double t, LimitDirection limit_direction) const;

 private:
  StepsTimingFunction(int steps, StepPosition step_position);

  float GetStepsStartOffset() const;

  int steps_;
  StepPosition step_position_;
};

}  // namespace cc

#endif  // CC_ANIMATION_TIMING_FUNCTION_H_

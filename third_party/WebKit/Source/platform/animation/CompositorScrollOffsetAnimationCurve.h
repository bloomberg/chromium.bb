// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorScrollOffsetAnimationCurve_h
#define CompositorScrollOffsetAnimationCurve_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace cc {
class ScrollOffsetAnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorScrollOffsetAnimationCurve
    : public CompositorAnimationCurve {
  WTF_MAKE_NONCOPYABLE(CompositorScrollOffsetAnimationCurve);

 public:
  enum ScrollDurationBehavior {
    kScrollDurationDeltaBased = 0,
    kScrollDurationConstant,
    kScrollDurationInverseDelta
  };

  static std::unique_ptr<CompositorScrollOffsetAnimationCurve> Create(
      FloatPoint target_value,
      CompositorScrollOffsetAnimationCurve::ScrollDurationBehavior
          duration_behavior) {
    return WTF::WrapUnique(new CompositorScrollOffsetAnimationCurve(
        target_value, duration_behavior));
  }
  static std::unique_ptr<CompositorScrollOffsetAnimationCurve> Create(
      cc::ScrollOffsetAnimationCurve* curve) {
    return WTF::WrapUnique(new CompositorScrollOffsetAnimationCurve(curve));
  }

  ~CompositorScrollOffsetAnimationCurve() override;

  void SetInitialValue(FloatPoint);
  FloatPoint GetValue(double time) const;
  double Duration() const;
  FloatPoint TargetValue() const;
  void ApplyAdjustment(IntSize);
  void UpdateTarget(double time, FloatPoint new_target);

  // CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const override;

 private:
  CompositorScrollOffsetAnimationCurve(FloatPoint, ScrollDurationBehavior);
  CompositorScrollOffsetAnimationCurve(cc::ScrollOffsetAnimationCurve*);

  std::unique_ptr<cc::ScrollOffsetAnimationCurve> curve_;
};

}  // namespace blink

#endif  // CompositorScrollOffsetAnimationCurve_h

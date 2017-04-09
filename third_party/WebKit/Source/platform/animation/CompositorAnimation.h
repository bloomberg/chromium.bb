// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimation_h
#define CompositorAnimation_h

#include <memory>
#include "cc/animation/animation.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace cc {
class Animation;
}

namespace blink {

class CompositorAnimationCurve;
class CompositorFloatAnimationCurve;

// A compositor driven animation.
class PLATFORM_EXPORT CompositorAnimation {
  WTF_MAKE_NONCOPYABLE(CompositorAnimation);

 public:
  using Direction = cc::Animation::Direction;
  using FillMode = cc::Animation::FillMode;

  static std::unique_ptr<CompositorAnimation> Create(
      const blink::CompositorAnimationCurve& curve,
      CompositorTargetProperty::Type target,
      int group_id,
      int animation_id) {
    return WTF::WrapUnique(
        new CompositorAnimation(curve, target, animation_id, group_id));
  }

  ~CompositorAnimation();

  // An id must be unique.
  int Id() const;
  int Group() const;

  CompositorTargetProperty::Type TargetProperty() const;

  // This is the number of times that the animation will play. If this
  // value is zero the animation will not play. If it is negative, then
  // the animation will loop indefinitely.
  double Iterations() const;
  void SetIterations(double);

  double StartTime() const;
  void SetStartTime(double monotonic_time);

  double TimeOffset() const;
  void SetTimeOffset(double monotonic_time);

  Direction GetDirection() const;
  void SetDirection(Direction);

  double PlaybackRate() const;
  void SetPlaybackRate(double);

  FillMode GetFillMode() const;
  void SetFillMode(FillMode);

  double IterationStart() const;
  void SetIterationStart(double);

  std::unique_ptr<cc::Animation> ReleaseCcAnimation();

  std::unique_ptr<CompositorFloatAnimationCurve> FloatCurveForTesting() const;

 private:
  CompositorAnimation(const CompositorAnimationCurve&,
                      CompositorTargetProperty::Type,
                      int animation_id,
                      int group_id);

  std::unique_ptr<cc::Animation> animation_;
};

}  // namespace blink

#endif  // CompositorAnimation_h

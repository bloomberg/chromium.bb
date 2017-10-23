// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatAnimationCurve_h
#define CompositorFloatAnimationCurve_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace cc {
class KeyframedFloatAnimationCurve;
}

namespace blink {
class CompositorFloatKeyframe;
}

namespace blink {

// A keyframed float animation curve.
class PLATFORM_EXPORT CompositorFloatAnimationCurve
    : public CompositorAnimationCurve {
  WTF_MAKE_NONCOPYABLE(CompositorFloatAnimationCurve);

 public:
  static std::unique_ptr<CompositorFloatAnimationCurve> Create() {
    return WTF::WrapUnique(new CompositorFloatAnimationCurve());
  }

  ~CompositorFloatAnimationCurve() override;

  void AddKeyframe(const CompositorFloatKeyframe&);
  void SetTimingFunction(const TimingFunction&);
  void SetScaledDuration(double);
  float GetValue(double time) const;

  // CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const override;

  static std::unique_ptr<CompositorFloatAnimationCurve> CreateForTesting(
      std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

  using Keyframes = Vector<std::unique_ptr<CompositorFloatKeyframe>>;
  Keyframes KeyframesForTesting() const;

  scoped_refptr<TimingFunction> GetTimingFunctionForTesting() const;

 private:
  CompositorFloatAnimationCurve();
  CompositorFloatAnimationCurve(
      std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve_;
};

}  // namespace blink

#endif  // CompositorFloatAnimationCurve_h

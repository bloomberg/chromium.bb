// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterAnimationCurve_h
#define CompositorFilterAnimationCurve_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFilterKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace cc {
class KeyframedFilterAnimationCurve;
}

namespace blink {
class CompositorFilterKeyframe;
}

namespace blink {

// A keyframed filter animation curve.
class PLATFORM_EXPORT CompositorFilterAnimationCurve
    : public CompositorAnimationCurve {
  WTF_MAKE_NONCOPYABLE(CompositorFilterAnimationCurve);

 public:
  static std::unique_ptr<CompositorFilterAnimationCurve> Create() {
    return WTF::WrapUnique(new CompositorFilterAnimationCurve());
  }
  ~CompositorFilterAnimationCurve() override;

  void AddKeyframe(const CompositorFilterKeyframe&);
  void SetTimingFunction(const TimingFunction&);
  void SetScaledDuration(double);

  // blink::CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const override;

 private:
  CompositorFilterAnimationCurve();

  std::unique_ptr<cc::KeyframedFilterAnimationCurve> curve_;
};

}  // namespace blink

#endif  // CompositorFilterAnimationCurve_h

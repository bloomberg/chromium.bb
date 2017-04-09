// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatKeyframe_h
#define CompositorFloatKeyframe_h

#include "cc/animation/keyframed_animation_curve.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorKeyframe.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorFloatKeyframe : public CompositorKeyframe {
  WTF_MAKE_NONCOPYABLE(CompositorFloatKeyframe);

 public:
  CompositorFloatKeyframe(double time, float value, const TimingFunction&);
  CompositorFloatKeyframe(std::unique_ptr<cc::FloatKeyframe>);
  ~CompositorFloatKeyframe() override;

  // CompositorKeyframe implementation.
  double Time() const override;
  const cc::TimingFunction* CcTimingFunction() const override;

  float Value() { return float_keyframe_->Value(); }
  std::unique_ptr<cc::FloatKeyframe> CloneToCC() const;

 private:
  std::unique_ptr<cc::FloatKeyframe> float_keyframe_;
};

}  // namespace blink

#endif  // CompositorFloatKeyframe_h

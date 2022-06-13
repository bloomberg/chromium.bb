// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_FLOAT_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_FLOAT_KEYFRAME_H_

#include "third_party/blink/renderer/platform/animation/compositor_keyframe.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorFloatKeyframe : public CompositorKeyframe {
 public:
  CompositorFloatKeyframe(double time, float value, const TimingFunction&);
  CompositorFloatKeyframe(std::unique_ptr<gfx::FloatKeyframe>);
  CompositorFloatKeyframe(const CompositorFloatKeyframe&) = delete;
  CompositorFloatKeyframe& operator=(const CompositorFloatKeyframe&) = delete;
  ~CompositorFloatKeyframe() override;

  // CompositorKeyframe implementation.
  base::TimeDelta Time() const override;
  const gfx::TimingFunction* CcTimingFunction() const override;

  float Value() { return float_keyframe_->Value(); }
  std::unique_ptr<gfx::FloatKeyframe> CloneToCC() const;

 private:
  std::unique_ptr<gfx::FloatKeyframe> float_keyframe_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_FLOAT_KEYFRAME_H_

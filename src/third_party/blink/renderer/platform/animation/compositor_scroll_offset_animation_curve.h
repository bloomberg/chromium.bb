// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_SCROLL_OFFSET_ANIMATION_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_SCROLL_OFFSET_ANIMATION_CURVE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {
class ScrollOffsetAnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorScrollOffsetAnimationCurve
    : public CompositorAnimationCurve {
 public:
  using ScrollType = cc::ScrollOffsetAnimationCurveFactory::ScrollType;

  CompositorScrollOffsetAnimationCurve(gfx::PointF, ScrollType);
  explicit CompositorScrollOffsetAnimationCurve(
      cc::ScrollOffsetAnimationCurve*);
  CompositorScrollOffsetAnimationCurve(
      const CompositorScrollOffsetAnimationCurve&) = delete;
  CompositorScrollOffsetAnimationCurve& operator=(
      const CompositorScrollOffsetAnimationCurve&) = delete;

  ~CompositorScrollOffsetAnimationCurve() override;

  void SetInitialValue(gfx::PointF);
  gfx::PointF GetValue(double time) const;
  base::TimeDelta Duration() const;
  gfx::PointF TargetValue() const;
  void ApplyAdjustment(gfx::Vector2d);
  void UpdateTarget(base::TimeDelta time, gfx::PointF new_target);

  // CompositorAnimationCurve implementation.
  std::unique_ptr<gfx::AnimationCurve> CloneToAnimationCurve() const override;

 private:
  std::unique_ptr<cc::ScrollOffsetAnimationCurve> curve_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_SCROLL_OFFSET_ANIMATION_CURVE_H_

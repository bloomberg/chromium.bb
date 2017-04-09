/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollAnimator_h
#define ScrollAnimator_h

#include "platform/Timer.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include <memory>

namespace blink {

class CompositorAnimationTimeline;

class PLATFORM_EXPORT ScrollAnimator : public ScrollAnimatorBase {
 public:
  explicit ScrollAnimator(ScrollableArea*,
                          WTF::TimeFunction = WTF::MonotonicallyIncreasingTime);
  ~ScrollAnimator() override;

  bool HasRunningAnimation() const override;
  ScrollOffset ComputeDeltaToConsume(const ScrollOffset& delta) const override;

  ScrollResult UserScroll(ScrollGranularity,
                          const ScrollOffset& delta) override;
  void ScrollToOffsetWithoutAnimation(const ScrollOffset&) override;
  ScrollOffset DesiredTargetOffset() const override;

  // ScrollAnimatorCompositorCoordinator implementation.
  void TickAnimation(double monotonic_time) override;
  void CancelAnimation() override;
  void AdjustAnimationAndSetScrollOffset(const ScrollOffset&,
                                         ScrollType) override;
  void TakeOverCompositorAnimation() override;
  void ResetAnimationState() override;
  void UpdateCompositorAnimations() override;
  void NotifyCompositorAnimationFinished(int group_id) override;
  void NotifyCompositorAnimationAborted(int group_id) override;
  void LayerForCompositedScrollingDidChange(
      CompositorAnimationTimeline*) override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  // Returns whether or not the animation was sent to the compositor.
  virtual bool SendAnimationToCompositor();

  void NotifyAnimationTakeover(double monotonic_time,
                               double animation_start_time,
                               std::unique_ptr<cc::AnimationCurve>) override;

  std::unique_ptr<CompositorScrollOffsetAnimationCurve> animation_curve_;
  double start_time_;
  WTF::TimeFunction time_function_;

 private:
  // Returns true if the animation was scheduled successfully. If animation
  // could not be scheduled (e.g. because the frame is detached), scrolls
  // immediately to the target and returns false.
  bool RegisterAndScheduleAnimation();

  void CreateAnimationCurve();
  void PostAnimationCleanupAndReset();

  void AddMainThreadScrollingReason();
  void RemoveMainThreadScrollingReason();

  // Returns true if will animate to the given target offset. Returns false
  // only when there is no animation running and we are not starting one
  // because we are already at targetPos.
  bool WillAnimateToOffset(const ScrollOffset& target_pos);

  ScrollOffset target_offset_;
  ScrollGranularity last_granularity_;
};

}  // namespace blink

#endif  // ScrollAnimator_h

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

#include "third_party/blink/renderer/core/scroll/scroll_animator.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

ScrollAnimatorBase* ScrollAnimatorBase::Create(
    ScrollableArea* scrollable_area) {
  if (scrollable_area && scrollable_area->ScrollAnimatorEnabled())
    return MakeGarbageCollected<ScrollAnimator>(scrollable_area);
  return MakeGarbageCollected<ScrollAnimatorBase>(scrollable_area);
}

ScrollAnimator::ScrollAnimator(ScrollableArea* scrollable_area,
                               const base::TickClock* tick_clock)
    : ScrollAnimatorBase(scrollable_area),
      tick_clock_(tick_clock),
      last_granularity_(ScrollGranularity::kScrollByPixel) {}

ScrollAnimator::~ScrollAnimator() {
  if (on_finish_)
    std::move(on_finish_).Run();
}

ScrollOffset ScrollAnimator::DesiredTargetOffset() const {
  if (run_state_ == RunState::kWaitingToCancelOnCompositor)
    return CurrentOffset();
  return (animation_curve_ ||
          run_state_ == RunState::kWaitingToSendToCompositor)
             ? target_offset_
             : CurrentOffset();
}

bool ScrollAnimator::HasRunningAnimation() const {
  return run_state_ != RunState::kPostAnimationCleanup &&
         (animation_curve_ ||
          run_state_ == RunState::kWaitingToSendToCompositor);
}

ScrollOffset ScrollAnimator::ComputeDeltaToConsume(
    const ScrollOffset& delta) const {
  ScrollOffset pos = DesiredTargetOffset();
  ScrollOffset new_pos = scrollable_area_->ClampScrollOffset(pos + delta);
  return new_pos - pos;
}

void ScrollAnimator::ResetAnimationState() {
  ScrollAnimatorCompositorCoordinator::ResetAnimationState();
  if (animation_curve_)
    animation_curve_.reset();
  start_time_ = base::TimeTicks();
  if (on_finish_)
    std::move(on_finish_).Run();
}

ScrollResult ScrollAnimator::UserScroll(
    ScrollGranularity granularity,
    const ScrollOffset& delta,
    ScrollableArea::ScrollCallback on_finish) {
  // We only store on_finish_ when running an animation, and it should be
  // invoked as soon as the animation is finished. If we don't animate the
  // scroll, the callback is invoked immediately without being stored.
  DCHECK(HasRunningAnimation() || on_finish_.is_null());

  base::ScopedClosureRunner run_on_return(std::move(on_finish));

  if (!scrollable_area_->ScrollAnimatorEnabled() ||
      granularity == ScrollGranularity::kScrollByPrecisePixel) {
    // Cancel scroll animation because asked to instant scroll.
    if (HasRunningAnimation())
      CancelAnimation();
    return ScrollAnimatorBase::UserScroll(granularity, delta,
                                          run_on_return.Release());
  }

  TRACE_EVENT0("blink", "ScrollAnimator::scroll");

  bool needs_post_animation_cleanup =
      run_state_ == RunState::kPostAnimationCleanup;
  if (run_state_ == RunState::kPostAnimationCleanup)
    ResetAnimationState();

  ScrollOffset consumed_delta = ComputeDeltaToConsume(delta);
  ScrollOffset target_offset = DesiredTargetOffset();
  target_offset += consumed_delta;

  if (WillAnimateToOffset(target_offset)) {
    last_granularity_ = granularity;
    if (on_finish_)
      std::move(on_finish_).Run();
    on_finish_ = run_on_return.Release();
    // Report unused delta only if there is no animation running. See
    // comment below regarding scroll latching.
    // TODO(bokan): Need to standardize how ScrollAnimators report
    // unusedDelta. This differs from ScrollAnimatorMac currently.
    return ScrollResult(true, true, 0, 0);
  }

  // If the run state when this method was called was PostAnimationCleanup and
  // we're not starting an animation, stay in PostAnimationCleanup state so
  // that the main thread scrolling reason can be removed.
  if (needs_post_animation_cleanup)
    run_state_ = RunState::kPostAnimationCleanup;

  // Report unused delta only if there is no animation and we are not
  // starting one. This ensures we latch for the duration of the
  // animation rather than animating multiple scrollers at the same time.
  if (on_finish_)
    std::move(on_finish_).Run();
  return ScrollResult(false, false, delta.Width(), delta.Height());
}

bool ScrollAnimator::WillAnimateToOffset(const ScrollOffset& target_offset) {
  if (run_state_ == RunState::kPostAnimationCleanup)
    ResetAnimationState();

  if (run_state_ == RunState::kWaitingToCancelOnCompositor ||
      run_state_ == RunState::kWaitingToCancelOnCompositorButNewScroll) {
    DCHECK(animation_curve_);
    target_offset_ = target_offset;
    if (RegisterAndScheduleAnimation())
      run_state_ = RunState::kWaitingToCancelOnCompositorButNewScroll;
    return true;
  }

  if (animation_curve_) {
    if ((target_offset - target_offset_).IsZero())
      return true;

    target_offset_ = target_offset;
    DCHECK(run_state_ == RunState::kRunningOnMainThread ||
           run_state_ == RunState::kRunningOnCompositor ||
           run_state_ == RunState::kRunningOnCompositorButNeedsUpdate ||
           run_state_ == RunState::kRunningOnCompositorButNeedsTakeover ||
           run_state_ == RunState::kRunningOnCompositorButNeedsAdjustment);

    // Running on the main thread, simply update the target offset instead
    // of sending to the compositor.
    if (run_state_ == RunState::kRunningOnMainThread) {
      animation_curve_->UpdateTarget(
          tick_clock_->NowTicks() - start_time_,
          CompositorOffsetFromBlinkOffset(target_offset));

      // Schedule an animation for this scrollable area even though we are
      // updating the animation target - updating the animation will keep
      // it going for another frame. This typically will happen at the
      // beginning of a frame when coalesced input is dispatched.
      // If we don't schedule an animation during the handling of the input
      // event, the LatencyInfo associated with the input event will not be
      // added as a swap promise and we won't get any swap results.
      GetScrollableArea()->ScheduleAnimation();

      return true;
    }

    if (RegisterAndScheduleAnimation())
      run_state_ = RunState::kRunningOnCompositorButNeedsUpdate;
    return true;
  }

  if ((target_offset - CurrentOffset()).IsZero())
    return false;

  target_offset_ = target_offset;
  start_time_ = tick_clock_->NowTicks();

  if (RegisterAndScheduleAnimation())
    run_state_ = RunState::kWaitingToSendToCompositor;

  return true;
}

void ScrollAnimator::AdjustAnimationAndSetScrollOffset(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type) {
  IntSize adjustment = RoundedIntSize(offset) -
                       RoundedIntSize(scrollable_area_->GetScrollOffset());
  ScrollOffsetChanged(offset, scroll_type);

  if (run_state_ == RunState::kIdle) {
    AdjustImplOnlyScrollOffsetAnimation(adjustment);
  } else if (HasRunningAnimation()) {
    target_offset_ += ScrollOffset(adjustment);
    if (animation_curve_) {
      animation_curve_->ApplyAdjustment(adjustment);
      if (run_state_ != RunState::kRunningOnMainThread &&
          RegisterAndScheduleAnimation())
        run_state_ = RunState::kRunningOnCompositorButNeedsAdjustment;
    }
  }
}

void ScrollAnimator::ScrollToOffsetWithoutAnimation(
    const ScrollOffset& offset) {
  current_offset_ = offset;

  ResetAnimationState();
  NotifyOffsetChanged();
}

void ScrollAnimator::TickAnimation(double monotonic_time) {
  if (run_state_ != RunState::kRunningOnMainThread)
    return;

  TRACE_EVENT0("blink", "ScrollAnimator::tickAnimation");
  double elapsed_time =
      monotonic_time - start_time_.since_origin().InSecondsF();

  bool is_finished = (elapsed_time > animation_curve_->Duration());
  ScrollOffset offset = BlinkOffsetFromCompositorOffset(
      is_finished ? animation_curve_->TargetValue()
                  : animation_curve_->GetValue(elapsed_time));

  offset = scrollable_area_->ClampScrollOffset(offset);

  current_offset_ = offset;

  if (is_finished) {
    run_state_ = RunState::kPostAnimationCleanup;
    if (on_finish_)
      std::move(on_finish_).Run();
  } else {
    GetScrollableArea()->ScheduleAnimation();
  }

  TRACE_EVENT0("blink", "ScrollAnimator::notifyOffsetChanged");
  NotifyOffsetChanged();
}

bool ScrollAnimator::SendAnimationToCompositor() {
  if (scrollable_area_->ShouldScrollOnMainThread())
    return false;

  auto animation = std::make_unique<CompositorKeyframeModel>(
      *animation_curve_, compositor_target_property::SCROLL_OFFSET, 0, 0);
  // Being here means that either there is an animation that needs
  // to be sent to the compositor, or an animation that needs to
  // be updated (a new scroll event before the previous animation
  // is finished). In either case, the start time is when the
  // first animation was initiated. This re-targets the animation
  // using the current time on main thread.
  animation->SetStartTime(start_time_);

  bool sent_to_compositor = AddAnimation(std::move(animation));
  if (sent_to_compositor)
    run_state_ = RunState::kRunningOnCompositor;

  return sent_to_compositor;
}

void ScrollAnimator::CreateAnimationCurve() {
  DCHECK(!animation_curve_);
  // It is not correct to assume the input type from the granularity, but we've
  // historically determined animation parameters from granularity.
  CompositorScrollOffsetAnimationCurve::ScrollType scroll_type =
      (last_granularity_ == ScrollGranularity::kScrollByPixel)
          ? CompositorScrollOffsetAnimationCurve::ScrollType::kMouseWheel
          : CompositorScrollOffsetAnimationCurve::ScrollType::kKeyboard;
  animation_curve_ = std::make_unique<CompositorScrollOffsetAnimationCurve>(
      CompositorOffsetFromBlinkOffset(target_offset_), scroll_type);
  animation_curve_->SetInitialValue(
      CompositorOffsetFromBlinkOffset(CurrentOffset()));
}

void ScrollAnimator::UpdateCompositorAnimations() {
  ScrollAnimatorCompositorCoordinator::UpdateCompositorAnimations();

  if (run_state_ == RunState::kPostAnimationCleanup) {
    ResetAnimationState();
    return;
  }

  if (run_state_ == RunState::kWaitingToCancelOnCompositor) {
    DCHECK(compositor_animation_id());
    AbortAnimation();
    ResetAnimationState();
    return;
  }

  if (run_state_ == RunState::kRunningOnCompositorButNeedsTakeover) {
    // The call to ::takeOverCompositorAnimation aborted the animation and
    // put us in this state. The assumption is that takeOver is called
    // because a main thread scrolling reason is added, and simply trying
    // to ::sendAnimationToCompositor will fail and we will run on the main
    // thread.
    RemoveAnimation();
    run_state_ = RunState::kWaitingToSendToCompositor;
  }

  if (run_state_ == RunState::kRunningOnCompositorButNeedsUpdate ||
      run_state_ == RunState::kWaitingToCancelOnCompositorButNewScroll ||
      run_state_ == RunState::kRunningOnCompositorButNeedsAdjustment) {
    // Abort the running animation before a new one with an updated
    // target is added.
    AbortAnimation();

    if (run_state_ != RunState::kRunningOnCompositorButNeedsAdjustment) {
      // When in RunningOnCompositorButNeedsAdjustment, the call to
      // ::adjustScrollOffsetAnimation should have made the necessary
      // adjustment to the curve.
      animation_curve_->UpdateTarget(
          tick_clock_->NowTicks() - start_time_,
          CompositorOffsetFromBlinkOffset(target_offset_));
    }

    if (run_state_ == RunState::kWaitingToCancelOnCompositorButNewScroll) {
      animation_curve_->SetInitialValue(
          CompositorOffsetFromBlinkOffset(CurrentOffset()));
    }

    run_state_ = RunState::kWaitingToSendToCompositor;
  }

  if (run_state_ == RunState::kWaitingToSendToCompositor) {
    if (!element_id_)
      ReattachCompositorAnimationIfNeeded(
          GetScrollableArea()->GetCompositorAnimationTimeline());

    if (!animation_curve_)
      CreateAnimationCurve();

    bool running_on_main_thread = false;
    bool sent_to_compositor = SendAnimationToCompositor();
    if (!sent_to_compositor) {
      running_on_main_thread = RegisterAndScheduleAnimation();
      if (running_on_main_thread)
        run_state_ = RunState::kRunningOnMainThread;
    }
  }
}

void ScrollAnimator::NotifyCompositorAnimationAborted(int group_id) {
  // An animation aborted by the compositor is treated as a finished
  // animation.
  ScrollAnimatorCompositorCoordinator::CompositorAnimationFinished(group_id);
  if (on_finish_)
    std::move(on_finish_).Run();
}

void ScrollAnimator::NotifyCompositorAnimationFinished(int group_id) {
  ScrollAnimatorCompositorCoordinator::CompositorAnimationFinished(group_id);
  if (on_finish_)
    std::move(on_finish_).Run();
}

void ScrollAnimator::CancelAnimation() {
  ScrollAnimatorCompositorCoordinator::CancelAnimation();
  if (on_finish_)
    std::move(on_finish_).Run();
}

void ScrollAnimator::TakeOverCompositorAnimation() {
  ScrollAnimatorCompositorCoordinator::TakeOverCompositorAnimation();
}

void ScrollAnimator::LayerForCompositedScrollingDidChange(
    CompositorAnimationTimeline* timeline) {
  ReattachCompositorAnimationIfNeeded(timeline);
}

bool ScrollAnimator::RegisterAndScheduleAnimation() {
  GetScrollableArea()->RegisterForAnimation();
  if (!scrollable_area_->ScheduleAnimation()) {
    ScrollToOffsetWithoutAnimation(target_offset_);
    ResetAnimationState();
    return false;
  }
  return true;
}

void ScrollAnimator::Trace(Visitor* visitor) {
  ScrollAnimatorBase::Trace(visitor);
}

}  // namespace blink

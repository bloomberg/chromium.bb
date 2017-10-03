// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"

#include <memory>
#include "cc/animation/scroll_offset_animation_curve.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

ScrollAnimatorCompositorCoordinator::ScrollAnimatorCompositorCoordinator()
    : element_id_(),
      run_state_(RunState::kIdle),
      compositor_animation_id_(0),
      compositor_animation_group_id_(0),
      impl_only_animation_takeover_(false) {
  compositor_player_ = CompositorAnimationPlayer::Create();
  DCHECK(compositor_player_);
  compositor_player_->SetAnimationDelegate(this);
}

ScrollAnimatorCompositorCoordinator::~ScrollAnimatorCompositorCoordinator() {}

void ScrollAnimatorCompositorCoordinator::Dispose() {
  compositor_player_->SetAnimationDelegate(nullptr);
  compositor_player_.reset();
}

void ScrollAnimatorCompositorCoordinator::ResetAnimationIds() {
  compositor_animation_id_ = 0;
  compositor_animation_group_id_ = 0;
}

void ScrollAnimatorCompositorCoordinator::ResetAnimationState() {
  run_state_ = RunState::kIdle;
  ResetAnimationIds();
}

bool ScrollAnimatorCompositorCoordinator::HasAnimationThatRequiresService()
    const {
  if (HasImplOnlyAnimationUpdate())
    return true;

  switch (run_state_) {
    case RunState::kIdle:
    case RunState::kRunningOnCompositor:
      return false;
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
    case RunState::kPostAnimationCleanup:
    case RunState::kWaitingToSendToCompositor:
    case RunState::kRunningOnMainThread:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kWaitingToCancelOnCompositor:
      return true;
  }
  NOTREACHED();
  return false;
}

bool ScrollAnimatorCompositorCoordinator::AddAnimation(
    std::unique_ptr<CompositorAnimation> animation) {
  if (compositor_player_->IsElementAttached()) {
    compositor_player_->AddAnimation(std::move(animation));
    return true;
  }
  return false;
}

void ScrollAnimatorCompositorCoordinator::RemoveAnimation() {
  if (compositor_player_->IsElementAttached())
    compositor_player_->RemoveAnimation(compositor_animation_id_);
}

void ScrollAnimatorCompositorCoordinator::AbortAnimation() {
  if (compositor_player_->IsElementAttached())
    compositor_player_->AbortAnimation(compositor_animation_id_);
}

void ScrollAnimatorCompositorCoordinator::CancelAnimation() {
  switch (run_state_) {
    case RunState::kIdle:
    case RunState::kWaitingToCancelOnCompositor:
    case RunState::kPostAnimationCleanup:
      break;
    case RunState::kWaitingToSendToCompositor:
      if (compositor_animation_id_) {
        // We still have a previous animation running on the compositor.
        run_state_ = RunState::kWaitingToCancelOnCompositor;
      } else {
        ResetAnimationState();
      }
      break;
    case RunState::kRunningOnMainThread:
      run_state_ = RunState::kPostAnimationCleanup;
      break;
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositor:
      run_state_ = RunState::kWaitingToCancelOnCompositor;

      // Get serviced the next time compositor updates are allowed.
      GetScrollableArea()->RegisterForAnimation();
  }
}

void ScrollAnimatorCompositorCoordinator::TakeOverCompositorAnimation() {
  switch (run_state_) {
    case RunState::kIdle:
      TakeOverImplOnlyScrollOffsetAnimation();
      break;
    case RunState::kWaitingToCancelOnCompositor:
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
    case RunState::kPostAnimationCleanup:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kWaitingToSendToCompositor:
    case RunState::kRunningOnMainThread:
      break;
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositor:
      // We call abortAnimation that makes changes to the animation running on
      // the compositor. Thus, this function should only be called when in
      // CompositingClean state.
      AbortAnimation();

      run_state_ = RunState::kRunningOnCompositorButNeedsTakeover;

      // Get serviced the next time compositor updates are allowed.
      GetScrollableArea()->RegisterForAnimation();
  }
}

void ScrollAnimatorCompositorCoordinator::CompositorAnimationFinished(
    int group_id) {
  if (compositor_animation_group_id_ != group_id)
    return;

  compositor_animation_id_ = 0;
  compositor_animation_group_id_ = 0;

  switch (run_state_) {
    case RunState::kIdle:
    case RunState::kPostAnimationCleanup:
    case RunState::kRunningOnMainThread:
      NOTREACHED();
      break;
    case RunState::kWaitingToSendToCompositor:
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
      break;
    case RunState::kRunningOnCompositor:
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kWaitingToCancelOnCompositor:
      run_state_ = RunState::kPostAnimationCleanup;
      // Get serviced the next time compositor updates are allowed.
      if (GetScrollableArea())
        GetScrollableArea()->RegisterForAnimation();
      else
        ResetAnimationState();
  }
}

bool ScrollAnimatorCompositorCoordinator::ReattachCompositorPlayerIfNeeded(
    CompositorAnimationTimeline* timeline) {
  bool reattached = false;
  CompositorElementId element_id = GetScrollElementId();
  DCHECK(element_id || (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
                        !GetScrollableArea()->LayerForScrolling()));

  if (element_id != element_id_) {
    if (compositor_player_ && timeline) {
      // Detach from old layer (if any).
      if (element_id_) {
        if (compositor_player_->IsElementAttached())
          compositor_player_->DetachElement();
        timeline->PlayerDestroyed(*this);
      }
      // Attach to new layer (if any).
      if (element_id) {
        DCHECK(!compositor_player_->IsElementAttached());
        timeline->PlayerAttached(*this);
        compositor_player_->AttachElement(element_id);
        reattached = true;
      }
      element_id_ = element_id;
    }
  }

  return reattached;
}

void ScrollAnimatorCompositorCoordinator::NotifyAnimationStarted(
    double monotonic_time,
    int group) {}

void ScrollAnimatorCompositorCoordinator::NotifyAnimationFinished(
    double monotonic_time,
    int group) {
  NotifyCompositorAnimationFinished(group);
}

void ScrollAnimatorCompositorCoordinator::NotifyAnimationAborted(
    double monotonic_time,
    int group) {
  // An animation aborted by the compositor is treated as a finished
  // animation.
  NotifyCompositorAnimationFinished(group);
}

CompositorAnimationPlayer*
ScrollAnimatorCompositorCoordinator::CompositorPlayer() const {
  return compositor_player_.get();
}

FloatPoint ScrollAnimatorCompositorCoordinator::CompositorOffsetFromBlinkOffset(
    ScrollOffset offset) {
  return GetScrollableArea()->ScrollOrigin() + offset;
}

ScrollOffset
ScrollAnimatorCompositorCoordinator::BlinkOffsetFromCompositorOffset(
    FloatPoint offset) {
  return offset - GetScrollableArea()->ScrollOrigin();
}

bool ScrollAnimatorCompositorCoordinator::HasImplOnlyAnimationUpdate() const {
  return !impl_only_animation_adjustment_.IsZero() ||
         impl_only_animation_takeover_;
}

CompositorElementId ScrollAnimatorCompositorCoordinator::GetScrollElementId()
    const {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return GetScrollableArea()->GetCompositorElementId();

  GraphicsLayer* layer = GetScrollableArea()->LayerForScrolling();
  return layer ? layer->PlatformLayer()->GetElementId() : CompositorElementId();
}

void ScrollAnimatorCompositorCoordinator::UpdateImplOnlyCompositorAnimations() {
  if (!HasImplOnlyAnimationUpdate())
    return;

  CompositorAnimationHost* host =
      GetScrollableArea()->GetCompositorAnimationHost();
  CompositorElementId element_id = GetScrollElementId();
  if (host && element_id) {
    if (!impl_only_animation_adjustment_.IsZero()) {
      host->AdjustImplOnlyScrollOffsetAnimation(
          element_id, gfx::Vector2dF(impl_only_animation_adjustment_.Width(),
                                     impl_only_animation_adjustment_.Height()));
    }
    if (impl_only_animation_takeover_)
      host->TakeOverImplOnlyScrollOffsetAnimation(element_id);
  }
  impl_only_animation_adjustment_ = IntSize();
  impl_only_animation_takeover_ = false;
}

void ScrollAnimatorCompositorCoordinator::UpdateCompositorAnimations() {
  if (!GetScrollableArea()->ScrollAnimatorEnabled())
    return;

  UpdateImplOnlyCompositorAnimations();
}

void ScrollAnimatorCompositorCoordinator::ScrollOffsetChanged(
    const ScrollOffset& offset,
    ScrollType scroll_type) {
  GetScrollableArea()->ScrollOffsetChanged(offset, scroll_type);
}

void ScrollAnimatorCompositorCoordinator::AdjustAnimationAndSetScrollOffset(
    const ScrollOffset& offset,
    ScrollType scroll_type) {
  // Subclasses should override this and adjust the animation as necessary.
  ScrollOffsetChanged(offset, scroll_type);
}

void ScrollAnimatorCompositorCoordinator::AdjustImplOnlyScrollOffsetAnimation(
    const IntSize& adjustment) {
  if (!GetScrollableArea()->ScrollAnimatorEnabled())
    return;

  impl_only_animation_adjustment_.Expand(adjustment.Width(),
                                         adjustment.Height());

  GetScrollableArea()->RegisterForAnimation();
}

void ScrollAnimatorCompositorCoordinator::
    TakeOverImplOnlyScrollOffsetAnimation() {
  if (!GetScrollableArea()->ScrollAnimatorEnabled())
    return;

  impl_only_animation_takeover_ = true;

  // Update compositor animations right away to avoid skipping a frame.
  // This imposes the constraint that this function should only be called
  // from or after DocumentLifecycle::LifecycleState::CompositingClean state.
  UpdateImplOnlyCompositorAnimations();

  GetScrollableArea()->RegisterForAnimation();
}

String ScrollAnimatorCompositorCoordinator::RunStateAsText() const {
  switch (run_state_) {
    case RunState::kIdle:
      return String("Idle");
    case RunState::kWaitingToSendToCompositor:
      return String("WaitingToSendToCompositor");
    case RunState::kRunningOnCompositor:
      return String("RunningOnCompositor");
    case RunState::kRunningOnMainThread:
      return String("RunningOnMainThread");
    case RunState::kRunningOnCompositorButNeedsUpdate:
      return String("RunningOnCompositorButNeedsUpdate");
    case RunState::kWaitingToCancelOnCompositor:
      return String("WaitingToCancelOnCompositor");
    case RunState::kPostAnimationCleanup:
      return String("PostAnimationCleanup");
    case RunState::kRunningOnCompositorButNeedsTakeover:
      return String("RunningOnCompositorButNeedsTakeover");
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
      return String("WaitingToCancelOnCompositorButNewScroll");
    case RunState::kRunningOnCompositorButNeedsAdjustment:
      return String("RunningOnCompositorButNeedsAdjustment");
  }
  NOTREACHED();
  return String();
}

}  // namespace blink

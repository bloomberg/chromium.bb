/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/animation/CompositorPendingAnimations.h"

#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffect.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/page/Page.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

void CompositorPendingAnimations::Add(Animation* animation) {
  DCHECK(animation);
  DCHECK_EQ(pending_.Find(animation), kNotFound);
  pending_.push_back(animation);

  Document* document = animation->TimelineInternal()->GetDocument();
  if (document->View())
    document->View()->ScheduleAnimation();

  bool visible = document->GetPage() && document->GetPage()->IsPageVisible();
  if (!visible && !timer_.IsActive()) {
    timer_.StartOneShot(0, BLINK_FROM_HERE);
  }
}

bool CompositorPendingAnimations::Update(
    const Optional<CompositorElementIdSet>& composited_element_ids,
    bool start_on_compositor) {
  HeapVector<Member<Animation>> waiting_for_start_time;
  bool started_synchronized_on_compositor = false;

  HeapVector<Member<Animation>> animations;
  HeapVector<Member<Animation>> deferred;
  animations.swap(pending_);
  int compositor_group = ++compositor_group_;
  while (compositor_group == 0 || compositor_group == 1) {
    // Wrap around, skipping 0, 1.
    // * 0 is reserved for automatic assignment
    // * 1 is used for animations with a specified start time
    compositor_group = ++compositor_group_;
  }

  for (auto& animation : animations) {
    bool had_compositor_animation =
        animation->HasActiveAnimationsOnCompositor();
    // Animations with a start time do not participate in compositor start-time
    // grouping.
    if (animation->PreCommit(animation->HasStartTime() ? 1 : compositor_group,
                             composited_element_ids, start_on_compositor)) {
      if (animation->HasActiveAnimationsOnCompositor() &&
          !had_compositor_animation) {
        started_synchronized_on_compositor = true;
      }

      if (animation->Playing() && !animation->HasStartTime() &&
          animation->TimelineInternal() &&
          animation->TimelineInternal()->IsActive()) {
        waiting_for_start_time.push_back(animation.Get());
      }
    } else {
      deferred.push_back(animation);
    }
  }

  // If any synchronized animations were started on the compositor, all
  // remaining synchronized animations need to wait for the synchronized
  // start time. Otherwise they may start immediately.
  if (started_synchronized_on_compositor) {
    for (auto& animation : waiting_for_start_time) {
      if (!animation->HasStartTime()) {
        waiting_for_compositor_animation_start_.push_back(animation);
      }
    }
  } else {
    for (auto& animation : waiting_for_start_time) {
      if (!animation->HasStartTime()) {
        animation->NotifyCompositorStartTime(
            animation->TimelineInternal()->CurrentTimeInternal());
      }
    }
  }

  // FIXME: The postCommit should happen *after* the commit, not before.
  for (auto& animation : animations)
    animation->PostCommit(animation->TimelineInternal()->CurrentTimeInternal());

  DCHECK(pending_.IsEmpty());
  DCHECK(start_on_compositor || deferred.IsEmpty());
  for (auto& animation : deferred)
    animation->SetCompositorPending();
  DCHECK_EQ(pending_.size(), deferred.size());

  if (started_synchronized_on_compositor)
    return true;

  if (waiting_for_compositor_animation_start_.IsEmpty())
    return false;

  // Check if we're still waiting for any compositor animations to start.
  for (auto& animation : waiting_for_compositor_animation_start_) {
    if (animation->HasActiveAnimationsOnCompositor())
      return true;
  }

  // If not, go ahead and start any animations that were waiting.
  NotifyCompositorAnimationStarted(MonotonicallyIncreasingTime());

  DCHECK_EQ(pending_.size(), deferred.size());
  return false;
}

void CompositorPendingAnimations::NotifyCompositorAnimationStarted(
    double monotonic_animation_start_time,
    int compositor_group) {
  TRACE_EVENT0("blink",
               "CompositorPendingAnimations::notifyCompositorAnimationStarted");
  HeapVector<Member<Animation>> animations;
  animations.swap(waiting_for_compositor_animation_start_);

  for (auto animation : animations) {
    if (animation->HasStartTime() ||
        animation->PlayStateInternal() != Animation::kPending ||
        !animation->TimelineInternal() ||
        !animation->TimelineInternal()->IsActive()) {
      // Already started or no longer relevant.
      continue;
    }
    if (compositor_group && animation->CompositorGroup() != compositor_group) {
      // Still waiting.
      waiting_for_compositor_animation_start_.push_back(animation);
      continue;
    }
    animation->NotifyCompositorStartTime(
        monotonic_animation_start_time -
        animation->TimelineInternal()->ZeroTime());
  }
}

DEFINE_TRACE(CompositorPendingAnimations) {
  visitor->Trace(pending_);
  visitor->Trace(waiting_for_compositor_animation_start_);
}

}  // namespace blink

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

#include "core/animation/DocumentAnimations.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/PendingAnimations.h"
#include "core/animation/WorkletAnimationController.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"

namespace blink {

namespace {

void UpdateAnimationTiming(Document& document, TimingUpdateReason reason) {
  document.Timeline().ServiceAnimations(reason);
}

}  // namespace

void DocumentAnimations::UpdateAnimationTimingForAnimationFrame(
    Document& document) {
  UpdateAnimationTiming(document, kTimingUpdateForAnimationFrame);
}

bool DocumentAnimations::NeedsAnimationTimingUpdate(const Document& document) {
  return document.Timeline().HasOutdatedAnimation() ||
         document.Timeline().NeedsAnimationTimingUpdate();
}

void DocumentAnimations::UpdateAnimationTimingIfNeeded(Document& document) {
  if (NeedsAnimationTimingUpdate(document))
    UpdateAnimationTiming(document, kTimingUpdateOnDemand);
}

void DocumentAnimations::UpdateAnimations(
    Document& document,
    DocumentLifecycle::LifecycleState required_lifecycle_state,
    Optional<CompositorElementIdSet>& composited_element_ids) {
  DCHECK(document.Lifecycle().GetState() >= required_lifecycle_state);

  if (document.GetPendingAnimations().Update(composited_element_ids)) {
    DCHECK(document.View());
    document.View()->ScheduleAnimation();
  }

  document.GetWorkletAnimationController().Update();

  document.Timeline().ScheduleNextService();
}

}  // namespace blink

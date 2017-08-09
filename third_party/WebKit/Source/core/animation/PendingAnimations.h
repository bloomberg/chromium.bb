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

#ifndef PendingAnimations_h
#define PendingAnimations_h

#include "core/CoreExport.h"
#include "core/animation/Animation.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/Timer.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Handles starting animations when they could potentially require
// interaction with the compositor. This can include both main-thread
// and compositor thread animations. For example, when the Document
// changes visibility state, all animations for the document's
// timeline are set to "compositor pending" which will include them in
// a consideration pass here.
//
// Manages the starting of pending animations on the compositor following a
// compositing update.
//
// For CSS Animations, used to synchronize the start of main-thread animations
// with compositor animations when both classes of CSS Animations are triggered
// by the same recalc.
class CORE_EXPORT PendingAnimations final
    : public GarbageCollectedFinalized<PendingAnimations> {
 public:
  explicit PendingAnimations(Document& document)
      : timer_(TaskRunnerHelper::Get(TaskType::kUnspecedTimer, &document),
               this,
               &PendingAnimations::TimerFired),
        compositor_group_(1) {}

  void Add(Animation*);
  // Returns whether we are waiting for an animation to start and should
  // service again on the next frame.
  bool Update(const Optional<CompositorElementIdSet>&,
              bool start_on_compositor = true);
  void NotifyCompositorAnimationStarted(double monotonic_animation_start_time,
                                        int compositor_group = 0);

  DECLARE_TRACE();

 private:
  void TimerFired(TimerBase*) {
    Update(Optional<CompositorElementIdSet>(), false);
  }

  HeapVector<Member<Animation>> pending_;
  HeapVector<Member<Animation>> waiting_for_compositor_animation_start_;
  TaskRunnerTimer<PendingAnimations> timer_;
  int compositor_group_;
};

}  // namespace blink

#endif

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

#ifndef DocumentTimeline_h
#define DocumentTimeline_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/EffectModel.h"
#include "core/dom/Element.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/Timer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Animation;
class AnimationEffectReadOnly;
class Document;
class DocumentTimelineOptions;

// DocumentTimeline is constructed and owned by Document, and tied to its
// lifecycle.
class CORE_EXPORT DocumentTimeline : public AnimationTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class PlatformTiming : public GarbageCollectedFinalized<PlatformTiming> {
   public:
    // Calls DocumentTimeline's wake() method after duration seconds.
    virtual void WakeAfter(double duration) = 0;
    virtual void ServiceOnNextFrame() = 0;
    virtual ~PlatformTiming() {}
    virtual void Trace(blink::Visitor* visitor) {}
  };

  static DocumentTimeline* Create(Document*,
                                  double origin_time_in_milliseconds = 0.0,
                                  PlatformTiming* = nullptr);

  // Web Animations API IDL constructor
  static DocumentTimeline* Create(ExecutionContext*,
                                  const DocumentTimelineOptions&);

  virtual ~DocumentTimeline() {}

  bool IsDocumentTimeline() const final { return true; }

  void ServiceAnimations(TimingUpdateReason);
  void ScheduleNextService();

  Animation* Play(AnimationEffectReadOnly*);
  HeapVector<Member<Animation>> getAnimations();

  void AnimationAttached(Animation&);

  bool IsActive();
  bool HasPendingUpdates() const {
    return !animations_needing_update_.IsEmpty();
  }
  double ZeroTime();
  double currentTime(bool& is_null) override;
  double currentTime();
  double CurrentTimeInternal(bool& is_null);
  double CurrentTimeInternal();
  double EffectiveTime();
  void PauseAnimationsForTesting(double);

  void SetAllCompositorPending(bool source_changed = false);
  void SetOutdatedAnimation(Animation*);
  void ClearOutdatedAnimation(Animation*);
  bool HasOutdatedAnimation() const { return outdated_animation_count_ > 0; }
  bool NeedsAnimationTimingUpdate();
  void InvalidateKeyframeEffects(const TreeScope&);

  void SetPlaybackRate(double);
  double PlaybackRate() const;

  CompositorAnimationTimeline* CompositorTimeline() const {
    return compositor_timeline_.get();
  }

  Document* GetDocument() { return document_.Get(); }
  void Wake();
  void ResetForTesting();

  virtual void Trace(blink::Visitor*);

 private:
  DocumentTimeline(Document*,
                   double origin_time_in_milliseconds,
                   PlatformTiming*);

  Member<Document> document_;
  double origin_time_;
  double zero_time_;
  bool zero_time_initialized_;
  unsigned outdated_animation_count_;
  // Animations which will be updated on the next frame
  // i.e. current, in effect, or had timing changed
  HeapHashSet<Member<Animation>> animations_needing_update_;
  HeapHashSet<WeakMember<Animation>> animations_;

  double playback_rate_;

  friend class SMILTimeContainer;
  static const double kMinimumDelay;

  Member<PlatformTiming> timing_;
  double last_current_time_internal_;

  std::unique_ptr<CompositorAnimationTimeline> compositor_timeline_;

  class DocumentTimelineTiming final : public PlatformTiming {
   public:
    DocumentTimelineTiming(DocumentTimeline* timeline)
        : timeline_(timeline),
          timer_(TaskRunnerHelper::Get(TaskType::kUnspecedTimer,
                                       timeline->GetDocument()),
                 this,
                 &DocumentTimelineTiming::TimerFired) {
      DCHECK(timeline_);
    }

    void WakeAfter(double duration) override;
    void ServiceOnNextFrame() override;

    void TimerFired(TimerBase*) { timeline_->Wake(); }

    virtual void Trace(blink::Visitor*);

   private:
    Member<DocumentTimeline> timeline_;
    TaskRunnerTimer<DocumentTimelineTiming> timer_;
  };

  friend class AnimationDocumentTimelineTest;
};

DEFINE_TYPE_CASTS(DocumentTimeline,
                  AnimationTimeline,
                  timeline,
                  timeline->IsDocumentTimeline(),
                  timeline.IsDocumentTimeline());

}  // namespace blink

#endif

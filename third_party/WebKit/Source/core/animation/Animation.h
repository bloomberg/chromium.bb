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

#ifndef Animation_h
#define Animation_h

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMException.h"
#include "core/events/EventTarget.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CompositorAnimationPlayer;
class Element;
class ExceptionState;
class TreeScope;

class CORE_EXPORT Animation final : public EventTargetWithInlineData,
                                    public ActiveScriptWrappable<Animation>,
                                    public ContextLifecycleObserver,
                                    public CompositorAnimationDelegate,
                                    public CompositorAnimationPlayerClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Animation);

 public:
  enum AnimationPlayState {
    kUnset,
    kIdle,
    kPending,
    kRunning,
    kPaused,
    kFinished
  };

  static Animation* Create(AnimationEffectReadOnly*, AnimationTimeline*);

  // Web Animations API IDL constructors.
  static Animation* Create(ExecutionContext*,
                           AnimationEffectReadOnly*,
                           ExceptionState&);
  static Animation* Create(ExecutionContext*,
                           AnimationEffectReadOnly*,
                           AnimationTimeline*,
                           ExceptionState&);

  ~Animation();
  void Dispose();

  // Returns whether the animation is finished.
  bool Update(TimingUpdateReason);

  // timeToEffectChange returns:
  //  infinity  - if this animation is no longer in effect
  //  0         - if this animation requires an update on the next frame
  //  n         - if this animation requires an update after 'n' units of time
  double TimeToEffectChange();

  void cancel();

  double currentTime(bool& is_null);
  double currentTime();
  void setCurrentTime(double new_current_time, bool is_null);

  double CurrentTimeInternal() const;
  double UnlimitedCurrentTimeInternal() const;

  void SetCurrentTimeInternal(double new_current_time,
                              TimingUpdateReason = kTimingUpdateOnDemand);
  bool Paused() const { return paused_ && !is_paused_for_testing_; }
  static const char* PlayStateString(AnimationPlayState);
  String playState() const { return PlayStateString(PlayStateInternal()); }
  AnimationPlayState PlayStateInternal() const;

  void pause(ExceptionState& = ASSERT_NO_EXCEPTION);
  void play(ExceptionState& = ASSERT_NO_EXCEPTION);
  void reverse(ExceptionState& = ASSERT_NO_EXCEPTION);
  void finish(ExceptionState& = ASSERT_NO_EXCEPTION);

  ScriptPromise finished(ScriptState*);
  ScriptPromise ready(ScriptState*);

  bool Playing() const {
    return !(PlayStateInternal() == kIdle || Limited() || paused_ ||
             is_paused_for_testing_);
  }
  bool Limited() const { return Limited(CurrentTimeInternal()); }
  bool FinishedInternal() const { return finished_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(finish);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel);

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  bool HasPendingActivity() const final;
  void ContextDestroyed(ExecutionContext*) override;

  double playbackRate() const;
  void setPlaybackRate(double);
  AnimationTimeline* timeline() {
    return static_cast<AnimationTimeline*>(timeline_);
  }
  const DocumentTimeline* TimelineInternal() const { return timeline_; }
  DocumentTimeline* TimelineInternal() { return timeline_; }

  double CalculateStartTime(double current_time) const;
  bool HasStartTime() const { return !IsNull(start_time_); }
  double startTime(bool& is_null) const;
  double startTime() const;
  double StartTimeInternal() const { return start_time_; }
  void setStartTime(double, bool is_null);
  void SetStartTimeInternal(double);

  const AnimationEffectReadOnly* effect() const { return content_.Get(); }
  AnimationEffectReadOnly* effect() { return content_.Get(); }
  void setEffect(AnimationEffectReadOnly*);

  void setId(const String& id) { id_ = id; }
  const String& id() const { return id_; }

  // Pausing via this method is not reflected in the value returned by
  // paused() and must never overlap with pausing via pause().
  void PauseForTesting(double pause_time);
  void DisableCompositedAnimationForTesting();

  // This should only be used for CSS
  void Unpause();

  void SetOutdated();
  bool Outdated() { return outdated_; }

  CompositorAnimations::FailureCode CheckCanStartAnimationOnCompositor(
      const Optional<CompositorElementIdSet>& composited_element_ids) const;
  void StartAnimationOnCompositor(
      const Optional<CompositorElementIdSet>& composited_element_ids);
  void CancelAnimationOnCompositor();
  void RestartAnimationOnCompositor();
  void CancelIncompatibleAnimationsOnCompositor();
  bool HasActiveAnimationsOnCompositor();
  void SetCompositorPending(bool effect_changed = false);
  void NotifyCompositorStartTime(double timeline_time);
  void NotifyStartTime(double timeline_time);
  // CompositorAnimationPlayerClient implementation.
  CompositorAnimationPlayer* CompositorPlayer() const override {
    return compositor_player_ ? compositor_player_->Player() : nullptr;
  }

  bool Affects(const Element&, CSSPropertyID) const;

  // Returns whether we should continue with the commit for this animation or
  // wait until next commit.
  bool PreCommit(int compositor_group,
                 const Optional<CompositorElementIdSet>&,
                 bool start_on_compositor);
  void PostCommit(double timeline_time);

  unsigned SequenceNumber() const { return sequence_number_; }
  int CompositorGroup() const { return compositor_group_; }

  static bool HasLowerPriority(const Animation* animation1,
                               const Animation* animation2) {
    return animation1->SequenceNumber() < animation2->SequenceNumber();
  }

  bool EffectSuppressed() const { return effect_suppressed_; }
  void SetEffectSuppressed(bool);

  void InvalidateKeyframeEffect(const TreeScope&);

  DECLARE_VIRTUAL_TRACE();

 protected:
  DispatchEventResult DispatchEventInternal(Event*) override;
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  Animation(ExecutionContext*, DocumentTimeline&, AnimationEffectReadOnly*);

  void ClearOutdated();
  void ForceServiceOnNextFrame();

  double EffectEnd() const;
  bool Limited(double current_time) const;

  AnimationPlayState CalculatePlayState();
  double CalculateCurrentTime() const;

  void UnpauseInternal();
  void SetPlaybackRateInternal(double);
  void UpdateCurrentTimingState(TimingUpdateReason);

  void BeginUpdatingState();
  void EndUpdatingState();

  CompositorAnimations::FailureCode CheckCanStartAnimationOnCompositorInternal(
      const Optional<CompositorElementIdSet>&) const;
  void CreateCompositorPlayer();
  void DestroyCompositorPlayer();
  void AttachCompositorTimeline();
  void DetachCompositorTimeline();
  void AttachCompositedLayers();
  void DetachCompositedLayers();
  // CompositorAnimationDelegate implementation.
  void NotifyAnimationStarted(double monotonic_time, int group) override;
  void NotifyAnimationFinished(double monotonic_time, int group) override {}
  void NotifyAnimationAborted(double monotonic_time, int group) override {}

  using AnimationPromise = ScriptPromiseProperty<Member<Animation>,
                                                 Member<Animation>,
                                                 Member<DOMException>>;
  void ResolvePromiseMaybeAsync(AnimationPromise*);
  void RejectAndResetPromise(AnimationPromise*);
  void RejectAndResetPromiseMaybeAsync(AnimationPromise*);

  String id_;

  AnimationPlayState play_state_;
  double playback_rate_;
  double start_time_;
  double hold_time_;

  unsigned sequence_number_;

  Member<AnimationPromise> finished_promise_;
  Member<AnimationPromise> ready_promise_;

  Member<AnimationEffectReadOnly> content_;
  Member<DocumentTimeline> timeline_;

  // Reflects all pausing, including via pauseForTesting().
  bool paused_;
  bool held_;
  bool is_paused_for_testing_;
  bool is_composited_animation_disabled_for_testing_;

  // This indicates timing information relevant to the animation's effect
  // has changed by means other than the ordinary progression of time
  bool outdated_;

  bool finished_;
  // Holds a 'finished' event queued for asynchronous dispatch via the
  // ScriptedAnimationController. This object remains active until the
  // event is actually dispatched.
  Member<Event> pending_finished_event_;

  Member<Event> pending_cancelled_event_;

  enum CompositorAction { kNone, kPause, kStart, kPauseThenStart };

  class CompositorState {
    USING_FAST_MALLOC(CompositorState);
    WTF_MAKE_NONCOPYABLE(CompositorState);

   public:
    CompositorState(Animation& animation)
        : start_time(animation.start_time_),
          hold_time(animation.hold_time_),
          playback_rate(animation.playback_rate_),
          effect_changed(false),
          pending_action(kStart) {}
    double start_time;
    double hold_time;
    double playback_rate;
    bool effect_changed;
    CompositorAction pending_action;
  };

  enum CompositorPendingChange {
    kSetCompositorPending,
    kSetCompositorPendingWithEffectChanged,
    kDoNotSetCompositorPending,
  };

  class PlayStateUpdateScope {
    STACK_ALLOCATED();

   public:
    PlayStateUpdateScope(Animation&,
                         TimingUpdateReason,
                         CompositorPendingChange = kSetCompositorPending);
    ~PlayStateUpdateScope();

   private:
    Member<Animation> animation_;
    AnimationPlayState initial_play_state_;
    CompositorPendingChange compositor_pending_change_;
  };

  // CompositorAnimationPlayer objects need to eagerly sever
  // their connection to their Animation delegate; use a separate
  // 'holder' on-heap object to accomplish that.
  class CompositorAnimationPlayerHolder
      : public GarbageCollectedFinalized<CompositorAnimationPlayerHolder> {
    USING_PRE_FINALIZER(CompositorAnimationPlayerHolder, Dispose);

   public:
    static CompositorAnimationPlayerHolder* Create(Animation*);

    void Detach();

    DEFINE_INLINE_TRACE() { visitor->Trace(animation_); }

    CompositorAnimationPlayer* Player() const {
      return compositor_player_.get();
    }

   private:
    explicit CompositorAnimationPlayerHolder(Animation*);

    void Dispose();

    std::unique_ptr<CompositorAnimationPlayer> compositor_player_;
    Member<Animation> animation_;
  };

  // This mirrors the known compositor state. It is created when a compositor
  // animation is started. Updated once the start time is known and each time
  // modifications are pushed to the compositor.
  std::unique_ptr<CompositorState> compositor_state_;
  bool compositor_pending_;
  int compositor_group_;

  Member<CompositorAnimationPlayerHolder> compositor_player_;

  bool current_time_pending_;
  bool state_is_being_updated_;

  bool effect_suppressed_;

  FRIEND_TEST_ALL_PREFIXES(AnimationAnimationTest,
                           NoCompositeWithoutCompositedElementId);
};

}  // namespace blink

#endif  // Animation_h

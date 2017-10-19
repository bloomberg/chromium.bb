// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletAnimation_h
#define WorkletAnimation_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/modules/v8/document_timeline_or_scroll_timeline.h"
#include "core/animation/Animation.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/WorkletAnimationBase.h"
#include "modules/ModulesExport.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

// The main-thread controller for a single AnimationWorklet animator instance.
//
// WorkletAnimation instances exist in the document execution context (i.e. in
// the main javascript thread), and are a type of animation that delegates
// actual playback to an 'animator instance'. The animator instance runs in a
// separate worklet execution context (which can either also be on the main
// thread or may be in a separate worklet thread).
//
// All methods in this class should be called in the document execution context.
//
// Spec: https://wicg.github.io/animation-worklet/#worklet-animation-desc
class MODULES_EXPORT WorkletAnimation : public WorkletAnimationBase,
                                        public ScriptWrappable,
                                        public CompositorAnimationPlayerClient,
                                        public CompositorAnimationDelegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(WorkletAnimation, Dispose);

 public:
  static WorkletAnimation* Create(
      String animator_name,
      const HeapVector<Member<KeyframeEffectReadOnly>>&,
      HeapVector<DocumentTimelineOrScrollTimeline>&,
      RefPtr<SerializedScriptValue>,
      ExceptionState&);

  virtual ~WorkletAnimation() {}

  String playState();
  void play();
  void cancel();

  // WorkletAnimationBase implementation.
  bool StartOnCompositor() override;

  // CompositorAnimationPlayerClient implementation.
  CompositorAnimationPlayer* CompositorPlayer() const override {
    return compositor_player_.get();
  }

  // CompositorAnimationDelegate implementation.
  void NotifyAnimationStarted(double monotonic_time, int group) override {}
  void NotifyAnimationFinished(double monotonic_time, int group) override {}
  void NotifyAnimationAborted(double monotonic_time, int group) override {}

  void Dispose();

  const String& Name() { return animator_name_; }

  const HeapVector<DocumentTimelineOrScrollTimeline>& Timelines() {
    return timelines_;
  }

  const RefPtr<SerializedScriptValue> Options() { return options_; }

  virtual void Trace(blink::Visitor*);

 private:
  WorkletAnimation(const String& animator_name,
                   Document&,
                   const HeapVector<Member<KeyframeEffectReadOnly>>&,
                   HeapVector<DocumentTimelineOrScrollTimeline>&,
                   RefPtr<SerializedScriptValue>);

  const String animator_name_;
  Animation::AnimationPlayState play_state_;

  Member<Document> document_;

  HeapVector<Member<KeyframeEffectReadOnly>> effects_;
  HeapVector<DocumentTimelineOrScrollTimeline> timelines_;
  RefPtr<SerializedScriptValue> options_;

  std::unique_ptr<CompositorAnimationPlayer> compositor_player_;
};

}  // namespace blink

#endif

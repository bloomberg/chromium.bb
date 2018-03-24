// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletAnimation_h
#define WorkletAnimation_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/modules/v8/document_timeline_or_scroll_timeline.h"
#include "core/animation/Animation.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/WorkletAnimationBase.h"
#include "modules/ModulesExport.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationClient.h"
#include "platform/animation/CompositorAnimationDelegate.h"

namespace blink {

class AnimationEffectOrAnimationEffectSequence;

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
                                        public CompositorAnimationClient,
                                        public CompositorAnimationDelegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(WorkletAnimation, Dispose);

 public:
  static WorkletAnimation* Create(
      String animator_name,
      const AnimationEffectOrAnimationEffectSequence&,
      DocumentTimelineOrScrollTimeline,
      scoped_refptr<SerializedScriptValue>,
      ExceptionState&);

  ~WorkletAnimation() override = default;

  String playState();
  void play();
  void cancel();

  // WorkletAnimationBase implementation.
  bool StartOnCompositor(String* failure_message) override;

  // CompositorAnimationClient implementation.
  CompositorAnimation* GetCompositorAnimation() const override {
    return compositor_animation_.get();
  }

  // CompositorAnimationDelegate implementation.
  void NotifyAnimationStarted(double monotonic_time, int group) override {}
  void NotifyAnimationFinished(double monotonic_time, int group) override {}
  void NotifyAnimationAborted(double monotonic_time, int group) override {}

  void Dispose();

  Document* GetDocument() const override { return document_.Get(); }
  const String& Name() { return animator_name_; }

  const DocumentTimelineOrScrollTimeline& Timeline() { return timeline_; }

  const scoped_refptr<SerializedScriptValue> Options() { return options_; }
  KeyframeEffect* GetEffect() const override { return effects_.at(0); }

  void Trace(blink::Visitor*) override;

 private:
  WorkletAnimation(const String& animator_name,
                   Document&,
                   const HeapVector<Member<KeyframeEffect>>&,
                   DocumentTimelineOrScrollTimeline,
                   scoped_refptr<SerializedScriptValue>);

  const String animator_name_;
  Animation::AnimationPlayState play_state_;

  Member<Document> document_;

  HeapVector<Member<KeyframeEffect>> effects_;
  DocumentTimelineOrScrollTimeline timeline_;
  scoped_refptr<SerializedScriptValue> options_;

  std::unique_ptr<CompositorAnimation> compositor_animation_;
};

}  // namespace blink

#endif

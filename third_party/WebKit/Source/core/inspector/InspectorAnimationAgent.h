// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAnimationAgent_h
#define InspectorAnimationAgent_h

#include "core/CoreExport.h"
#include "core/animation/Animation.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Animation.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class DocumentTimeline;
class InspectedFrames;
class InspectorCSSAgent;

class CORE_EXPORT InspectorAnimationAgent final
    : public InspectorBaseAgent<protocol::Animation::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);

 public:
  InspectorAnimationAgent(InspectedFrames*,
                          InspectorCSSAgent*,
                          v8_inspector::V8InspectorSession*);

  // Base agent methods.
  void Restore() override;
  void DidCommitLoadForLocalFrame(LocalFrame*) override;

  // Protocol method implementations
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getPlaybackRate(double* playback_rate) override;
  protocol::Response setPlaybackRate(double) override;
  protocol::Response getCurrentTime(const String& id,
                                    double* current_time) override;
  protocol::Response setPaused(
      std::unique_ptr<protocol::Array<String>> animations,
      bool paused) override;
  protocol::Response setTiming(const String& animation_id,
                               double duration,
                               double delay) override;
  protocol::Response seekAnimations(
      std::unique_ptr<protocol::Array<String>> animations,
      double current_time) override;
  protocol::Response releaseAnimations(
      std::unique_ptr<protocol::Array<String>> animations) override;
  protocol::Response resolveAnimation(
      const String& animation_id,
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*)
      override;

  // API for InspectorInstrumentation
  void DidCreateAnimation(unsigned);
  void AnimationPlayStateChanged(blink::Animation*,
                                 blink::Animation::AnimationPlayState,
                                 blink::Animation::AnimationPlayState);
  void DidClearDocumentOfWindowObject(LocalFrame*);

  // Methods for other agents to use.
  protocol::Response AssertAnimation(const String& id,
                                     blink::Animation*& result);

  DECLARE_VIRTUAL_TRACE();

 private:
  using AnimationType = protocol::Animation::Animation::TypeEnum;

  std::unique_ptr<protocol::Animation::Animation> BuildObjectForAnimation(
      blink::Animation&);
  std::unique_ptr<protocol::Animation::Animation> BuildObjectForAnimation(
      blink::Animation&,
      String,
      std::unique_ptr<protocol::Animation::KeyframesRule> keyframe_rule =
          nullptr);
  double NormalizedStartTime(blink::Animation&);
  DocumentTimeline& ReferenceTimeline();
  blink::Animation* AnimationClone(blink::Animation*);
  String CreateCSSId(blink::Animation&);

  Member<InspectedFrames> inspected_frames_;
  Member<InspectorCSSAgent> css_agent_;
  v8_inspector::V8InspectorSession* v8_session_;
  HeapHashMap<String, Member<blink::Animation>> id_to_animation_;
  HeapHashMap<String, Member<blink::Animation>> id_to_animation_clone_;
  HashMap<String, String> id_to_animation_type_;
  bool is_cloning_;
  HashSet<String> cleared_animations_;
};

}  // namespace blink

#endif  // InspectorAnimationAgent_h

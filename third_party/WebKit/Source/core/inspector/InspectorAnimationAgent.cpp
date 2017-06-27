// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorAnimationAgent.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/ComputedTimingProperties.h"
#include "core/animation/EffectModel.h"
#include "core/animation/ElementAnimation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/inspector/V8InspectorString.h"
#include "platform/Decimal.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/text/Base64.h"

namespace AnimationAgentState {
static const char animationAgentEnabled[] = "animationAgentEnabled";
static const char animationAgentPlaybackRate[] = "animationAgentPlaybackRate";
}

namespace blink {

using protocol::Response;

InspectorAnimationAgent::InspectorAnimationAgent(
    InspectedFrames* inspected_frames,
    InspectorCSSAgent* css_agent,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      css_agent_(css_agent),
      v8_session_(v8_session),
      is_cloning_(false) {}

void InspectorAnimationAgent::Restore() {
  if (state_->booleanProperty(AnimationAgentState::animationAgentEnabled,
                              false)) {
    enable();
    double playback_rate = 1;
    state_->getDouble(AnimationAgentState::animationAgentPlaybackRate,
                      &playback_rate);
    setPlaybackRate(playback_rate);
  }
}

Response InspectorAnimationAgent::enable() {
  state_->setBoolean(AnimationAgentState::animationAgentEnabled, true);
  instrumenting_agents_->addInspectorAnimationAgent(this);
  return Response::OK();
}

Response InspectorAnimationAgent::disable() {
  setPlaybackRate(1);
  for (const auto& clone : id_to_animation_clone_.Values())
    clone->cancel();
  state_->setBoolean(AnimationAgentState::animationAgentEnabled, false);
  instrumenting_agents_->removeInspectorAnimationAgent(this);
  id_to_animation_.clear();
  id_to_animation_type_.clear();
  id_to_animation_clone_.clear();
  cleared_animations_.clear();
  return Response::OK();
}

void InspectorAnimationAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    id_to_animation_.clear();
    id_to_animation_type_.clear();
    id_to_animation_clone_.clear();
    cleared_animations_.clear();
  }
  double playback_rate = 1;
  state_->getDouble(AnimationAgentState::animationAgentPlaybackRate,
                    &playback_rate);
  setPlaybackRate(playback_rate);
}

static std::unique_ptr<protocol::Animation::AnimationEffect>
BuildObjectForAnimationEffect(KeyframeEffectReadOnly* effect,
                              bool is_transition) {
  ComputedTimingProperties computed_timing = effect->getComputedTiming();
  double delay = computed_timing.delay();
  double duration = computed_timing.duration().getAsUnrestrictedDouble();
  String easing = effect->SpecifiedTiming().timing_function->ToString();

  if (is_transition) {
    // Obtain keyframes and convert keyframes back to delay
    DCHECK(effect->Model()->IsKeyframeEffectModel());
    const KeyframeEffectModelBase* model =
        ToKeyframeEffectModelBase(effect->Model());
    Vector<RefPtr<Keyframe>> keyframes =
        KeyframeEffectModelBase::NormalizedKeyframesForInspector(
            model->GetFrames());
    if (keyframes.size() == 3) {
      delay = keyframes.at(1)->Offset() * duration;
      duration -= delay;
      easing = keyframes.at(1)->Easing().ToString();
    } else {
      easing = keyframes.at(0)->Easing().ToString();
    }
  }

  std::unique_ptr<protocol::Animation::AnimationEffect> animation_object =
      protocol::Animation::AnimationEffect::create()
          .setDelay(delay)
          .setEndDelay(computed_timing.endDelay())
          .setIterationStart(computed_timing.iterationStart())
          .setIterations(computed_timing.iterations())
          .setDuration(duration)
          .setDirection(computed_timing.direction())
          .setFill(computed_timing.fill())
          .setBackendNodeId(DOMNodeIds::IdForNode(effect->Target()))
          .setEasing(easing)
          .build();
  return animation_object;
}

static std::unique_ptr<protocol::Animation::KeyframeStyle>
BuildObjectForStringKeyframe(const StringKeyframe* keyframe) {
  Decimal decimal = Decimal::FromDouble(keyframe->Offset() * 100);
  String offset = decimal.ToString();
  offset.append('%');

  std::unique_ptr<protocol::Animation::KeyframeStyle> keyframe_object =
      protocol::Animation::KeyframeStyle::create()
          .setOffset(offset)
          .setEasing(keyframe->Easing().ToString())
          .build();
  return keyframe_object;
}

static std::unique_ptr<protocol::Animation::KeyframesRule>
BuildObjectForAnimationKeyframes(const KeyframeEffectReadOnly* effect) {
  if (!effect || !effect->Model() || !effect->Model()->IsKeyframeEffectModel())
    return nullptr;
  const KeyframeEffectModelBase* model =
      ToKeyframeEffectModelBase(effect->Model());
  Vector<RefPtr<Keyframe>> normalized_keyframes =
      KeyframeEffectModelBase::NormalizedKeyframesForInspector(
          model->GetFrames());
  std::unique_ptr<protocol::Array<protocol::Animation::KeyframeStyle>>
      keyframes = protocol::Array<protocol::Animation::KeyframeStyle>::create();

  for (const auto& keyframe : normalized_keyframes) {
    // Ignore CSS Transitions
    if (!keyframe.Get()->IsStringKeyframe())
      continue;
    const StringKeyframe* string_keyframe = ToStringKeyframe(keyframe.Get());
    keyframes->addItem(BuildObjectForStringKeyframe(string_keyframe));
  }
  return protocol::Animation::KeyframesRule::create()
      .setKeyframes(std::move(keyframes))
      .build();
}

std::unique_ptr<protocol::Animation::Animation>
InspectorAnimationAgent::BuildObjectForAnimation(blink::Animation& animation) {
  const Element* element =
      ToKeyframeEffectReadOnly(animation.effect())->Target();
  CSSAnimations& css_animations =
      element->GetElementAnimations()->CssAnimations();
  std::unique_ptr<protocol::Animation::KeyframesRule> keyframe_rule = nullptr;
  String animation_type;

  if (css_animations.IsTransitionAnimationForInspector(animation)) {
    // CSS Transitions
    animation_type = AnimationType::CSSTransition;
  } else {
    // Keyframe based animations
    keyframe_rule = BuildObjectForAnimationKeyframes(
        ToKeyframeEffectReadOnly(animation.effect()));
    animation_type = css_animations.IsAnimationForInspector(animation)
                         ? AnimationType::CSSAnimation
                         : AnimationType::WebAnimation;
  }

  String id = String::Number(animation.SequenceNumber());
  id_to_animation_.Set(id, &animation);
  id_to_animation_type_.Set(id, animation_type);

  std::unique_ptr<protocol::Animation::AnimationEffect>
      animation_effect_object = BuildObjectForAnimationEffect(
          ToKeyframeEffectReadOnly(animation.effect()),
          animation_type == AnimationType::CSSTransition);
  animation_effect_object->setKeyframesRule(std::move(keyframe_rule));

  std::unique_ptr<protocol::Animation::Animation> animation_object =
      protocol::Animation::Animation::create()
          .setId(id)
          .setName(animation.id())
          .setPausedState(animation.Paused())
          .setPlayState(animation.playState())
          .setPlaybackRate(animation.playbackRate())
          .setStartTime(NormalizedStartTime(animation))
          .setCurrentTime(animation.currentTime())
          .setSource(std::move(animation_effect_object))
          .setType(animation_type)
          .build();
  if (animation_type != AnimationType::WebAnimation)
    animation_object->setCssId(CreateCSSId(animation));
  return animation_object;
}

Response InspectorAnimationAgent::getPlaybackRate(double* playback_rate) {
  *playback_rate = ReferenceTimeline().PlaybackRate();
  return Response::OK();
}

Response InspectorAnimationAgent::setPlaybackRate(double playback_rate) {
  for (LocalFrame* frame : *inspected_frames_)
    frame->GetDocument()->Timeline().SetPlaybackRate(playback_rate);
  state_->setDouble(AnimationAgentState::animationAgentPlaybackRate,
                    playback_rate);
  return Response::OK();
}

Response InspectorAnimationAgent::getCurrentTime(const String& id,
                                                 double* current_time) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(id, animation);
  if (!response.isSuccess())
    return response;
  if (id_to_animation_clone_.at(id))
    animation = id_to_animation_clone_.at(id);

  if (animation->Paused()) {
    *current_time = animation->currentTime();
  } else {
    // Use startTime where possible since currentTime is limited.
    *current_time =
        animation->TimelineInternal()->currentTime() - animation->startTime();
  }
  return Response::OK();
}

Response InspectorAnimationAgent::setPaused(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    bool paused) {
  for (size_t i = 0; i < animation_ids->length(); ++i) {
    String animation_id = animation_ids->get(i);
    blink::Animation* animation = nullptr;
    Response response = AssertAnimation(animation_id, animation);
    if (!response.isSuccess())
      return response;
    blink::Animation* clone = AnimationClone(animation);
    if (!clone)
      return Response::Error("Failed to clone detached animation");
    if (paused && !clone->Paused()) {
      // Ensure we restore a current time if the animation is limited.
      double current_time =
          clone->TimelineInternal()->currentTime() - clone->startTime();
      clone->pause();
      clone->setCurrentTime(current_time, false);
    } else if (!paused && clone->Paused()) {
      clone->Unpause();
    }
  }
  return Response::OK();
}

blink::Animation* InspectorAnimationAgent::AnimationClone(
    blink::Animation* animation) {
  const String id = String::Number(animation->SequenceNumber());
  if (!id_to_animation_clone_.at(id)) {
    KeyframeEffectReadOnly* old_effect =
        ToKeyframeEffectReadOnly(animation->effect());
    DCHECK(old_effect->Model()->IsKeyframeEffectModel());
    KeyframeEffectModelBase* old_model =
        ToKeyframeEffectModelBase(old_effect->Model());
    EffectModel* new_model = nullptr;
    // Clone EffectModel.
    // TODO(samli): Determine if this is an animations bug.
    if (old_model->IsStringKeyframeEffectModel()) {
      StringKeyframeEffectModel* old_string_keyframe_model =
          ToStringKeyframeEffectModel(old_model);
      KeyframeVector old_keyframes = old_string_keyframe_model->GetFrames();
      StringKeyframeVector new_keyframes;
      for (auto& old_keyframe : old_keyframes)
        new_keyframes.push_back(ToStringKeyframe(old_keyframe.Get()));
      new_model = StringKeyframeEffectModel::Create(new_keyframes);
    } else if (old_model->IsAnimatableValueKeyframeEffectModel()) {
      AnimatableValueKeyframeEffectModel* old_animatable_value_keyframe_model =
          ToAnimatableValueKeyframeEffectModel(old_model);
      KeyframeVector old_keyframes =
          old_animatable_value_keyframe_model->GetFrames();
      AnimatableValueKeyframeVector new_keyframes;
      for (auto& old_keyframe : old_keyframes)
        new_keyframes.push_back(ToAnimatableValueKeyframe(old_keyframe.Get()));
      new_model = AnimatableValueKeyframeEffectModel::Create(new_keyframes);
    } else if (old_model->IsTransitionKeyframeEffectModel()) {
      TransitionKeyframeEffectModel* old_transition_keyframe_model =
          ToTransitionKeyframeEffectModel(old_model);
      KeyframeVector old_keyframes = old_transition_keyframe_model->GetFrames();
      TransitionKeyframeVector new_keyframes;
      for (auto& old_keyframe : old_keyframes)
        new_keyframes.push_back(ToTransitionKeyframe(old_keyframe.Get()));
      new_model = TransitionKeyframeEffectModel::Create(new_keyframes);
    }

    KeyframeEffect* new_effect = KeyframeEffect::Create(
        old_effect->Target(), new_model, old_effect->SpecifiedTiming());
    is_cloning_ = true;
    blink::Animation* clone =
        blink::Animation::Create(new_effect, animation->timeline());
    is_cloning_ = false;
    id_to_animation_clone_.Set(id, clone);
    id_to_animation_.Set(String::Number(clone->SequenceNumber()), clone);
    clone->play();
    clone->setStartTime(animation->startTime(), false);

    animation->SetEffectSuppressed(true);
  }
  return id_to_animation_clone_.at(id);
}

Response InspectorAnimationAgent::seekAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids,
    double current_time) {
  for (size_t i = 0; i < animation_ids->length(); ++i) {
    String animation_id = animation_ids->get(i);
    blink::Animation* animation = nullptr;
    Response response = AssertAnimation(animation_id, animation);
    if (!response.isSuccess())
      return response;
    blink::Animation* clone = AnimationClone(animation);
    if (!clone)
      return Response::Error("Failed to clone a detached animation.");
    if (!clone->Paused())
      clone->play();
    clone->setCurrentTime(current_time, false);
  }
  return Response::OK();
}

Response InspectorAnimationAgent::releaseAnimations(
    std::unique_ptr<protocol::Array<String>> animation_ids) {
  for (size_t i = 0; i < animation_ids->length(); ++i) {
    String animation_id = animation_ids->get(i);
    blink::Animation* animation = id_to_animation_.at(animation_id);
    if (animation)
      animation->SetEffectSuppressed(false);
    blink::Animation* clone = id_to_animation_clone_.at(animation_id);
    if (clone)
      clone->cancel();
    id_to_animation_clone_.erase(animation_id);
    id_to_animation_.erase(animation_id);
    id_to_animation_type_.erase(animation_id);
    cleared_animations_.insert(animation_id);
  }
  return Response::OK();
}

Response InspectorAnimationAgent::setTiming(const String& animation_id,
                                            double duration,
                                            double delay) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(animation_id, animation);
  if (!response.isSuccess())
    return response;

  animation = AnimationClone(animation);
  NonThrowableExceptionState exception_state;

  String type = id_to_animation_type_.at(animation_id);
  if (type == AnimationType::CSSTransition) {
    KeyframeEffect* effect = ToKeyframeEffect(animation->effect());
    KeyframeEffectModelBase* model = ToKeyframeEffectModelBase(effect->Model());
    const TransitionKeyframeEffectModel* old_model =
        ToTransitionKeyframeEffectModel(model);
    // Refer to CSSAnimations::calculateTransitionUpdateForProperty() for the
    // structure of transitions.
    const KeyframeVector& frames = old_model->GetFrames();
    DCHECK(frames.size() == 3);
    KeyframeVector new_frames;
    for (int i = 0; i < 3; i++)
      new_frames.push_back(ToTransitionKeyframe(frames[i]->Clone().Get()));
    // Update delay, represented by the distance between the first two
    // keyframes.
    new_frames[1]->SetOffset(delay / (delay + duration));
    model->SetFrames(new_frames);

    AnimationEffectTiming* timing = effect->timing();
    UnrestrictedDoubleOrString unrestricted_duration;
    unrestricted_duration.setUnrestrictedDouble(duration + delay);
    timing->setDuration(unrestricted_duration, exception_state);
  } else {
    AnimationEffectTiming* timing =
        ToAnimationEffectTiming(animation->effect()->timing());
    UnrestrictedDoubleOrString unrestricted_duration;
    unrestricted_duration.setUnrestrictedDouble(duration);
    timing->setDuration(unrestricted_duration, exception_state);
    timing->setDelay(delay);
  }
  return Response::OK();
}

Response InspectorAnimationAgent::resolveAnimation(
    const String& animation_id,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  blink::Animation* animation = nullptr;
  Response response = AssertAnimation(animation_id, animation);
  if (!response.isSuccess())
    return response;
  if (id_to_animation_clone_.at(animation_id))
    animation = id_to_animation_clone_.at(animation_id);
  const Element* element =
      ToKeyframeEffectReadOnly(animation->effect())->Target();
  Document* document = element->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  ScriptState* script_state =
      frame ? ToScriptStateForMainWorld(frame) : nullptr;
  if (!script_state)
    return Response::Error("Element not associated with a document.");

  ScriptState::Scope scope(script_state);
  static const char kAnimationObjectGroup[] = "animation";
  v8_session_->releaseObjectGroup(
      ToV8InspectorStringView(kAnimationObjectGroup));
  *result = v8_session_->wrapObject(
      script_state->GetContext(),
      ToV8(animation, script_state->GetContext()->Global(),
           script_state->GetIsolate()),
      ToV8InspectorStringView(kAnimationObjectGroup));
  if (!*result)
    return Response::Error("Element not associated with a document.");
  return Response::OK();
}

static CSSPropertyID g_animation_properties[] = {
    CSSPropertyAnimationDelay,          CSSPropertyAnimationDirection,
    CSSPropertyAnimationDuration,       CSSPropertyAnimationFillMode,
    CSSPropertyAnimationIterationCount, CSSPropertyAnimationName,
    CSSPropertyAnimationTimingFunction};

static CSSPropertyID g_transition_properties[] = {
    CSSPropertyTransitionDelay, CSSPropertyTransitionDuration,
    CSSPropertyTransitionProperty, CSSPropertyTransitionTimingFunction,
};

static void AddStringToDigestor(WebCryptoDigestor* digestor,
                                const String& string) {
  digestor->Consume(
      reinterpret_cast<const unsigned char*>(string.Ascii().data()),
      string.length());
}

String InspectorAnimationAgent::CreateCSSId(blink::Animation& animation) {
  String type =
      id_to_animation_type_.at(String::Number(animation.SequenceNumber()));
  DCHECK_NE(type, AnimationType::WebAnimation);

  KeyframeEffectReadOnly* effect = ToKeyframeEffectReadOnly(animation.effect());
  Vector<CSSPropertyID> css_properties;
  if (type == AnimationType::CSSAnimation) {
    for (CSSPropertyID property : g_animation_properties)
      css_properties.push_back(property);
  } else {
    for (CSSPropertyID property : g_transition_properties)
      css_properties.push_back(property);
    css_properties.push_back(cssPropertyID(animation.id()));
  }

  Element* element = effect->Target();
  HeapVector<Member<CSSStyleDeclaration>> styles =
      css_agent_->MatchingStyles(element);
  std::unique_ptr<WebCryptoDigestor> digestor =
      CreateDigestor(kHashAlgorithmSha1);
  AddStringToDigestor(digestor.get(), type);
  AddStringToDigestor(digestor.get(), animation.id());
  for (CSSPropertyID property : css_properties) {
    CSSStyleDeclaration* style =
        css_agent_->FindEffectiveDeclaration(property, styles);
    // Ignore inline styles.
    if (!style || !style->ParentStyleSheet() || !style->parentRule() ||
        style->parentRule()->type() != CSSRule::kStyleRule)
      continue;
    AddStringToDigestor(digestor.get(), getPropertyNameString(property));
    AddStringToDigestor(digestor.get(),
                        css_agent_->StyleSheetId(style->ParentStyleSheet()));
    AddStringToDigestor(digestor.get(),
                        ToCSSStyleRule(style->parentRule())->selectorText());
  }
  DigestValue digest_result;
  FinishDigestor(digestor.get(), digest_result);
  return Base64Encode(reinterpret_cast<const char*>(digest_result.data()), 10);
}

void InspectorAnimationAgent::DidCreateAnimation(unsigned sequence_number) {
  if (is_cloning_)
    return;
  GetFrontend()->animationCreated(String::Number(sequence_number));
}

void InspectorAnimationAgent::AnimationPlayStateChanged(
    blink::Animation* animation,
    blink::Animation::AnimationPlayState old_play_state,
    blink::Animation::AnimationPlayState new_play_state) {
  const String& animation_id = String::Number(animation->SequenceNumber());

  // We no longer care about animations that have been released.
  if (cleared_animations_.Contains(animation_id))
    return;

  // Record newly starting animations only once, as |buildObjectForAnimation|
  // constructs and caches our internal representation of the given |animation|.
  if ((new_play_state == blink::Animation::kRunning ||
       new_play_state == blink::Animation::kFinished) &&
      !id_to_animation_.Contains(animation_id))
    GetFrontend()->animationStarted(BuildObjectForAnimation(*animation));
  else if (new_play_state == blink::Animation::kIdle ||
           new_play_state == blink::Animation::kPaused)
    GetFrontend()->animationCanceled(animation_id);
}

void InspectorAnimationAgent::DidClearDocumentOfWindowObject(
    LocalFrame* frame) {
  if (!state_->booleanProperty(AnimationAgentState::animationAgentEnabled,
                               false))
    return;
  DCHECK(frame->GetDocument());
  frame->GetDocument()->Timeline().SetPlaybackRate(
      ReferenceTimeline().PlaybackRate());
}

Response InspectorAnimationAgent::AssertAnimation(const String& id,
                                                  blink::Animation*& result) {
  result = id_to_animation_.at(id);
  if (!result)
    return Response::Error("Could not find animation with given id");
  return Response::OK();
}

DocumentTimeline& InspectorAnimationAgent::ReferenceTimeline() {
  return inspected_frames_->Root()->GetDocument()->Timeline();
}

double InspectorAnimationAgent::NormalizedStartTime(
    blink::Animation& animation) {
  if (ReferenceTimeline().PlaybackRate() == 0) {
    return animation.startTime() + ReferenceTimeline().currentTime() -
           animation.TimelineInternal()->currentTime();
  }
  return animation.startTime() + (animation.TimelineInternal()->ZeroTime() -
                                  ReferenceTimeline().ZeroTime()) *
                                     1000 * ReferenceTimeline().PlaybackRate();
}

DEFINE_TRACE(InspectorAnimationAgent) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(css_agent_);
  visitor->Trace(id_to_animation_);
  visitor->Trace(id_to_animation_clone_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

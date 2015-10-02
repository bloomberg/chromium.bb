// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/inspector/InspectorAnimationAgent.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/ComputedTimingProperties.h"
#include "core/animation/EffectModel.h"
#include "core/animation/ElementAnimation.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DOMNodeIds.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "platform/Decimal.h"
#include "platform/animation/TimingFunction.h"

namespace AnimationAgentState {
static const char animationAgentEnabled[] = "animationAgentEnabled";
}

namespace blink {

InspectorAnimationAgent::InspectorAnimationAgent(InspectorPageAgent* pageAgent, InspectorDOMAgent* domAgent)
    : InspectorBaseAgent<InspectorAnimationAgent, InspectorFrontend::Animation>("Animation")
    , m_pageAgent(pageAgent)
    , m_domAgent(domAgent)
    , m_isCloning(false)
{
}

void InspectorAnimationAgent::restore()
{
    if (m_state->getBoolean(AnimationAgentState::animationAgentEnabled)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorAnimationAgent::enable(ErrorString*)
{
    m_state->setBoolean(AnimationAgentState::animationAgentEnabled, true);
    m_instrumentingAgents->setInspectorAnimationAgent(this);
}

void InspectorAnimationAgent::disable(ErrorString*)
{
    ASSERT(m_pageAgent);
    setPlaybackRate(nullptr, 1);
    m_state->setBoolean(AnimationAgentState::animationAgentEnabled, false);
    m_instrumentingAgents->setInspectorAnimationAgent(nullptr);
    m_idToAnimation.clear();
    m_idToAnimationType.clear();
    m_idToAnimationClone.clear();
}

void InspectorAnimationAgent::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (frame == m_pageAgent->inspectedFrame()) {
        m_idToAnimation.clear();
        m_idToAnimationType.clear();
        m_idToAnimationClone.clear();
    }
}

static PassRefPtr<TypeBuilder::Animation::AnimationEffect> buildObjectForAnimationEffect(KeyframeEffect* effect, bool isTransition)
{
    ComputedTimingProperties computedTiming;
    effect->computedTiming(computedTiming);
    double delay = computedTiming.delay();
    double duration = computedTiming.duration().getAsUnrestrictedDouble();
    String easing = effect->specifiedTiming().timingFunction->toString();

    if (isTransition) {
        // Obtain keyframes and convert keyframes back to delay
        ASSERT(effect->model()->isKeyframeEffectModel());
        const KeyframeEffectModelBase* model = toKeyframeEffectModelBase(effect->model());
        Vector<RefPtr<Keyframe>> keyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(model->getFrames());
        if (keyframes.size() == 3) {
            delay = keyframes.at(1)->offset() * duration;
            duration -= delay;
            easing = keyframes.at(1)->easing().toString();
        } else {
            easing = keyframes.at(0)->easing().toString();
        }
    }

    RefPtr<TypeBuilder::Animation::AnimationEffect> animationObject = TypeBuilder::Animation::AnimationEffect::create()
        .setDelay(delay)
        .setEndDelay(computedTiming.endDelay())
        .setPlaybackRate(computedTiming.playbackRate())
        .setIterationStart(computedTiming.iterationStart())
        .setIterations(computedTiming.iterations())
        .setDuration(duration)
        .setDirection(computedTiming.direction())
        .setFill(computedTiming.fill())
        .setName(effect->name())
        .setBackendNodeId(DOMNodeIds::idForNode(effect->target()))
        .setEasing(easing);
    return animationObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframeStyle> buildObjectForStringKeyframe(const StringKeyframe* keyframe)
{
    Decimal decimal = Decimal::fromDouble(keyframe->offset() * 100);
    String offset = decimal.toString();
    offset.append("%");

    RefPtr<TypeBuilder::Animation::KeyframeStyle> keyframeObject = TypeBuilder::Animation::KeyframeStyle::create()
        .setOffset(offset)
        .setEasing(keyframe->easing().toString());
    return keyframeObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframesRule> buildObjectForAnimationKeyframes(const KeyframeEffect* effect)
{
    if (!effect || !effect->model() || !effect->model()->isKeyframeEffectModel())
        return nullptr;
    const KeyframeEffectModelBase* model = toKeyframeEffectModelBase(effect->model());
    Vector<RefPtr<Keyframe>> normalizedKeyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(model->getFrames());
    RefPtr<TypeBuilder::Array<TypeBuilder::Animation::KeyframeStyle> > keyframes = TypeBuilder::Array<TypeBuilder::Animation::KeyframeStyle>::create();

    for (const auto& keyframe : normalizedKeyframes) {
        // Ignore CSS Transitions
        if (!keyframe.get()->isStringKeyframe())
            continue;
        const StringKeyframe* stringKeyframe = toStringKeyframe(keyframe.get());
        keyframes->addItem(buildObjectForStringKeyframe(stringKeyframe));
    }
    RefPtr<TypeBuilder::Animation::KeyframesRule> keyframesObject = TypeBuilder::Animation::KeyframesRule::create()
        .setKeyframes(keyframes);
    return keyframesObject.release();
}

PassRefPtr<TypeBuilder::Animation::Animation> InspectorAnimationAgent::buildObjectForAnimation(Animation& animation)
{
    const Element* element = toKeyframeEffect(animation.effect())->target();
    CSSAnimations& cssAnimations = element->elementAnimations()->cssAnimations();
    RefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr;
    AnimationType animationType;

    if (cssAnimations.isTransitionAnimationForInspector(animation)) {
        // CSS Transitions
        animationType = AnimationType::CSSTransition;
    } else {
        // Keyframe based animations
        keyframeRule = buildObjectForAnimationKeyframes(toKeyframeEffect(animation.effect()));
        animationType = cssAnimations.isAnimationForInspector(animation) ? AnimationType::CSSAnimation : AnimationType::WebAnimation;
    }

    String id = String::number(animation.sequenceNumber());
    m_idToAnimation.set(id, &animation);
    m_idToAnimationType.set(id, animationType);

    RefPtr<TypeBuilder::Animation::AnimationEffect> animationEffectObject = buildObjectForAnimationEffect(toKeyframeEffect(animation.effect()), animationType == AnimationType::CSSTransition);
    if (keyframeRule)
        animationEffectObject->setKeyframesRule(keyframeRule);

    RefPtr<TypeBuilder::Animation::Animation> animationObject = TypeBuilder::Animation::Animation::create()
        .setId(id)
        .setPausedState(animation.paused())
        .setPlayState(animation.playState())
        .setPlaybackRate(animation.playbackRate())
        .setStartTime(normalizedStartTime(animation))
        .setCurrentTime(animation.currentTime())
        .setSource(animationEffectObject.release())
        .setType(animationType);
    return animationObject.release();
}

void InspectorAnimationAgent::getPlaybackRate(ErrorString*, double* playbackRate)
{
    *playbackRate = referenceTimeline().playbackRate();
}

void InspectorAnimationAgent::setPlaybackRate(ErrorString*, double playbackRate)
{
    for (Frame* frame = m_pageAgent->inspectedFrame(); frame; frame = frame->tree().traverseNext(m_pageAgent->inspectedFrame())) {
        if (frame->isLocalFrame())
            toLocalFrame(frame)->document()->timeline().setPlaybackRate(playbackRate);
    }
}

void InspectorAnimationAgent::getCurrentTime(ErrorString* errorString, const String& id, double* currentTime)
{
    Animation* animation = assertAnimation(errorString, id);
    if (m_idToAnimationClone.get(id))
        animation = m_idToAnimationClone.get(id);
    *currentTime = animation->timeline()->currentTime() - animation->startTime();
}

Animation* InspectorAnimationAgent::animationClone(Animation* animation)
{
    const String id = String::number(animation->sequenceNumber());
    if (!m_idToAnimationClone.get(id)) {
        // TODO(samli): Clone the AnimationEffect as well.
        Animation* clone = Animation::create(animation->effect(), animation->timeline());
        m_idToAnimationClone.set(id, clone);
        m_idToAnimation.set(String::number(clone->sequenceNumber()), clone);
    }
    return m_idToAnimationClone.get(id);
}

void InspectorAnimationAgent::seekAnimations(ErrorString* errorString, const RefPtr<JSONArray>& animationIds, double currentTime)
{
    for (const auto& id : *animationIds) {
        String animationId;
        if (!(id->asString(&animationId))) {
            *errorString = "Invalid argument type";
            return;
        }
        Animation* animation = assertAnimation(errorString, animationId);
        if (!animation)
            return;
        m_isCloning = true;
        Animation* clone = animationClone(animation);
        m_isCloning = false;
        clone->play();
        clone->setCurrentTime(currentTime);
    }
}

void InspectorAnimationAgent::setTiming(ErrorString* errorString, const String& animationId, double duration, double delay)
{
    Animation* animation = assertAnimation(errorString, animationId);
    if (!animation)
        return;

    AnimationType type = m_idToAnimationType.get(animationId);
    if (type == AnimationType::CSSTransition) {
        KeyframeEffect* effect = toKeyframeEffect(animation->effect());
        KeyframeEffectModelBase* model = toKeyframeEffectModelBase(effect->model());
        const AnimatableValueKeyframeEffectModel* oldModel = toAnimatableValueKeyframeEffectModel(model);
        // Refer to CSSAnimations::calculateTransitionUpdateForProperty() for the structure of transitions.
        const KeyframeVector& frames = oldModel->getFrames();
        ASSERT(frames.size() == 3);
        KeyframeVector newFrames;
        for (int i = 0; i < 3; i++)
            newFrames.append(toAnimatableValueKeyframe(frames[i]->clone().get()));
        // Update delay, represented by the distance between the first two keyframes.
        newFrames[1]->setOffset(delay / (delay + duration));
        model->setFrames(newFrames);

        AnimationEffectTiming* timing = animation->effect()->timing();
        UnrestrictedDoubleOrString unrestrictedDuration;
        unrestrictedDuration.setUnrestrictedDouble(duration + delay);
        timing->setDuration(unrestrictedDuration);
    } else if (type == AnimationType::WebAnimation) {
        AnimationEffectTiming* timing = animation->effect()->timing();
        UnrestrictedDoubleOrString unrestrictedDuration;
        unrestrictedDuration.setUnrestrictedDouble(duration);
        timing->setDuration(unrestrictedDuration);
        timing->setDelay(delay);
    }
}

void InspectorAnimationAgent::didCreateAnimation(unsigned sequenceNumber)
{
    if (m_isCloning)
        return;
    frontend()->animationCreated(String::number(sequenceNumber));
}

void InspectorAnimationAgent::didStartAnimation(Animation* animation)
{
    const String& animationId = String::number(animation->sequenceNumber());
    if (m_idToAnimation.get(animationId))
        return;
    frontend()->animationStarted(buildObjectForAnimation(*animation));
}

void InspectorAnimationAgent::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    if (!m_state->getBoolean(AnimationAgentState::animationAgentEnabled))
        return;
    ASSERT(frame->document());
    frame->document()->timeline().setPlaybackRate(referenceTimeline().playbackRate());
}

Animation* InspectorAnimationAgent::assertAnimation(ErrorString* errorString, const String& id)
{
    Animation* animation = m_idToAnimation.get(id);
    if (!animation) {
        *errorString = "Could not find animation with given id";
        return nullptr;
    }
    return animation;
}

AnimationTimeline& InspectorAnimationAgent::referenceTimeline()
{
    return m_pageAgent->inspectedFrame()->document()->timeline();
}

double InspectorAnimationAgent::normalizedStartTime(Animation& animation)
{
    if (referenceTimeline().playbackRate() == 0)
        return animation.startTime() + referenceTimeline().currentTime() - animation.timeline()->currentTime();
    return animation.startTime() + (animation.timeline()->zeroTime() - referenceTimeline().zeroTime()) * 1000 * referenceTimeline().playbackRate();
}

DEFINE_TRACE(InspectorAnimationAgent)
{
#if ENABLE(OILPAN)
    visitor->trace(m_pageAgent);
    visitor->trace(m_domAgent);
    visitor->trace(m_idToAnimation);
    visitor->trace(m_idToAnimationType);
    visitor->trace(m_idToAnimationClone);
#endif
    InspectorBaseAgent::trace(visitor);
}

}

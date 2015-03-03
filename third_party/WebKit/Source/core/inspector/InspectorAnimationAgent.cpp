// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/inspector/InspectorAnimationAgent.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/AnimationNode.h"
#include "core/animation/AnimationPlayer.h"
#include "core/animation/ComputedTimingProperties.h"
#include "core/animation/ElementAnimation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorNodeIds.h"
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
    : InspectorBaseAgent<InspectorAnimationAgent>("Animation")
    , m_pageAgent(pageAgent)
    , m_domAgent(domAgent)
    , m_frontend(nullptr)
{
}

void InspectorAnimationAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->animation();
}

void InspectorAnimationAgent::clearFrontend()
{
    m_instrumentingAgents->setInspectorAnimationAgent(nullptr);
    m_frontend = nullptr;
    reset();
}

void InspectorAnimationAgent::reset()
{
    m_idToAnimationPlayer.clear();
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

static PassRefPtr<TypeBuilder::Animation::AnimationNode> buildObjectForAnimation(Animation* animation, bool isTransition)
{
    ComputedTimingProperties computedTiming;
    animation->computedTiming(computedTiming);
    double delay = computedTiming.delay();
    double duration = computedTiming.duration().getAsUnrestrictedDouble();
    String easing = animation->specifiedTiming().timingFunction->toString();

    if (isTransition) {
        // Obtain keyframes and convert keyframes back to delay
        ASSERT(animation->effect()->isKeyframeEffectModel());
        const KeyframeEffectModelBase* effect = toKeyframeEffectModelBase(animation->effect());
        WillBeHeapVector<RefPtrWillBeMember<Keyframe> > keyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(effect->getFrames());
        if (keyframes.size() == 3) {
            delay = keyframes.at(1)->offset() * duration;
            duration -= delay;
            easing = keyframes.at(1)->easing().toString();
        } else {
            easing = keyframes.at(0)->easing().toString();
        }
    }

    RefPtr<TypeBuilder::Animation::AnimationNode> animationObject = TypeBuilder::Animation::AnimationNode::create()
        .setDelay(delay)
        .setPlaybackRate(computedTiming.playbackRate())
        .setIterationStart(computedTiming.iterationStart())
        .setIterations(computedTiming.iterations())
        .setDuration(duration)
        .setDirection(computedTiming.direction())
        .setFill(computedTiming.fill())
        .setName(animation->name())
        .setBackendNodeId(InspectorNodeIds::idForNode(animation->target()))
        .setEasing(easing);
    return animationObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframeStyle> buildObjectForStyleRuleKeyframe(StyleRuleKeyframe* keyframe, TimingFunction& easing)
{
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), keyframe->mutableProperties().ensureCSSStyleDeclaration(), 0);
    RefPtr<TypeBuilder::Animation::KeyframeStyle> keyframeObject = TypeBuilder::Animation::KeyframeStyle::create()
        .setOffset(keyframe->keyText())
        .setStyle(inspectorStyle->buildObjectForStyle())
        .setEasing(easing.toString());
    return keyframeObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframeStyle> buildObjectForStringKeyframe(const StringKeyframe* keyframe)
{
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), keyframe->propertySetForInspector().get()->ensureCSSStyleDeclaration(), 0);
    Decimal decimal = Decimal::fromDouble(keyframe->offset() * 100);
    String offset = decimal.toString();
    offset.append("%");

    RefPtr<TypeBuilder::Animation::KeyframeStyle> keyframeObject = TypeBuilder::Animation::KeyframeStyle::create()
        .setOffset(offset)
        .setStyle(inspectorStyle->buildObjectForStyle())
        .setEasing(keyframe->easing().toString());
    return keyframeObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframesRule> buildObjectForStyleRuleKeyframes(const AnimationPlayer& player, const StyleRuleKeyframes* keyframesRule)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Animation::KeyframeStyle> > keyframes = TypeBuilder::Array<TypeBuilder::Animation::KeyframeStyle>::create();
    const WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe> >& styleKeyframes = keyframesRule->keyframes();
    for (const auto& styleKeyframe : styleKeyframes) {
        WillBeHeapVector<RefPtrWillBeMember<Keyframe> > normalizedKeyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(toKeyframeEffectModelBase(toAnimation(player.source())->effect())->getFrames());
        TimingFunction* easing = nullptr;
        for (const auto& keyframe : normalizedKeyframes) {
            if (styleKeyframe->keys().contains(keyframe->offset()))
                easing = &keyframe->easing();
        }
        ASSERT(easing);
        keyframes->addItem(buildObjectForStyleRuleKeyframe(styleKeyframe.get(), *easing));
    }

    RefPtr<TypeBuilder::Animation::KeyframesRule> keyframesObject = TypeBuilder::Animation::KeyframesRule::create()
        .setKeyframes(keyframes);
    keyframesObject->setName(keyframesRule->name());
    return keyframesObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframesRule> buildObjectForAnimationKeyframes(const Animation* animation)
{
    if (!animation->effect()->isKeyframeEffectModel())
        return nullptr;
    const KeyframeEffectModelBase* effect = toKeyframeEffectModelBase(animation->effect());
    WillBeHeapVector<RefPtrWillBeMember<Keyframe> > normalizedKeyframes = KeyframeEffectModelBase::normalizedKeyframesForInspector(effect->getFrames());
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

PassRefPtr<TypeBuilder::Animation::AnimationPlayer> InspectorAnimationAgent::buildObjectForAnimationPlayer(AnimationPlayer& player)
{
    // Find type of animation
    const Element* element = toAnimation(player.source())->target();
    StyleResolver& styleResolver = element->ownerDocument()->ensureStyleResolver();
    CSSAnimations& cssAnimations = element->elementAnimations()->cssAnimations();
    const AtomicString animationName = cssAnimations.getAnimationNameForInspector(player);
    RefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr;
    AnimationType animationType;

    if (!animationName.isNull()) {
        // CSS Animations
        const StyleRuleKeyframes* keyframes = styleResolver.findKeyframesRule(element, animationName);
        keyframeRule = buildObjectForStyleRuleKeyframes(player, keyframes);
        animationType = AnimationType::CSSAnimation;
    } else if (cssAnimations.isTransitionAnimationForInspector(player)) {
        // CSS Transitions
        animationType = AnimationType::CSSTransition;
    } else {
        // Web Animations
        keyframeRule = buildObjectForAnimationKeyframes(toAnimation(player.source()));
        animationType = AnimationType::WebAnimation;
    }

    RefPtr<TypeBuilder::Animation::AnimationNode> animationObject = buildObjectForAnimation(toAnimation(player.source()), animationType == AnimationType::CSSTransition);
    if (keyframeRule)
        animationObject->setKeyframesRule(keyframeRule);

    RefPtr<TypeBuilder::Animation::AnimationPlayer> playerObject = TypeBuilder::Animation::AnimationPlayer::create()
        .setId(String::number(player.sequenceNumber()))
        .setPausedState(player.paused())
        .setPlayState(player.playState())
        .setPlaybackRate(player.playbackRate())
        .setStartTime(player.startTime())
        .setCurrentTime(player.currentTime())
        .setSource(animationObject.release())
        .setType(animationType);
    return playerObject.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> > InspectorAnimationAgent::buildArrayForAnimationPlayers(Element& element, const WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer> > players)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> > animationPlayersArray = TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer>::create();
    for (const auto& it : players) {
        AnimationPlayer& player = *(it.get());
        Animation* animation = toAnimation(player.source());
        if (!element.contains(animation->target()))
            continue;
        animationPlayersArray->addItem(buildObjectForAnimationPlayer(player));
    }
    return animationPlayersArray.release();
}

void InspectorAnimationAgent::getAnimationPlayersForNode(ErrorString* errorString, int nodeId, bool includeSubtreeAnimations, RefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> >& animationPlayersArray)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;
    WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer> > players;
    if (!includeSubtreeAnimations)
        players = ElementAnimation::getAnimationPlayers(*element);
    else
        players = element->ownerDocument()->timeline().getAnimationPlayers();
    animationPlayersArray = buildArrayForAnimationPlayers(*element, players);
}

void InspectorAnimationAgent::getPlaybackRate(ErrorString*, double* playbackRate)
{
    *playbackRate = m_pageAgent->inspectedFrame()->document()->timeline().playbackRate();
}

void InspectorAnimationAgent::setPlaybackRate(ErrorString*, double playbackRate)
{
    for (Frame* frame = m_pageAgent->inspectedFrame(); frame; frame = frame->tree().traverseNext(m_pageAgent->inspectedFrame())) {
        if (frame->isLocalFrame())
            toLocalFrame(frame)->document()->timeline().setPlaybackRate(playbackRate);
    }
}

void InspectorAnimationAgent::setCurrentTime(ErrorString*, double currentTime)
{
    m_pageAgent->inspectedFrame()->document()->timeline().setCurrentTime(currentTime);
}

void InspectorAnimationAgent::didCreateAnimationPlayer(AnimationPlayer& player)
{
    const String& playerId = String::number(player.sequenceNumber());
    if (m_idToAnimationPlayer.get(playerId))
        return;

    // Check threshold
    double latestStartTime = 0;
    for (const auto& p : m_idToAnimationPlayer.values())
        latestStartTime = max(latestStartTime, p->startTime());

    bool reset = false;
    if (player.startTime() - latestStartTime > 1000) {
        reset = true;
        m_idToAnimationPlayer.clear();
    }

    m_idToAnimationPlayer.set(playerId, &player);
    m_frontend->animationPlayerCreated(buildObjectForAnimationPlayer(player), reset);
}

AnimationPlayer* InspectorAnimationAgent::assertAnimationPlayer(ErrorString* errorString, const String& id)
{
    AnimationPlayer* player = m_idToAnimationPlayer.get(id);
    if (!player) {
        *errorString = "Could not find animation player with given id";
        return nullptr;
    }
    return player;
}

DEFINE_TRACE(InspectorAnimationAgent)
{
#if ENABLE(OILPAN)
    visitor->trace(m_pageAgent);
    visitor->trace(m_domAgent);
    visitor->trace(m_idToAnimationPlayer);
#endif
    InspectorBaseAgent::trace(visitor);
}

}

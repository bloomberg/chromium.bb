// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/inspector/InspectorAnimationAgent.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/AnimationNode.h"
#include "core/animation/AnimationPlayer.h"
#include "core/animation/ElementAnimation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "platform/Decimal.h"

namespace blink {

InspectorAnimationAgent::InspectorAnimationAgent(InspectorDOMAgent* domAgent)
    : InspectorBaseAgent<InspectorAnimationAgent>("Animation")
    , m_domAgent(domAgent)
    , m_frontend(0)
{
}

void InspectorAnimationAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->animation();
}

void InspectorAnimationAgent::clearFrontend()
{
    m_frontend = nullptr;
    reset();
}

void InspectorAnimationAgent::reset()
{
    m_idToAnimationPlayer.clear();
}

static PassRefPtr<TypeBuilder::Animation::AnimationNode> buildObjectForAnimationNode(AnimationNode* animationNode)
{
    RefPtr<TypeBuilder::Animation::AnimationNode> animationObject = TypeBuilder::Animation::AnimationNode::create()
        .setStartDelay(animationNode->specifiedTiming().startDelay)
        .setPlaybackRate(animationNode->specifiedTiming().playbackRate)
        .setIterationStart(animationNode->specifiedTiming().iterationStart)
        .setIterationCount(animationNode->specifiedTiming().iterationCount)
        .setDuration(animationNode->duration())
        .setDirection(animationNode->specifiedTiming().direction)
        .setFillMode(animationNode->specifiedTiming().fillMode)
        .setTimeFraction(animationNode->timeFraction())
        .setName(animationNode->name());
    return animationObject.release();
}

static String playerId(AnimationPlayer& player)
{
    return String::number(player.sequenceNumber());
}

static PassRefPtr<TypeBuilder::Animation::AnimationPlayer> buildObjectForAnimationPlayer(AnimationPlayer& animationPlayer, PassRefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr)
{
    RefPtr<TypeBuilder::Animation::AnimationNode> animationObject = buildObjectForAnimationNode(animationPlayer.source());
    if (keyframeRule)
        animationObject->setKeyframesRule(keyframeRule);

    RefPtr<TypeBuilder::Animation::AnimationPlayer> playerObject = TypeBuilder::Animation::AnimationPlayer::create()
        .setId(playerId(animationPlayer))
        .setPausedState(animationPlayer.paused())
        .setPlayState(animationPlayer.playState())
        .setPlaybackRate(animationPlayer.playbackRate())
        .setStartTime(animationPlayer.startTime())
        .setCurrentTime(animationPlayer.currentTime())
        .setSource(animationObject.release());
    return playerObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframeStyle> buildObjectForStyleKeyframe(StyleKeyframe* keyframe)
{
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), keyframe->mutableProperties().ensureCSSStyleDeclaration(), 0);
    RefPtr<TypeBuilder::Animation::KeyframeStyle> keyframeObject = TypeBuilder::Animation::KeyframeStyle::create()
        .setOffset(keyframe->keyText())
        .setStyle(inspectorStyle->buildObjectForStyle());
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
        .setStyle(inspectorStyle->buildObjectForStyle());
    return keyframeObject.release();
}

static PassRefPtr<TypeBuilder::Animation::KeyframesRule> buildObjectForStyleRuleKeyframes(const StyleRuleKeyframes* keyframesRule)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Animation::KeyframeStyle> > keyframes = TypeBuilder::Array<TypeBuilder::Animation::KeyframeStyle>::create();
    const WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> >& styleKeyframes = keyframesRule->keyframes();
    for (const auto& styleKeyframe : styleKeyframes)
        keyframes->addItem(buildObjectForStyleKeyframe(styleKeyframe.get()));

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

static PassRefPtr<TypeBuilder::Animation::KeyframesRule> buildObjectForKeyframesRule(const Element& element, const AnimationPlayer& player)
{
    StyleResolver& styleResolver = element.ownerDocument()->ensureStyleResolver();
    CSSAnimations& cssAnimations = element.activeAnimations()->cssAnimations();
    const AtomicString animationName = cssAnimations.getAnimationNameForInspector(player);
    RefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule;

    if (!animationName.isNull()) {
        // CSS Animations
        const StyleRuleKeyframes* keyframes = cssAnimations.matchScopedKeyframesRule(&styleResolver, &element, animationName.impl());
        keyframeRule = buildObjectForStyleRuleKeyframes(keyframes);
    } else {
        // Web Animations
        keyframeRule = buildObjectForAnimationKeyframes(toAnimation(player.source()));
    }

    return keyframeRule;
}

void InspectorAnimationAgent::getAnimationPlayersForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> >& animationPlayersArray)
{
    animationPlayersArray = TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer>::create();
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;
    m_idToAnimationPlayer.clear();
    WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer> > players = ElementAnimation::getAnimationPlayers(*element);
    for (WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer> >::iterator it = players.begin(); it != players.end(); ++it) {
        AnimationPlayer& player = *(it->get());
        m_idToAnimationPlayer.set(playerId(player), &player);
        RefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = buildObjectForKeyframesRule(*element, player);
        animationPlayersArray->addItem(buildObjectForAnimationPlayer(player, keyframeRule));
    }
}

void InspectorAnimationAgent::pauseAnimationPlayer(ErrorString* errorString, const String& id, RefPtr<TypeBuilder::Animation::AnimationPlayer>& animationPlayer)
{
    AnimationPlayer* player = assertAnimationPlayer(errorString, id);
    if (!player)
        return;
    player->pause();
    animationPlayer = buildObjectForAnimationPlayer(*player);
}

void InspectorAnimationAgent::playAnimationPlayer(ErrorString* errorString, const String& id, RefPtr<TypeBuilder::Animation::AnimationPlayer>& animationPlayer)
{
    AnimationPlayer* player = assertAnimationPlayer(errorString, id);
    if (!player)
        return;
    player->play();
    animationPlayer = buildObjectForAnimationPlayer(*player);
}

void InspectorAnimationAgent::setAnimationPlayerCurrentTime(ErrorString* errorString, const String& id, double currentTime, RefPtr<TypeBuilder::Animation::AnimationPlayer>& animationPlayer)
{
    AnimationPlayer* player = assertAnimationPlayer(errorString, id);
    if (!player)
        return;
    player->setCurrentTime(currentTime);
    animationPlayer = buildObjectForAnimationPlayer(*player);
}

void InspectorAnimationAgent::getAnimationPlayerState(ErrorString* errorString, const String& id, double* currentTime, bool* isRunning)
{
    AnimationPlayer* player = assertAnimationPlayer(errorString, id);
    if (!player)
        return;
    *currentTime = player->currentTime();
    *isRunning = player->playing();
}

AnimationPlayer* InspectorAnimationAgent::assertAnimationPlayer(ErrorString* errorString, const String& id)
{
    AnimationPlayer* player = m_idToAnimationPlayer.get(id);
    if (!player) {
        *errorString = "Could not find animation player with given id";
        return 0;
    }
    return player;
}

void InspectorAnimationAgent::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_idToAnimationPlayer);
    visitor->trace(m_domAgent);
#endif
    InspectorBaseAgent::trace(visitor);
}

}

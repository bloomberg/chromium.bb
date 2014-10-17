// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/inspector/InspectorAnimationAgent.h"

#include "core/animation/AnimationNode.h"
#include "core/animation/AnimationPlayer.h"
#include "core/animation/ElementAnimation.h"
#include "core/inspector/InspectorDOMAgent.h"

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
}

void InspectorAnimationAgent::reset()
{
    m_idToAnimationPlayer.clear();
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
        RefPtr<TypeBuilder::Animation::AnimationPlayer> animationPlayerObject = buildObjectForAnimationPlayer(player);
        animationPlayersArray->addItem(animationPlayerObject);
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

String InspectorAnimationAgent::playerId(AnimationPlayer& player)
{
    return String::number(player.sequenceNumber());
}

PassRefPtr<TypeBuilder::Animation::AnimationPlayer> InspectorAnimationAgent::buildObjectForAnimationPlayer(AnimationPlayer& animationPlayer)
{
    RefPtr<TypeBuilder::Animation::AnimationPlayer> playerObject = TypeBuilder::Animation::AnimationPlayer::create()
        .setId(playerId(animationPlayer))
        .setPausedState(animationPlayer.paused())
        .setPlayState(animationPlayer.playState())
        .setPlaybackRate(animationPlayer.playbackRate())
        .setStartTime(animationPlayer.startTime())
        .setCurrentTime(animationPlayer.currentTime())
        .setSource(buildObjectForAnimationNode(*(animationPlayer.source())));
    return playerObject.release();
}

PassRefPtr<TypeBuilder::Animation::AnimationNode> InspectorAnimationAgent::buildObjectForAnimationNode(AnimationNode& animationNode)
{
    RefPtr<TypeBuilder::Animation::AnimationNode> animationObject = TypeBuilder::Animation::AnimationNode::create()
        .setStartDelay(animationNode.specifiedTiming().startDelay)
        .setPlaybackRate(animationNode.specifiedTiming().playbackRate)
        .setIterationStart(animationNode.specifiedTiming().iterationStart)
        .setIterationCount(animationNode.specifiedTiming().iterationCount)
        .setDuration(animationNode.duration())
        .setDirection(animationNode.specifiedTiming().direction)
        .setFillMode(animationNode.specifiedTiming().fillMode)
        .setTimeFraction(animationNode.timeFraction())
        .setName(animationNode.name());
    return animationObject.release();
}

void InspectorAnimationAgent::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_idToAnimationPlayer);
#endif
    InspectorBaseAgent::trace(visitor);
}

}

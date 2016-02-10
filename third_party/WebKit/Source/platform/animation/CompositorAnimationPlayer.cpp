// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "cc/animation/animation_id_provider.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/WebToCCAnimationDelegateAdapter.h"
#include "public/platform/WebLayer.h"

namespace blink {

CompositorAnimationPlayer::CompositorAnimationPlayer()
    : m_animationPlayer(cc::AnimationPlayer::Create(cc::AnimationIdProvider::NextPlayerId()))
{
}

CompositorAnimationPlayer::~CompositorAnimationPlayer()
{
}

cc::AnimationPlayer* CompositorAnimationPlayer::animationPlayer() const
{
    return m_animationPlayer.get();
}

void CompositorAnimationPlayer::setAnimationDelegate(WebCompositorAnimationDelegate* delegate)
{
    if (!delegate) {
        m_animationDelegateAdapter.reset();
        m_animationPlayer->set_layer_animation_delegate(nullptr);
        return;
    }
    m_animationDelegateAdapter.reset(new WebToCCAnimationDelegateAdapter(delegate));
    m_animationPlayer->set_layer_animation_delegate(m_animationDelegateAdapter.get());
}

void CompositorAnimationPlayer::attachLayer(WebLayer* webLayer)
{
    m_animationPlayer->AttachLayer(webLayer->id());
}

void CompositorAnimationPlayer::detachLayer()
{
    m_animationPlayer->DetachLayer();
}

bool CompositorAnimationPlayer::isLayerAttached() const
{
    return m_animationPlayer->layer_id() != 0;
}

void CompositorAnimationPlayer::addAnimation(CompositorAnimation* animation)
{
    m_animationPlayer->AddAnimation(animation->passAnimation());
    delete animation;
}

void CompositorAnimationPlayer::removeAnimation(int animationId)
{
    m_animationPlayer->RemoveAnimation(animationId);
}

void CompositorAnimationPlayer::pauseAnimation(int animationId, double timeOffset)
{
    m_animationPlayer->PauseAnimation(animationId, timeOffset);
}

void CompositorAnimationPlayer::abortAnimation(int animationId)
{
    m_animationPlayer->AbortAnimation(animationId);
}

} // namespace blink

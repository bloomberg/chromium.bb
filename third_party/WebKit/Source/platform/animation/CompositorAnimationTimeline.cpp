// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationTimeline.h"

#include "cc/animation/animation_id_provider.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"

namespace blink {

CompositorAnimationTimeline::CompositorAnimationTimeline()
    : m_animationTimeline(cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId()))
{
}

CompositorAnimationTimeline::~CompositorAnimationTimeline()
{
}

cc::AnimationTimeline* CompositorAnimationTimeline::animationTimeline() const
{
    return m_animationTimeline.get();
}

void CompositorAnimationTimeline::playerAttached(const blink::CompositorAnimationPlayerClient& client)
{
    if (client.compositorPlayer())
        m_animationTimeline->AttachPlayer(client.compositorPlayer()->animationPlayer());
}

void CompositorAnimationTimeline::playerDestroyed(const blink::CompositorAnimationPlayerClient& client)
{
    if (client.compositorPlayer())
        m_animationTimeline->DetachPlayer(client.compositorPlayer()->animationPlayer());
}

} // namespace blink

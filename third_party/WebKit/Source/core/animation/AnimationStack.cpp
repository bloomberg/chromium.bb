/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/AnimationStack.h"

#include "core/animation/Interpolation.h"
#include "core/animation/css/CSSAnimations.h"

namespace WebCore {

namespace {

void copyToActiveInterpolationMap(const WillBeHeapVector<RefPtrWillBeMember<WebCore::Interpolation> >& source, WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<WebCore::Interpolation> >& target)
{
    for (WillBeHeapVector<RefPtrWillBeMember<WebCore::Interpolation> >::const_iterator iter = source.begin(); iter != source.end(); ++iter) {
        RefPtrWillBeRawPtr<WebCore::Interpolation> interpolation = *iter;
        WebCore::StyleInterpolation *styleInterpolation = toStyleInterpolation(interpolation.get());
        target.set(styleInterpolation->id(), styleInterpolation);
    }
}

bool compareAnimations(Animation* animation1, Animation* animation2)
{
    ASSERT(animation1->player() && animation2->player());
    return AnimationPlayer::hasLowerPriority(animation1->player(), animation2->player());
}

void copyNewAnimationsToActiveInterpolationMap(const Vector<InertAnimation*>& newAnimations, WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> >& result)
{
    for (size_t i = 0; i < newAnimations.size(); ++i) {
        OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > sample = newAnimations[i]->sample(0);
        if (sample) {
            copyToActiveInterpolationMap(*sample, result);
        }
    }
}

} // namespace

AnimationStack::AnimationStack()
{
}

bool AnimationStack::affects(CSSPropertyID property) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->affects(property))
            return true;
    }
    return false;
}

bool AnimationStack::hasActiveAnimationsOnCompositor(CSSPropertyID property) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->hasActiveAnimationsOnCompositor(property))
            return true;
    }
    return false;
}

WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > AnimationStack::activeInterpolations(AnimationStack* animationStack, const Vector<InertAnimation*>* newAnimations, const HashSet<const AnimationPlayer*>* cancelledAnimationPlayers, Animation::Priority priority, double timelineCurrentTime)
{
    // We don't exactly know when new animations will start, but timelineCurrentTime is a good estimate.

    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > result;

    if (animationStack) {
        Vector<Animation*>& animations = animationStack->m_activeAnimations;
        std::sort(animations.begin(), animations.end(), compareAnimations);
        for (size_t i = 0; i < animations.size(); ++i) {
            Animation* animation = animations[i];
            if (animation->priority() != priority)
                continue;
            if (cancelledAnimationPlayers && cancelledAnimationPlayers->contains(animation->player()))
                continue;
            if (animation->player()->startTime() > timelineCurrentTime && newAnimations) {
                copyNewAnimationsToActiveInterpolationMap(*newAnimations, result);
                newAnimations = 0;
            }
            copyToActiveInterpolationMap(animation->activeInterpolations(), result);
        }
    }

    if (newAnimations)
        copyNewAnimationsToActiveInterpolationMap(*newAnimations, result);

    return result;
}

} // namespace WebCore

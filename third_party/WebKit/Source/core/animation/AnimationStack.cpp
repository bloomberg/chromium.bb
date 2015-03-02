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

#include "core/animation/CompositorAnimations.h"
#include "core/animation/StyleInterpolation.h"
#include "core/animation/css/CSSAnimations.h"
#include "wtf/BitArray.h"
#include "wtf/NonCopyingSort.h"
#include <algorithm>

namespace blink {

namespace {

void copyToActiveInterpolationMap(const WillBeHeapVector<RefPtrWillBeMember<blink::Interpolation> >& source, WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<blink::Interpolation> >& target)
{
    for (const auto& interpolation : source) {
        target.set(toStyleInterpolation(interpolation.get())->id(), interpolation.get());
    }
}

bool compareEffects(const OwnPtrWillBeMember<SampledEffect>& effect1, const OwnPtrWillBeMember<SampledEffect>& effect2)
{
    ASSERT(effect1 && effect2);
    return effect1->sequenceNumber() < effect2->sequenceNumber();
}

void copyNewAnimationsToActiveInterpolationMap(const WillBeHeapVector<RawPtrWillBeMember<InertAnimation> >& newAnimations, WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> >& result)
{
    for (const auto& newAnimation : newAnimations) {
        OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>> sample = nullptr;
        newAnimation->sample(sample);
        if (sample)
            copyToActiveInterpolationMap(*sample, result);
    }
}

} // namespace

AnimationStack::AnimationStack()
{
}

bool AnimationStack::hasActiveAnimationsOnCompositor(CSSPropertyID property) const
{
    for (const auto& effect : m_effects) {
        if (effect->animation() && effect->animation()->hasActiveAnimationsOnCompositor(property))
            return true;
    }
    return false;
}

WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > AnimationStack::activeInterpolations(AnimationStack* animationStack, const WillBeHeapVector<RawPtrWillBeMember<InertAnimation> >* newAnimations, const WillBeHeapHashSet<RawPtrWillBeMember<const AnimationPlayer> >* suppressedAnimationPlayers, Animation::Priority priority, double timelineCurrentTime)
{
    // We don't exactly know when new animations will start, but timelineCurrentTime is a good estimate.

    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > result;

    if (animationStack) {
        WillBeHeapVector<OwnPtrWillBeMember<SampledEffect> >& effects = animationStack->m_effects;
        // std::sort doesn't work with OwnPtrs
        nonCopyingSort(effects.begin(), effects.end(), compareEffects);
        for (const auto& effect : effects) {
            if (effect->priority() != priority || (suppressedAnimationPlayers && effect->animation() && suppressedAnimationPlayers->contains(effect->animation()->player())))
                continue;
            copyToActiveInterpolationMap(effect->interpolations(), result);
        }
    }

    if (newAnimations)
        copyNewAnimationsToActiveInterpolationMap(*newAnimations, result);

    return result;
}

DEFINE_TRACE(AnimationStack)
{
    visitor->trace(m_effects);
}

bool AnimationStack::getAnimatedBoundingBox(FloatBox& box, CSSPropertyID property) const
{
    FloatBox originalBox(box);
    for (const auto& effect : m_effects) {
        if (effect->animation() && effect->animation()->affects(property)) {
            Animation* anim = effect->animation();
            if (!anim)
                continue;
            const Timing& timing = anim->specifiedTiming();
            double startRange = 0;
            double endRange = 1;
            timing.timingFunction->range(&startRange, &endRange);
            FloatBox expandingBox(originalBox);
            if (!CompositorAnimations::instance()->getAnimatedBoundingBox(expandingBox, *anim->effect(), startRange, endRange))
                return false;
            box.expandTo(expandingBox);
        }
    }
    return true;
}

} // namespace blink

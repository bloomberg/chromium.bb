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
#include "core/animation/Animation.h"

#include "core/dom/Element.h"

namespace WebCore {

PassRefPtr<Animation> Animation::create(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect)
{
    return adoptRef(new Animation(target, effect));
}

Animation::Animation(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect)
    : m_target(target)
    , m_effect(effect)
    , m_isInTargetActiveAnimationsList(false)
{
}

Animation::~Animation()
{
    if (m_isInTargetActiveAnimationsList)
        m_target->removeActiveAnimation(this);
}

// Generate animation values for a timer tick at the provided time. This time represents the
// parent group's iteration time, or the document time if no parent group exists.
TimedItem::ChildAnimationState Animation::serviceAnimations(double time)
{
    if (!m_isInTargetActiveAnimationsList) {
        m_target->addActiveAnimation(this);
        m_isInTargetActiveAnimationsList = true;
    }

    updateTimeFraction(time);
    m_cachedStyle = m_effect->sample(m_timeFraction, m_currentIteration);
    m_target->setNeedsStyleRecalc(SyntheticStyleChange);

    // For now, all animations last 1 second.
    if (time < 1)
        return AnimationInProgress;

    m_target->removeActiveAnimation(this);
    m_isInTargetActiveAnimationsList = false;
    return AnimationCompleted;
}

} // namespace

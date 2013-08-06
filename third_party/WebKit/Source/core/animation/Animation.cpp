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

#include "core/animation/DocumentTimeline.h"
#include "core/animation/Player.h"
#include "core/dom/Element.h"

namespace WebCore {

PassRefPtr<Animation> Animation::create(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect, const Timing& timing, PassOwnPtr<TimedItemEventDelegate> eventDelegate)
{
    return adoptRef(new Animation(target, effect, timing, eventDelegate));
}

Animation::Animation(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect, const Timing& timing, PassOwnPtr<TimedItemEventDelegate> eventDelegate)
    : TimedItem(timing, eventDelegate)
    , m_target(target)
    , m_effect(effect)
    , m_activeInAnimationStack(false)
{
}

void Animation::willDetach()
{
    if (m_activeInAnimationStack)
        clearEffects();
}

static AnimationStack* ensureAnimationStack(Element* element)
{
    return element->ensureActiveAnimations()->defaultStack();
}

void Animation::applyEffects(bool previouslyInEffect)
{
    ASSERT(player());
    if (!previouslyInEffect) {
        ensureAnimationStack(m_target.get())->add(this);
        m_activeInAnimationStack = true;
    }
    m_compositableValues = m_effect->sample(currentIteration(), timeFraction());
    m_target->setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);
}

void Animation::clearEffects()
{
    ASSERT(player());
    ASSERT(m_activeInAnimationStack);
    ensureAnimationStack(m_target.get())->remove(this);
    m_activeInAnimationStack = false;
    m_compositableValues.clear();
}

void Animation::updateChildrenAndEffects(bool wasInEffect) const
{
    ASSERT(m_activeInAnimationStack == wasInEffect);
    if (isInEffect())
        const_cast<Animation*>(this)->applyEffects(wasInEffect);
    else if (wasInEffect)
        const_cast<Animation*>(this)->clearEffects();
}

} // namespace WebCore

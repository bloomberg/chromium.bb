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

#include "core/animation/css/CSSAnimations.h"

namespace WebCore {

namespace {

void copyToCompositableValueMap(const AnimationEffect::CompositableValueMap* source, AnimationEffect::CompositableValueMap& target)
{
    if (!source)
        return;
    for (AnimationEffect::CompositableValueMap::const_iterator iter = source->begin(); iter != source->end(); ++iter)
        target.set(iter->key, iter->value);
}

} // namespace

AnimationEffect::CompositableValueMap AnimationStack::compositableValues(const AnimationStack* animationStack, const Vector<InertAnimation*>* newAnimations, const HashSet<const Player*>* cancelledPlayers, Animation::Priority priority)
{
    AnimationEffect::CompositableValueMap result;

    if (animationStack) {
        const Vector<Animation*>& animations = animationStack->m_activeAnimations;
        for (size_t i = 0; i < animations.size(); ++i) {
            Animation* animation = animations[i];
            if (animation->priority() != priority)
                continue;
            if (cancelledPlayers && cancelledPlayers->contains(animation->player()))
                continue;
            copyToCompositableValueMap(animation->compositableValues(), result);
        }
    }

    if (newAnimations) {
        for (size_t i = 0; i < newAnimations->size(); ++i)
            copyToCompositableValueMap(newAnimations->at(i)->sample().get(), result);
    }

    return result;
}

} // namespace WebCore

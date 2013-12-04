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
#include "core/animation/ActiveAnimations.h"

#include "core/frame/animation/AnimationController.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

bool shouldCompositeForActiveAnimations(const RenderObject& renderer)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsCSSEnabled());

    if (!renderer.node() || !renderer.node()->isElementNode())
        return false;

    const Element* element = toElement(renderer.node());
    if (const ActiveAnimations* activeAnimations = element->activeAnimations()) {
        if (activeAnimations->hasActiveAnimations(CSSPropertyOpacity)
            || activeAnimations->hasActiveAnimations(CSSPropertyWebkitTransform)
            || activeAnimations->hasActiveAnimations(CSSPropertyWebkitFilter))
            return true;
    }

    return false;
}

bool hasActiveAnimations(const RenderObject& renderer, CSSPropertyID property)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsCSSEnabled());

    if (!renderer.node() || !renderer.node()->isElementNode())
        return false;

    const Element* element = toElement(renderer.node());
    if (const ActiveAnimations* activeAnimations = element->activeAnimations())
        return activeAnimations->hasActiveAnimations(property);

    return false;
}

bool hasActiveAnimationsOnCompositor(const RenderObject& renderer, CSSPropertyID property)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsCSSEnabled());

    if (!renderer.node() || !renderer.node()->isElementNode())
        return false;

    const Element* element = toElement(renderer.node());
    if (const ActiveAnimations* activeAnimations = element->activeAnimations())
        return activeAnimations->hasActiveAnimationsOnCompositor(property);

    return false;
}

bool ActiveAnimations::hasActiveAnimations(CSSPropertyID property) const
{
    return m_defaultStack.affects(property);
}

bool ActiveAnimations::hasActiveAnimationsOnCompositor(CSSPropertyID property) const
{
    return m_defaultStack.hasActiveAnimationsOnCompositor(property);
}

void ActiveAnimations::cancelAnimationOnCompositor()
{
    for (PlayerSet::iterator it = m_players.begin(); it != players().end(); ++it)
        it->key->cancelAnimationOnCompositor();
}

} // namespace WebCore

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

#ifndef AnimationStack_h
#define AnimationStack_h

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffect.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace WebCore {

class InertAnimation;

class AnimationStack {
    WTF_MAKE_NONCOPYABLE(AnimationStack);
public:
    AnimationStack();

    void add(Animation* animation) { m_activeAnimations.append(animation); }
    void remove(Animation* animation)
    {
        size_t position = m_activeAnimations.find(animation);
        ASSERT(position != kNotFound);
        m_activeAnimations.remove(position);
    }
    bool isEmpty() const { return m_activeAnimations.isEmpty(); }
    bool affects(CSSPropertyID) const;
    bool hasActiveAnimationsOnCompositor(CSSPropertyID) const;
    static WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > activeInterpolations(AnimationStack*, const Vector<InertAnimation*>* newAnimations, const HashSet<const AnimationPlayer*>* cancelledAnimationPlayers, Animation::Priority, double timelineCurrentTime);

private:
    Vector<Animation*> m_activeAnimations;
};

} // namespace WebCore

#endif

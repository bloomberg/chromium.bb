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

#ifndef ActiveAnimations_h
#define ActiveAnimations_h

#include "core/animation/AnimationStack.h"
#include "core/animation/css/CSSAnimations.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ActiveAnimations {
public:
    // Animations that are currently active for this element, their effects will be applied
    // during a style recalc.
    AnimationStack* defaultStack() { return &m_defaultStack; }
    // Tracks the state of active CSS Animations. The individual animations will also be
    // part of the default stack, but the mapping betwen animation name and player is kept
    // here.
    CSSAnimations* cssAnimations() { return &m_cssAnimations; }
    // FIXME: Add AnimationStack for CSS Transitions
    // CSS Transitions form a separate animation stack as they apply at a different level of
    // the style cascade. Active transitions will not be present in the default stack.
    bool isEmpty() const { return m_defaultStack.isEmpty() && m_cssAnimations.isEmpty(); }
private:
    AnimationStack m_defaultStack;
    CSSAnimations m_cssAnimations;
};

} // namespace WebCore

#endif

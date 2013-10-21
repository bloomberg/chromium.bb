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

#ifndef ThreadedAnimation_h
#define ThreadedAnimation_h

#include "wtf/Vector.h"

namespace WebCore {

class Element;
class Timing;
class AnimationEffect;

class ThreadedAnimation FINAL {

public:
    // Note that the implementation of these functions is indirected for mocking during unit testing.
    static void isCandidateForThreadedAnimation(const Timing* timing, const AnimationEffect* effect) { (*s_isCandidateForThreadedAnimationFunction)(timing, effect); }
    static void canStartThreadedAnimation(const Element* element) { (*s_canStartThreadedAnimationFunction)(element); }
    static void startThreadedAnimation(const Element* element, const Timing* timing, const AnimationEffect* effect, Vector<int>& threadedAnimationIds) { (*s_startThreadedAnimationFunction)(element, timing, effect, threadedAnimationIds); }
    static void cancelThreadedAnimation(const Element* element, int id) { (*s_cancelThreadedAnimationFunction)(element, id); }

private:
    ThreadedAnimation() { }
    static void isCandidateForThreadedAnimationImpl(const Timing*, const AnimationEffect*);
    static void canStartThreadedAnimationImpl(const Element*);
    static void startThreadedAnimationImpl(const Element*, const Timing*, const AnimationEffect*, Vector<int>& threadedAnimationIds);
    static void cancelThreadedAnimationImpl(const Element*, int threadedAnimationId);

    typedef void (*IsCandidateForThreadedAnimationFunction)(const Timing*, const AnimationEffect*);
    typedef void (*CanStartThreadedAnimationFunction)(const Element*);
    typedef void (*StartThreadedAnimationFunction)(const Element*, const Timing*, const AnimationEffect*, Vector<int>&);
    typedef void (*CancelThreadedAnimationFunction)(const Element*, int);

    static IsCandidateForThreadedAnimationFunction s_isCandidateForThreadedAnimationFunction;
    static CanStartThreadedAnimationFunction s_canStartThreadedAnimationFunction;
    static StartThreadedAnimationFunction s_startThreadedAnimationFunction;
    static CancelThreadedAnimationFunction s_cancelThreadedAnimationFunction;
};

} // namespace WebCore

#endif

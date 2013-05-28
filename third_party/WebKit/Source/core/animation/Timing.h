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

#ifndef Timing_h
#define Timing_h

#include "core/platform/animation/TimingFunction.h"
#include "wtf/MathExtras.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct Timing {
    enum FillMode {
        FillModeNone,
        FillModeForwards,
        FillModeBackwards,
        FillModeBoth
    };

    enum PlaybackDirection {
        PlaybackDirectionNormal,
        PlaybackDirectionReverse,
        PlaybackDirectionAlternate,
        PlaybackDirectionAlternateReverse
    };

    Timing()
        : startDelay(0)
        , fillMode(FillModeForwards)
        , iterationStart(0)
        , iterationCount(1)
        , hasIterationDuration(false)
        , iterationDuration(0)
        , playbackRate(1)
        , direction(PlaybackDirectionNormal)
        , timingFunction(0)
    {
    }

    void assertValid() const
    {
        ASSERT(std::isfinite(startDelay));
        ASSERT(std::isfinite(iterationStart));
        ASSERT(iterationStart >= 0);
        ASSERT(iterationCount >= 0);
        ASSERT(iterationDuration >= 0);
        ASSERT(std::isfinite(playbackRate));
    }

    double startDelay;
    FillMode fillMode;
    double iterationStart;
    double iterationCount;
    bool hasIterationDuration;
    double iterationDuration;
    // FIXME: Add activeDuration.
    double playbackRate;
    PlaybackDirection direction;
    RefPtr<TimingFunction> timingFunction;

};

} // namespace WebCore

#endif

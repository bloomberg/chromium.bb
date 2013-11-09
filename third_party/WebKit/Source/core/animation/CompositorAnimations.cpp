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
#include "core/animation/CompositorAnimations.h"

#include "core/rendering/CompositedLayerMapping.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAnimation.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatKeyframe.h"

namespace WebCore {

// -----------------------------------------------------------------------

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const LinearTimingFunction* timefunc)
{
    return const_cast<LinearTimingFunction*>(timefunc);
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const CubicBezierTimingFunction* timefunc)
{
    switch (timefunc->subType()) {
    case CubicBezierTimingFunction::EaseIn:
        return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
    case CubicBezierTimingFunction::EaseOut:
        return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    case CubicBezierTimingFunction::EaseInOut:
        return const_cast<CubicBezierTimingFunction*>(timefunc);
    case CubicBezierTimingFunction::Ease: // Ease is not symmetrical
    case CubicBezierTimingFunction::Custom:
        return CubicBezierTimingFunction::create(1 - timefunc->x2(), 1 - timefunc->y2(), 1 - timefunc->x1(), 1 - timefunc->y1());
    default:
        ASSERT_NOT_REACHED();
        return PassRefPtr<TimingFunction>();
    }
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const ChainedTimingFunction* timefunc)
{
    RefPtr<ChainedTimingFunction> reversed = ChainedTimingFunction::create();
    for (size_t i = 0; i < timefunc->m_segments.size(); i++) {
        size_t index = timefunc->m_segments.size() - i - 1;

        RefPtr<TimingFunction> rtf = reverse(timefunc->m_segments[index].m_timingFunction.get());
        reversed->appendSegment(1 - timefunc->m_segments[index].m_min, rtf.get());
    }
    return reversed;
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const TimingFunction* timefunc)
{
    switch (timefunc->type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction* linear = static_cast<const LinearTimingFunction*>(timefunc);
        return reverse(linear);
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = static_cast<const CubicBezierTimingFunction*>(timefunc);
        return reverse(cubic);
    }
    case TimingFunction::ChainedFunction: {
        const ChainedTimingFunction* chained = static_cast<const ChainedTimingFunction*>(timefunc);
        return reverse(chained);
    }

    // Steps function can not be reversed.
    case TimingFunction::StepsFunction:
    default:
        ASSERT_NOT_REACHED();
        return PassRefPtr<TimingFunction>();
    }
}

// -----------------------------------------------------------------------


bool CompositorAnimations::isCandidateForCompositorAnimation(const Timing& timing, const AnimationEffect* effect)
{
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
    return false;
}

bool CompositorAnimations::canStartCompositorAnimation(const Element* element)
{
    return element->renderer() && element->renderer()->compositingState() == PaintsIntoOwnBacking;
}

void CompositorAnimations::startCompositorAnimation(const Element* element, const Timing&, const AnimationEffect*, Vector<int>& startedAnimationIds)
{
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

void CompositorAnimations::cancelCompositorAnimation(const Element* element, int id)
{
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

}

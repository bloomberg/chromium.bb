/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/platform/animation/TimingFunctionTestHelper.h"

#include <ostream> // NOLINT

namespace WebCore {

void PrintTo(const LinearTimingFunction& timingFunction, ::std::ostream* os)
{
    *os << "LinearTimingFunction@" << &timingFunction;
}

void PrintTo(const CubicBezierTimingFunction& timingFunction, ::std::ostream* os)
{
    *os << "CubicBezierTimingFunction@" << &timingFunction << "(";
    switch (timingFunction.subType()) {
    case CubicBezierTimingFunction::Ease:
        *os << "Ease";
        break;
    case CubicBezierTimingFunction::EaseIn:
        *os << "EaseIn";
        break;
    case CubicBezierTimingFunction::EaseOut:
        *os << "EaseOut";
        break;
    case CubicBezierTimingFunction::EaseInOut:
        *os << "EaseInOut";
        break;
    case CubicBezierTimingFunction::Custom:
        *os << "Custom";
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    *os << ", " << timingFunction.x1();
    *os << ", " << timingFunction.y1();
    *os << ", " << timingFunction.x2();
    *os << ", " << timingFunction.y2();
    *os << ")";
}

void PrintTo(const StepsTimingFunction& timingFunction, ::std::ostream* os)
{
    *os << "StepsTimingFunction@" << &timingFunction << "(";
    switch (timingFunction.subType()) {
    case StepsTimingFunction::Start:
        *os << "Start";
        break;
    case StepsTimingFunction::End:
        *os << "End";
        break;
    case StepsTimingFunction::Custom:
        *os << "Custom";
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    *os << ", " << timingFunction.numberOfSteps();
    *os << ", " << (timingFunction.stepAtStart() ? "true" : "false");
    *os << ")";
}

class ChainedTimingFunctionPrinter {
private:
    static void PrintTo(const ChainedTimingFunction& timingFunction, ::std::ostream* os)
    {
        // Forward declare the generic PrintTo function as ChainedTimingFunction needs to call it.
        void PrintTo(const TimingFunction&, ::std::ostream*);

        *os << "ChainedTimingFunction@" << &timingFunction << "(";
        for (size_t i = 0; i < timingFunction.m_segments.size(); i++) {
            ChainedTimingFunction::Segment segment = timingFunction.m_segments[i];
            PrintTo(*(segment.m_timingFunction.get()), os);
            *os << "[" << segment.m_min << " -> " << segment.m_max << "]";
            if (i+1 != timingFunction.m_segments.size()) {
                *os << ", ";
            }
        }
        *os << ")";
    }

    friend void PrintTo(const ChainedTimingFunction&, ::std::ostream*);
};

void PrintTo(const ChainedTimingFunction& timingFunction, ::std::ostream* os)
{
    ChainedTimingFunctionPrinter::PrintTo(timingFunction, os);
}

// The generic PrintTo *must* come after the non-generic PrintTo otherwise it
// will end up calling itself.
void PrintTo(const TimingFunction& timingFunction, ::std::ostream* os)
{
    switch (timingFunction.type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction* linear = static_cast<const LinearTimingFunction*>(&timingFunction);
        PrintTo(*linear, os);
        return;
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = static_cast<const CubicBezierTimingFunction*>(&timingFunction);
        PrintTo(*cubic, os);
        return;
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction* step = static_cast<const StepsTimingFunction*>(&timingFunction);
        PrintTo(*step, os);
        return;
    }
    case TimingFunction::ChainedFunction: {
        const ChainedTimingFunction* chained = static_cast<const ChainedTimingFunction*>(&timingFunction);
        PrintTo(*chained, os);
        return;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore

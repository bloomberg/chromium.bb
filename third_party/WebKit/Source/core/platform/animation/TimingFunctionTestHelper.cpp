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


namespace WebCore {

// This class exists so that ChainedTimingFunction only needs to friend one thing.
class ChainedTimingFunctionTestHelper {
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

    static bool equals(const ChainedTimingFunction& lhs, const TimingFunction& rhs)
    {
        if (rhs.type() != TimingFunction::ChainedFunction)
            return false;

        if (&lhs == &rhs)
            return true;

        const ChainedTimingFunction& ctf = toChainedTimingFunction(rhs);
        if (lhs.m_segments.size() != ctf.m_segments.size())
            return false;

        for (size_t i = 0; i < lhs.m_segments.size(); i++) {
            if (!equals(lhs.m_segments[i], ctf.m_segments[i]))
                return false;
        }
        return true;
    }

    static bool equals(const ChainedTimingFunction::Segment& lhs, const ChainedTimingFunction::Segment& rhs)
    {
        if (&lhs == &rhs)
            return true;

        if ((lhs.m_min != rhs.m_min) || (lhs.m_max != rhs.m_max))
            return false;

        if (lhs.m_timingFunction == rhs.m_timingFunction)
            return true;

        ASSERT(lhs.m_timingFunction);
        ASSERT(rhs.m_timingFunction);

        return (*(lhs.m_timingFunction.get())) == (*(rhs.m_timingFunction.get()));
    }

    friend void PrintTo(const ChainedTimingFunction&, ::std::ostream*);
    friend bool operator==(const ChainedTimingFunction& lhs, const TimingFunction& rhs);
};

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

void PrintTo(const ChainedTimingFunction& timingFunction, ::std::ostream* os)
{
    ChainedTimingFunctionTestHelper::PrintTo(timingFunction, os);
}

// The generic PrintTo *must* come after the non-generic PrintTo otherwise it
// will end up calling itself.
void PrintTo(const TimingFunction& timingFunction, ::std::ostream* os)
{
    switch (timingFunction.type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction& linear = toLinearTimingFunction(timingFunction);
        PrintTo(linear, os);
        return;
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(timingFunction);
        PrintTo(cubic, os);
        return;
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction& step = toStepsTimingFunction(timingFunction);
        PrintTo(step, os);
        return;
    }
    case TimingFunction::ChainedFunction: {
        const ChainedTimingFunction& chained = toChainedTimingFunction(timingFunction);
        PrintTo(chained, os);
        return;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

bool operator==(const LinearTimingFunction& lhs, const TimingFunction& rhs)
{
    return rhs.type() == TimingFunction::LinearFunction;
}

bool operator==(const CubicBezierTimingFunction& lhs, const TimingFunction& rhs)
{
    if (rhs.type() != TimingFunction::CubicBezierFunction)
        return false;

    const CubicBezierTimingFunction& ctf = toCubicBezierTimingFunction(rhs);
    if ((lhs.subType() == CubicBezierTimingFunction::Custom) && (ctf.subType() == CubicBezierTimingFunction::Custom))
        return (lhs.x1() == ctf.x1()) && (lhs.y1() == ctf.y1()) && (lhs.x2() == ctf.x2()) && (lhs.y2() == ctf.y2());

    return lhs.subType() == ctf.subType();
}

bool operator==(const StepsTimingFunction& lhs, const TimingFunction& rhs)
{
    if (rhs.type() != TimingFunction::StepsFunction)
        return false;

    const StepsTimingFunction& stf = toStepsTimingFunction(rhs);
    if ((lhs.subType() == StepsTimingFunction::Custom) && (stf.subType() == StepsTimingFunction::Custom))
        return (lhs.numberOfSteps() == stf.numberOfSteps()) && (lhs.stepAtStart() == stf.stepAtStart());

    return lhs.subType() == stf.subType();
}

bool operator==(const ChainedTimingFunction& lhs, const TimingFunction& rhs)
{
    return ChainedTimingFunctionTestHelper::equals(lhs, rhs);
}

// Like in the PrintTo case, the generic operator== *must* come after the
// non-generic operator== otherwise it will end up calling itself.
bool operator==(const TimingFunction& lhs, const TimingFunction& rhs)
{
    switch (lhs.type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction& linear = toLinearTimingFunction(lhs);
        return (linear == rhs);
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(lhs);
        return (cubic == rhs);
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction& step = toStepsTimingFunction(lhs);
        return (step == rhs);
    }
    case TimingFunction::ChainedFunction: {
        const ChainedTimingFunction& chained = toChainedTimingFunction(lhs);
        return (chained == rhs);
    }
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

// No need to define specific operator!= as they can all come via this function.
bool operator!=(const TimingFunction& lhs, const TimingFunction& rhs)
{
    return !(lhs == rhs);
}

} // namespace WebCore

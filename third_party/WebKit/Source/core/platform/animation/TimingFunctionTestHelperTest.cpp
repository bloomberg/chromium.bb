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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <string>


using namespace WebCore;

namespace {

class TimingFunctionTestHelperTest : public ::testing::Test {

protected:
    bool m_enabled;

    virtual void SetUp()
    {
        m_enabled = RuntimeEnabledFeatures::webAnimationsEnabled();
        // Needed for ChainedTimingFunction support
        RuntimeEnabledFeatures::setWebAnimationsEnabled(true);
    }

    virtual void TearDown()
    {
        RuntimeEnabledFeatures::setWebAnimationsEnabled(m_enabled);
    }

public:
    // Make sure that the CubicBezierTimingFunction call goes via the generic
    // TimingFunction PrintTo.
    ::std::string PrintToString(RefPtr<CubicBezierTimingFunction> timing)
    {
        RefPtr<TimingFunction> generic = timing;
        return PrintToString(generic.get());
    }

    ::std::string PrintToString(RefPtr<TimingFunction> timing)
    {
        return PrintToString(timing.get());
    }

    ::std::string PrintToString(const TimingFunction* timing)
    {
        return ::testing::PrintToString(*timing);
    }
};

TEST_F(TimingFunctionTestHelperTest, LinearPrintTo)
{
    RefPtr<TimingFunction> linearTiming = LinearTimingFunction::create();
    EXPECT_THAT(
        PrintToString(linearTiming),
        ::testing::MatchesRegex("LinearTimingFunction@.*"));
}

TEST_F(TimingFunctionTestHelperTest, CubicPrintTo)
{
    RefPtr<TimingFunction> cubicEaseTiming = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    EXPECT_THAT(
        PrintToString(cubicEaseTiming),
        ::testing::MatchesRegex("CubicBezierTimingFunction@.*\\(EaseIn, 0.42, 0, 1, 1\\)"));

    RefPtr<TimingFunction> cubicCustomTiming = CubicBezierTimingFunction::create(0.17, 0.67, 1, -1.73);
    EXPECT_THAT(
        PrintToString(cubicCustomTiming),
        ::testing::MatchesRegex("CubicBezierTimingFunction@.*\\(Custom, 0.17, 0.67, 1, -1.73\\)"));
}

TEST_F(TimingFunctionTestHelperTest, StepPrintTo)
{
    RefPtr<TimingFunction> stepTimingStart = StepsTimingFunction::preset(StepsTimingFunction::Start);
    EXPECT_THAT(
        PrintToString(stepTimingStart),
        ::testing::MatchesRegex("StepsTimingFunction@.*\\(Start, 1, true\\)"));

    RefPtr<TimingFunction> stepTimingCustom = StepsTimingFunction::create(5, false);
    EXPECT_THAT(
        PrintToString(stepTimingCustom),
        ::testing::MatchesRegex("StepsTimingFunction@.*\\(Custom, 5, false\\)"));
}

TEST_F(TimingFunctionTestHelperTest, ChainedPrintTo)
{
    RefPtr<TimingFunction> linearTiming = LinearTimingFunction::create();
    RefPtr<ChainedTimingFunction> chainedLinearSingle = ChainedTimingFunction::create();
    chainedLinearSingle->appendSegment(1.0, linearTiming.get());
    EXPECT_THAT(
        PrintToString(chainedLinearSingle),
        ::testing::MatchesRegex(
            "ChainedTimingFunction@.*\\("
                "LinearTimingFunction@.*\\[0 -> 1\\]"
            "\\)"));

    RefPtr<TimingFunction> cubicCustomTiming = CubicBezierTimingFunction::create(1.0, 0.0, 1, -1);

    RefPtr<ChainedTimingFunction> chainedMixed = ChainedTimingFunction::create();
    chainedMixed->appendSegment(0.75, chainedLinearSingle.get());
    chainedMixed->appendSegment(1.0, cubicCustomTiming.get());
    EXPECT_THAT(
        PrintToString(chainedMixed),
        ::testing::MatchesRegex(
            "ChainedTimingFunction@.*\\("
                "ChainedTimingFunction@.*\\("
                    "LinearTimingFunction@.*\\[0 -> 1\\]"
                "\\)\\[0 -> 0.75\\], "
                "CubicBezierTimingFunction@.*\\(Custom, 1, 0, 1, -1\\)\\[0.75 -> 1\\]"
            "\\)"));
}

} // namespace

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

// FIXME: Remove once https://codereview.chromium.org/50603011/ lands.
#define EXPECT_REFV_EQ(a, b) EXPECT_EQ(*(a.get()), *(b.get()))
#define EXPECT_REFV_NE(a, b) EXPECT_NE(*(a.get()), *(b.get()))

// Couple of macros to quickly assert a bunch of timing functions are not
// equal.
#define NE_STRINGIZE(x) NE_STRINGIZE2(x)
#define NE_STRINGIZE2(x) #x
#define NE_HELPER(v) \
    Vector<std::pair<std::string, RefPtr<TimingFunction> > > v;
#define NE_HELPER_APPEND(v, x) \
    v.append(std::make_pair(std::string("Line " NE_STRINGIZE(__LINE__) ":" # x), x))
#define NE_HELPER_LOOP(v) \
    for (size_t i = 0; i != v.size(); ++i) { \
        for (size_t j = 0; j != v.size(); ++j) { \
            if (i == j) \
                continue; \
            EXPECT_REFV_NE(v[i].second, v[j].second) \
                << v[i].first \
                << " (" << ::testing::PrintToString(*v[i].second.get()) << ")" \
                << "  ==  " \
                << v[j].first \
                << " (" << ::testing::PrintToString(*v[j].second.get()) << ")" \
                << "\n"; \
        } \
    }

namespace {

using namespace WebCore;

class TimingFunctionTestHelperTest : public ::testing::Test {

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

TEST_F(TimingFunctionTestHelperTest, BaseOperatorEq)
{
    RefPtr<TimingFunction> linearTiming = LinearTimingFunction::create();
    RefPtr<TimingFunction> cubicTiming1 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    RefPtr<TimingFunction> cubicTiming2 = CubicBezierTimingFunction::create(0.17, 0.67, 1, -1.73);
    RefPtr<TimingFunction> stepsTiming1 = StepsTimingFunction::preset(StepsTimingFunction::End);
    RefPtr<TimingFunction> stepsTiming2 = StepsTimingFunction::create(5, true);

    RefPtr<ChainedTimingFunction> chainedTiming1 = ChainedTimingFunction::create();
    chainedTiming1->appendSegment(1.0, linearTiming.get());

    RefPtr<ChainedTimingFunction> chainedTiming2 = ChainedTimingFunction::create();
    chainedTiming2->appendSegment(0.5, cubicTiming1.get());
    chainedTiming2->appendSegment(1.0, cubicTiming2.get());

    NE_HELPER(v);
    NE_HELPER_APPEND(v, linearTiming);
    NE_HELPER_APPEND(v, cubicTiming1);
    NE_HELPER_APPEND(v, cubicTiming2);
    NE_HELPER_APPEND(v, stepsTiming1);
    NE_HELPER_APPEND(v, stepsTiming2);
    NE_HELPER_APPEND(v, chainedTiming1);
    NE_HELPER_APPEND(v, chainedTiming2);
    NE_HELPER_LOOP(v);
}

TEST_F(TimingFunctionTestHelperTest, LinearOperatorEq)
{
    RefPtr<TimingFunction> linearTiming1 = LinearTimingFunction::create();
    RefPtr<TimingFunction> linearTiming2 = LinearTimingFunction::create();
    EXPECT_REFV_EQ(linearTiming1, linearTiming1);
    EXPECT_REFV_EQ(linearTiming1, linearTiming2);
}

TEST_F(TimingFunctionTestHelperTest, CubicOperatorEq)
{
    RefPtr<TimingFunction> cubicEaseInTiming1 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    RefPtr<TimingFunction> cubicEaseInTiming2 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    EXPECT_REFV_EQ(cubicEaseInTiming1, cubicEaseInTiming1);
    EXPECT_REFV_EQ(cubicEaseInTiming1, cubicEaseInTiming2);

    RefPtr<TimingFunction> cubicEaseOutTiming1 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
    RefPtr<TimingFunction> cubicEaseOutTiming2 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
    EXPECT_REFV_EQ(cubicEaseOutTiming1, cubicEaseOutTiming2);

    RefPtr<TimingFunction> cubicEaseInOutTiming1 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut);
    RefPtr<TimingFunction> cubicEaseInOutTiming2 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut);
    EXPECT_REFV_EQ(cubicEaseInOutTiming1, cubicEaseInOutTiming2);

    RefPtr<TimingFunction> cubicCustomTiming1 = CubicBezierTimingFunction::create(0.17, 0.67, 1, -1.73);
    RefPtr<TimingFunction> cubicCustomTiming2 = CubicBezierTimingFunction::create(0.17, 0.67, 1, -1.73);
    EXPECT_REFV_EQ(cubicCustomTiming1, cubicCustomTiming2);

    NE_HELPER(v);
    NE_HELPER_APPEND(v, cubicEaseInTiming1);
    NE_HELPER_APPEND(v, cubicEaseOutTiming1);
    NE_HELPER_APPEND(v, cubicEaseInOutTiming1);
    NE_HELPER_APPEND(v, cubicCustomTiming1);
    NE_HELPER_LOOP(v);
}

TEST_F(TimingFunctionTestHelperTest, CubicOperatorEqReflectivity)
{
    RefPtr<TimingFunction> cubicA = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    RefPtr<TimingFunction> cubicB = CubicBezierTimingFunction::create(0.42, 0.0, 1.0, 1.0);
    EXPECT_REFV_NE(cubicA, cubicB);
    EXPECT_REFV_NE(cubicB, cubicA);
}

TEST_F(TimingFunctionTestHelperTest, StepsOperatorEq)
{
    RefPtr<TimingFunction> stepsTimingStart1 = StepsTimingFunction::preset(StepsTimingFunction::Start);
    RefPtr<TimingFunction> stepsTimingStart2 = StepsTimingFunction::preset(StepsTimingFunction::Start);
    EXPECT_REFV_EQ(stepsTimingStart1, stepsTimingStart1);
    EXPECT_REFV_EQ(stepsTimingStart1, stepsTimingStart2);

    RefPtr<TimingFunction> stepsTimingEnd1 = StepsTimingFunction::preset(StepsTimingFunction::End);
    RefPtr<TimingFunction> stepsTimingEnd2 = StepsTimingFunction::preset(StepsTimingFunction::End);
    EXPECT_REFV_EQ(stepsTimingEnd1, stepsTimingEnd2);

    RefPtr<TimingFunction> stepsTimingCustom1 = StepsTimingFunction::create(5, true);
    RefPtr<TimingFunction> stepsTimingCustom2 = StepsTimingFunction::create(5, false);
    RefPtr<TimingFunction> stepsTimingCustom3 = StepsTimingFunction::create(7, true);
    RefPtr<TimingFunction> stepsTimingCustom4 = StepsTimingFunction::create(7, false);

    EXPECT_REFV_EQ(stepsTimingCustom1, StepsTimingFunction::create(5, true));
    EXPECT_REFV_EQ(stepsTimingCustom2, StepsTimingFunction::create(5, false));
    EXPECT_REFV_EQ(stepsTimingCustom3, StepsTimingFunction::create(7, true));
    EXPECT_REFV_EQ(stepsTimingCustom4, StepsTimingFunction::create(7, false));

    NE_HELPER(v);
    NE_HELPER_APPEND(v, stepsTimingStart1);
    NE_HELPER_APPEND(v, stepsTimingEnd1);
    NE_HELPER_APPEND(v, stepsTimingCustom1);
    NE_HELPER_APPEND(v, stepsTimingCustom2);
    NE_HELPER_APPEND(v, stepsTimingCustom3);
    NE_HELPER_APPEND(v, stepsTimingCustom4);
    NE_HELPER_LOOP(v);
}

TEST_F(TimingFunctionTestHelperTest, StepsOperatorEqReflectivity)
{
    RefPtr<TimingFunction> stepsA = StepsTimingFunction::preset(StepsTimingFunction::Start);
    RefPtr<TimingFunction> stepsB = StepsTimingFunction::create(1, true);
    EXPECT_REFV_NE(stepsA, stepsB);
    EXPECT_REFV_NE(stepsB, stepsA);
}

TEST_F(TimingFunctionTestHelperTest, ChainedEq)
{
    // Single item in chain
    RefPtr<TimingFunction> cubicTiming1 = CubicBezierTimingFunction::create(0.25, 0.1, 0.25, 1.0);
    RefPtr<TimingFunction> cubicTiming2 = CubicBezierTimingFunction::create(0.25, 0.1, 0.25, 1.0);
    RefPtr<TimingFunction> cubicTiming3 = CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);

    RefPtr<ChainedTimingFunction> chainedSingleCubic1 = ChainedTimingFunction::create();
    chainedSingleCubic1->appendSegment(1.0, cubicTiming1.get());
    EXPECT_REFV_EQ(chainedSingleCubic1, chainedSingleCubic1);

    RefPtr<ChainedTimingFunction> chainedSingleCubic2 = ChainedTimingFunction::create();
    chainedSingleCubic2->appendSegment(1.0, cubicTiming1.get()); // Same inner timing function
    EXPECT_REFV_EQ(chainedSingleCubic1, chainedSingleCubic2);

    RefPtr<ChainedTimingFunction> chainedSingleCubic3 = ChainedTimingFunction::create();
    chainedSingleCubic3->appendSegment(1.0, cubicTiming2.get()); // == inner timing function
    EXPECT_REFV_EQ(chainedSingleCubic1, chainedSingleCubic3);

    RefPtr<ChainedTimingFunction> chainedSingleCubic4 = ChainedTimingFunction::create();
    chainedSingleCubic4->appendSegment(0.5, cubicTiming1.get()); // Different offset
    EXPECT_REFV_NE(chainedSingleCubic1, chainedSingleCubic4);
    EXPECT_REFV_NE(chainedSingleCubic3, chainedSingleCubic4);

    RefPtr<ChainedTimingFunction> chainedSingleCubic5 = ChainedTimingFunction::create();
    chainedSingleCubic5->appendSegment(1.0, cubicTiming3.get()); // != inner timing function (same type)
    EXPECT_REFV_NE(chainedSingleCubic1, chainedSingleCubic5);
    EXPECT_REFV_NE(chainedSingleCubic2, chainedSingleCubic5);
    EXPECT_REFV_NE(chainedSingleCubic3, chainedSingleCubic5);
    EXPECT_REFV_NE(chainedSingleCubic4, chainedSingleCubic5);

    RefPtr<TimingFunction> linearTiming1 = LinearTimingFunction::create();
    RefPtr<ChainedTimingFunction> chainedSingleLinear1 = ChainedTimingFunction::create();
    chainedSingleLinear1->appendSegment(1.0, linearTiming1.get()); // != inner timing function (different type)
    EXPECT_REFV_NE(chainedSingleLinear1, chainedSingleCubic1);
    EXPECT_REFV_NE(chainedSingleLinear1, chainedSingleCubic2);
    EXPECT_REFV_NE(chainedSingleLinear1, chainedSingleCubic3);
    EXPECT_REFV_NE(chainedSingleLinear1, chainedSingleCubic4);

    // Multiple items in chain
    RefPtr<ChainedTimingFunction> chainedMixed1 = ChainedTimingFunction::create();
    chainedMixed1->appendSegment(0.25, chainedSingleLinear1.get());
    chainedMixed1->appendSegment(1.0, cubicTiming1.get());

    RefPtr<ChainedTimingFunction> chainedMixed2 = ChainedTimingFunction::create();
    chainedMixed2->appendSegment(0.25, chainedSingleLinear1.get());
    chainedMixed2->appendSegment(1.0, cubicTiming1.get());

    RefPtr<ChainedTimingFunction> chainedMixed3 = ChainedTimingFunction::create();
    chainedMixed3->appendSegment(0.25, chainedSingleLinear1.get());
    chainedMixed3->appendSegment(1.0, cubicTiming2.get());

    EXPECT_REFV_EQ(chainedMixed1, chainedMixed2);
    EXPECT_REFV_EQ(chainedMixed1, chainedMixed3);
    EXPECT_REFV_NE(chainedMixed1, chainedSingleCubic1);
    EXPECT_REFV_NE(chainedMixed1, chainedSingleLinear1);

    RefPtr<ChainedTimingFunction> chainedMixed4 = ChainedTimingFunction::create();
    chainedMixed4->appendSegment(0.20, chainedSingleLinear1.get()); // Different offset
    chainedMixed4->appendSegment(1.0, cubicTiming1.get());
    EXPECT_REFV_NE(chainedMixed1, chainedMixed4);
}

} // namespace

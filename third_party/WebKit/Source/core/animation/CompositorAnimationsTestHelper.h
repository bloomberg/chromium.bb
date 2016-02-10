/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositorAnimationsTestHelper_h
#define CompositorAnimationsTestHelper_h

#include "core/animation/CompositorAnimations.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/graphics/CompositorFactory.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/PassOwnPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Test helpers and mocks for Web* types
// -----------------------------------------------------------------------
namespace blink {

// CompositorFloatKeyframe is a plain struct, so we just create an == operator
// for it.
inline bool operator==(const CompositorFloatKeyframe& a, const CompositorFloatKeyframe& b)
{
    return a.time == b.time && a.value == b.value;
}

inline void PrintTo(const CompositorFloatKeyframe& frame, ::std::ostream* os)
{
    *os << "CompositorFloatKeyframe@" << &frame << "(" << frame.time << ", " << frame.value << ")";
}

// -----------------------------------------------------------------------

class WebCompositorAnimationMock : public CompositorAnimation {
private:
    CompositorAnimation::TargetProperty m_property;

public:
    // Target Property is set through the constructor.
    WebCompositorAnimationMock(CompositorAnimation::TargetProperty p) : m_property(p) { }
    virtual CompositorAnimation::TargetProperty targetProperty() const { return m_property; }

    MOCK_METHOD0(id, int());
    MOCK_METHOD0(group, int());

    MOCK_CONST_METHOD0(iterations, double());
    MOCK_METHOD1(setIterations, void(double));

    MOCK_CONST_METHOD0(iterationStart, double());
    MOCK_METHOD1(setIterationStart, void(double));

    MOCK_CONST_METHOD0(startTime, double());
    MOCK_METHOD1(setStartTime, void(double));

    MOCK_CONST_METHOD0(timeOffset, double());
    MOCK_METHOD1(setTimeOffset, void(double));

    MOCK_CONST_METHOD0(direction, Direction());
    MOCK_METHOD1(setDirection, void(Direction));

    MOCK_CONST_METHOD0(playbackRate, double());
    MOCK_METHOD1(setPlaybackRate, void(double));

    MOCK_CONST_METHOD0(fillMode, FillMode());
    MOCK_METHOD1(setFillMode, void(FillMode));

    MOCK_METHOD0(delete_, void());
    ~WebCompositorAnimationMock() { delete_(); }
};

template<typename CurveType, CompositorAnimationCurve::AnimationCurveType CurveId, typename KeyframeType>
class WebCompositorAnimationCurveMock : public CurveType {
public:
    MOCK_METHOD1_T(add, void(const KeyframeType&));
    MOCK_METHOD2_T(add, void(const KeyframeType&, CompositorAnimationCurve::TimingFunctionType));
    MOCK_METHOD5_T(add, void(const KeyframeType&, double, double, double, double));
    MOCK_METHOD3_T(add, void(const KeyframeType&, int steps, float stepsStartOffset));

    MOCK_METHOD0(setLinearTimingFunction, void());
    MOCK_METHOD4(setCubicBezierTimingFunction, void(double, double, double, double));
    MOCK_METHOD1(setCubicBezierTimingFunction, void(CompositorAnimationCurve::TimingFunctionType));
    MOCK_METHOD2(setStepsTimingFunction, void(int, float));

    MOCK_CONST_METHOD1_T(getValue, float(double)); // Only on CompositorFloatAnimationCurve, but can't hurt to have here.

    virtual CompositorAnimationCurve::AnimationCurveType type() const { return CurveId; }

    MOCK_METHOD0(delete_, void());
    ~WebCompositorAnimationCurveMock() override { delete_(); }
};

using WebFloatAnimationCurveMock = WebCompositorAnimationCurveMock<CompositorFloatAnimationCurve, CompositorAnimationCurve::AnimationCurveTypeFloat, CompositorFloatKeyframe>;

class WebCompositorAnimationTimelineMock : public CompositorAnimationTimeline {
public:
    MOCK_METHOD1(playerAttached, void(const CompositorAnimationPlayerClient&));
    MOCK_METHOD1(playerDestroyed, void(const CompositorAnimationPlayerClient&));
};

} // namespace blink

namespace blink {

class AnimationCompositorAnimationsTestBase : public ::testing::Test {
public:
    class CompositorFactoryMock : public CompositorFactory {
    public:
        MOCK_METHOD4(createAnimation, CompositorAnimation*(const CompositorAnimationCurve& curve, CompositorAnimation::TargetProperty target, int groupId, int animationId));
        MOCK_METHOD0(createFloatAnimationCurve, CompositorFloatAnimationCurve*());
        MOCK_METHOD0(createAnimationPlayer, CompositorAnimationPlayer*());
        MOCK_METHOD0(createAnimationTimeline, CompositorAnimationTimeline*());
    };

private:
    TestingPlatformSupport m_proxyPlatform;

protected:
    void SetUp() override
    {
        Platform::initialize(&m_proxyPlatform);
    }
};

} // namespace blink

#endif

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
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/WebCompositorAnimationPlayer.h"
#include "public/platform/WebCompositorAnimationTimeline.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatKeyframe.h"
#include "wtf/PassOwnPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Test helpers and mocks for Web* types
// -----------------------------------------------------------------------
namespace blink {

// WebFloatKeyframe is a plain struct, so we just create an == operator
// for it.
inline bool operator==(const WebFloatKeyframe& a, const WebFloatKeyframe& b)
{
    return a.time == b.time && a.value == b.value;
}

inline void PrintTo(const WebFloatKeyframe& frame, ::std::ostream* os)
{
    *os << "WebFloatKeyframe@" << &frame << "(" << frame.time << ", " << frame.value << ")";
}

// -----------------------------------------------------------------------

class WebCompositorAnimationMock : public WebCompositorAnimation {
private:
    WebCompositorAnimation::TargetProperty m_property;

public:
    // Target Property is set through the constructor.
    WebCompositorAnimationMock(WebCompositorAnimation::TargetProperty p) : m_property(p) { }
    virtual WebCompositorAnimation::TargetProperty targetProperty() const { return m_property; }

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

template<typename CurveType, WebCompositorAnimationCurve::AnimationCurveType CurveId, typename KeyframeType>
class WebCompositorAnimationCurveMock : public CurveType {
public:
    MOCK_METHOD1_T(add, void(const KeyframeType&));
    MOCK_METHOD2_T(add, void(const KeyframeType&, WebCompositorAnimationCurve::TimingFunctionType));
    MOCK_METHOD5_T(add, void(const KeyframeType&, double, double, double, double));
    MOCK_METHOD3_T(add, void(const KeyframeType&, int steps, float stepsStartOffset));

    MOCK_METHOD0(setLinearTimingFunction, void());
    MOCK_METHOD4(setCubicBezierTimingFunction, void(double, double, double, double));
    MOCK_METHOD1(setCubicBezierTimingFunction, void(WebCompositorAnimationCurve::TimingFunctionType));
    MOCK_METHOD2(setStepsTimingFunction, void(int, float));

    MOCK_CONST_METHOD1_T(getValue, float(double)); // Only on WebFloatAnimationCurve, but can't hurt to have here.

    virtual WebCompositorAnimationCurve::AnimationCurveType type() const { return CurveId; }

    MOCK_METHOD0(delete_, void());
    ~WebCompositorAnimationCurveMock() { delete_(); }
};

using WebFloatAnimationCurveMock = WebCompositorAnimationCurveMock<WebFloatAnimationCurve, WebCompositorAnimationCurve::AnimationCurveTypeFloat, WebFloatKeyframe>;

class WebCompositorAnimationTimelineMock : public WebCompositorAnimationTimeline {
public:
    MOCK_METHOD1(playerAttached, void(const WebCompositorAnimationPlayerClient&));
    MOCK_METHOD1(playerDestroyed, void(const WebCompositorAnimationPlayerClient&));
};

class WebCompositorAnimationPlayerMock : public WebCompositorAnimationPlayer {
public:
    MOCK_METHOD1(setAnimationDelegate, void(WebCompositorAnimationDelegate*));

    MOCK_METHOD1(attachLayer, void(WebLayer*));
    MOCK_METHOD0(detachLayer, void());
    MOCK_CONST_METHOD0(isLayerAttached, bool());

    MOCK_METHOD1(addAnimation, void(WebCompositorAnimation*));
    MOCK_METHOD1(removeAnimation, void(int));
    MOCK_METHOD2(pauseAnimation, void(int, double));
};

} // namespace blink

namespace blink {

class AnimationCompositorAnimationsTestBase : public ::testing::Test {
public:
    AnimationCompositorAnimationsTestBase() : m_proxyPlatform(&m_mockCompositor) { }

    class WebCompositorSupportMock : public WebCompositorSupport {
    public:
        MOCK_METHOD4(createAnimation, WebCompositorAnimation*(const WebCompositorAnimationCurve& curve, WebCompositorAnimation::TargetProperty target, int groupId, int animationId));
        MOCK_METHOD0(createFloatAnimationCurve, WebFloatAnimationCurve*());

        MOCK_METHOD0(createAnimationPlayer, WebCompositorAnimationPlayer*());
        MOCK_METHOD0(createAnimationTimeline, WebCompositorAnimationTimeline*());
    };

private:
    class PlatformProxy : public TestingPlatformSupport {
    public:
        explicit PlatformProxy(WebCompositorSupportMock** compositor) : m_compositor(compositor) { }
    private:
        WebCompositorSupport* compositorSupport() override { return *m_compositor; }

        WebCompositorSupportMock** m_compositor;
    };

    WebCompositorSupportMock* m_mockCompositor;
    PlatformProxy m_proxyPlatform;

protected:
    virtual void SetUp()
    {
        m_mockCompositor = 0;
        Platform::initialize(&m_proxyPlatform);
    }

    void setCompositorForTesting(WebCompositorSupportMock& mock)
    {
        ASSERT(!m_mockCompositor);
        m_mockCompositor = &mock;
    }
};

}

#endif

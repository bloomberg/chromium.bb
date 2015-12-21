// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/AnimationInputHelpers.h"

#include "core/testing/DummyPageHolder.h"
#include "platform/animation/TimingFunction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AnimationAnimationInputHelpersTest, ParseKeyframePropertyAttributes)
{
    OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create();
    const Element& dummyElement = *dummyPageHolder->document().documentElement();

    EXPECT_EQ(CSSPropertyLineHeight, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("line-height"), dummyElement));
    EXPECT_EQ(CSSPropertyLineHeight, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("lineHeight"), dummyElement));
    EXPECT_EQ(CSSPropertyBorderTopWidth, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("borderTopWidth"), dummyElement));
    EXPECT_EQ(CSSPropertyBorderTopWidth, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("border-topWidth"), dummyElement));
    EXPECT_EQ(CSSPropertyWidth, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("width"), dummyElement));
    EXPECT_EQ(CSSPropertyFloat, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("float"), dummyElement));
    EXPECT_EQ(CSSPropertyFloat, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("cssFloat"), dummyElement));

    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("Width"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("-epub-text-transform"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("EpubTextTransform"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("-internal-marquee-repetition"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("InternalMarqueeRepetition"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("-webkit-filter"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("-webkit-transform"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("webkitTransform"), dummyElement));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSPropertyID(String("WebkitTransform"), dummyElement));
}

static bool timingFunctionRoundTrips(const String& string)
{
    RefPtr<TimingFunction> timingFunction = AnimationInputHelpers::parseTimingFunction(string);
    return timingFunction && string == timingFunction->toString();
}

TEST(AnimationAnimationInputHelpersTest, ParseAnimationTimingFunction)
{
    EXPECT_EQ(nullptr, AnimationInputHelpers::parseTimingFunction("initial"));
    EXPECT_EQ(nullptr, AnimationInputHelpers::parseTimingFunction("inherit"));
    EXPECT_EQ(nullptr, AnimationInputHelpers::parseTimingFunction("unset"));

    EXPECT_TRUE(timingFunctionRoundTrips("ease"));
    EXPECT_TRUE(timingFunctionRoundTrips("linear"));
    EXPECT_TRUE(timingFunctionRoundTrips("ease-in"));
    EXPECT_TRUE(timingFunctionRoundTrips("ease-out"));
    EXPECT_TRUE(timingFunctionRoundTrips("ease-in-out"));
    EXPECT_TRUE(timingFunctionRoundTrips("step-start"));
    EXPECT_TRUE(timingFunctionRoundTrips("step-middle"));
    EXPECT_TRUE(timingFunctionRoundTrips("step-end"));
    EXPECT_TRUE(timingFunctionRoundTrips("steps(3, start)"));
    EXPECT_TRUE(timingFunctionRoundTrips("steps(3, middle)"));
    EXPECT_TRUE(timingFunctionRoundTrips("steps(3, end)"));
    EXPECT_TRUE(timingFunctionRoundTrips("cubic-bezier(0.1, 5, 0.23, 0)"));

    EXPECT_EQ("steps(3, end)", AnimationInputHelpers::parseTimingFunction("steps(3)")->toString());

    EXPECT_EQ(nullptr, AnimationInputHelpers::parseTimingFunction("steps(3, nowhere)"));
    EXPECT_EQ(nullptr, AnimationInputHelpers::parseTimingFunction("steps(-3, end)"));
    EXPECT_EQ(nullptr, AnimationInputHelpers::parseTimingFunction("cubic-bezier(0.1, 0, 4, 0.4)"));
}

} // namespace blink

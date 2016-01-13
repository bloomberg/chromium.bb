// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationInputHelpers.h"

#include "platform/animation/TimingFunction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AnimationAnimationInputHelpersTest, ParseKeyframePropertyAttributes)
{
    EXPECT_EQ(CSSPropertyLineHeight, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("line-height")));
    EXPECT_EQ(CSSPropertyLineHeight, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("lineHeight")));
    EXPECT_EQ(CSSPropertyBorderTopWidth, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("borderTopWidth")));
    EXPECT_EQ(CSSPropertyBorderTopWidth, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("border-topWidth")));
    EXPECT_EQ(CSSPropertyWidth, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("width")));
    EXPECT_EQ(CSSPropertyFloat, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("float")));
    EXPECT_EQ(CSSPropertyFloat, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("cssFloat")));

    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("Width")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("-epub-text-transform")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("EpubTextTransform")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("-internal-marquee-repetition")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("InternalMarqueeRepetition")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("-webkit-filter")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("-webkit-transform")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("webkitTransform")));
    EXPECT_EQ(CSSPropertyInvalid, AnimationInputHelpers::keyframeAttributeToCSSProperty(String("WebkitTransform")));
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

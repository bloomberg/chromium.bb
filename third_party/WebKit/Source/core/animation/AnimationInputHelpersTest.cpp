// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationInputHelpers.h"

#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/animation/TimingFunction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationAnimationInputHelpersTest : public ::testing::Test {
public:
    CSSPropertyID keyframeAttributeToCSSProperty(const String& property)
    {
        return AnimationInputHelpers::keyframeAttributeToCSSProperty(property, *document);
    }

protected:
    void SetUp() override
    {
        pageHolder = DummyPageHolder::create();
        document = &pageHolder->document();
    }

    void TearDown() override
    {
        document.release();
#if ENABLE(OILPAN)
        Heap::collectAllGarbage();
#endif
    }

    OwnPtr<DummyPageHolder> pageHolder;
    RefPtrWillBePersistent<Document> document;
};

TEST_F(AnimationAnimationInputHelpersTest, ParseKeyframePropertyAttributes)
{
    EXPECT_EQ(CSSPropertyLineHeight, keyframeAttributeToCSSProperty("lineHeight"));
    EXPECT_EQ(CSSPropertyBorderTopWidth, keyframeAttributeToCSSProperty("borderTopWidth"));
    EXPECT_EQ(CSSPropertyWidth, keyframeAttributeToCSSProperty("width"));
    EXPECT_EQ(CSSPropertyFloat, keyframeAttributeToCSSProperty("float"));
    EXPECT_EQ(CSSPropertyFloat, keyframeAttributeToCSSProperty("cssFloat"));

    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("line-height"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("border-topWidth"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("Width"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("-epub-text-transform"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("EpubTextTransform"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("-internal-marquee-repetition"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("InternalMarqueeRepetition"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("-webkit-filter"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("-webkit-transform"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("webkitTransform"));
    EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("WebkitTransform"));
}

static bool timingFunctionRoundTrips(const String& string)
{
    RefPtr<TimingFunction> timingFunction = AnimationInputHelpers::parseTimingFunction(string);
    return timingFunction && string == timingFunction->toString();
}

TEST_F(AnimationAnimationInputHelpersTest, ParseAnimationTimingFunction)
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

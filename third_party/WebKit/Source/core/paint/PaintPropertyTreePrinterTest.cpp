// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreePrinter.h"

#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifndef NDEBUG

namespace blink {

typedef bool TestParamRootLayerScrolling;
class PaintPropertyTreePrinterTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>
    , private ScopedSlimmingPaintV2ForTest
    , private ScopedRootLayerScrollingForTest
    , public RenderingTest {
public:
    PaintPropertyTreePrinterTest()
        : ScopedSlimmingPaintV2ForTest(true)
        , ScopedRootLayerScrollingForTest(GetParam())
        , RenderingTest(SingleChildFrameLoaderClient::create()) { }

private:
    void SetUp() override
    {
        Settings::setMockScrollbarsEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();
    }

    void TearDown() override
    {
        RenderingTest::TearDown();

        Settings::setMockScrollbarsEnabled(false);
    }
};

INSTANTIATE_TEST_CASE_P(All, PaintPropertyTreePrinterTest, ::testing::Bool());

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTree)
{
    setBodyInnerHTML("hello world");
    String transformTreeAsString = transformPropertyTreeAsString(*document().view());
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
        EXPECT_THAT(transformTreeAsString.ascii().data(),
            testing::MatchesRegex("PaintOffsetTranslation \\(LayoutView #document\\) .*"
                "  ScrollTranslation \\(LayoutView #document\\) \\w+ .*"));
    } else {
        EXPECT_THAT(transformTreeAsString.ascii().data(),
            testing::MatchesRegex("root .*"
                "  PreTranslation \\(FrameView\\) .*"
                "    ScrollTranslation \\(FrameView\\) .*"));
    }
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTree)
{
    setBodyInnerHTML("hello world");
    String clipTreeAsString = clipPropertyTreeAsString(*document().view());
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
        EXPECT_THAT(clipTreeAsString.ascii().data(),
            testing::MatchesRegex("root \\w+ .*"
                "  OverflowClip \\(LayoutView #document\\) .*"));
    } else {
        EXPECT_THAT(clipTreeAsString.ascii().data(),
            testing::MatchesRegex("root .*"
                "  ContentClip \\(FrameView\\) .*"));
    }
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTree)
{
    setBodyInnerHTML("<div style='opacity: 0.9;'>hello world</div>");
    String effectTreeAsString = effectPropertyTreeAsString(*document().view());
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
        EXPECT_THAT(effectTreeAsString.ascii().data(),
            testing::MatchesRegex("Effect \\(LayoutView #document\\) .*"
                "  Effect \\(LayoutBlockFlow DIV\\) .*"));
    } else {
        EXPECT_THAT(effectTreeAsString.ascii().data(),
            testing::MatchesRegex("root .*"
                "  Effect \\(LayoutBlockFlow DIV\\) .*"));
    }
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTree)
{
    setBodyInnerHTML("<div style='height: 4000px;'>hello world</div>");
    String scrollTreeAsString = scrollPropertyTreeAsString(*document().view());
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
        EXPECT_THAT(scrollTreeAsString.ascii().data(),
            testing::MatchesRegex("Scroll \\(LayoutView #document\\) .*"));
    } else {
        EXPECT_THAT(scrollTreeAsString.ascii().data(),
            testing::MatchesRegex("root .*"
                "  Scroll \\(FrameView\\) .*"));
    }
}

} // namespace blink

#endif

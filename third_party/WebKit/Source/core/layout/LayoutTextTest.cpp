// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutText.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/line/InlineTextBox.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class LayoutTextTest : public RenderingTest {
public:
    void setBasicBody(const char* message)
    {
        setBodyInnerHTML(String::format("<div id='target' style='font-size: 10px;'>%s</div>", message));
    }

    LayoutText* getBasicText()
    {
        return toLayoutText(getLayoutObjectByElementId("target")->slowFirstChild());
    }
};

const char* kTacoText = "Los Compadres Taco Truck";

} // namespace

TEST_F(LayoutTextTest, WidthZeroFromZeroLength)
{
    setBasicBody(kTacoText);
    ASSERT_EQ(0, getBasicText()->width(0u, 0u, LayoutUnit(), LTR, false));
}

TEST_F(LayoutTextTest, WidthMaxFromZeroLength)
{
    setBasicBody(kTacoText);
    ASSERT_EQ(0, getBasicText()->width(std::numeric_limits<unsigned>::max(), 0u, LayoutUnit(), LTR, false));
}

TEST_F(LayoutTextTest, WidthZeroFromMaxLength)
{
    setBasicBody(kTacoText);
    float width = getBasicText()->width(0u, std::numeric_limits<unsigned>::max(), LayoutUnit(), LTR, false);
    // Width may vary by platform and we just want to make sure it's something roughly reasonable.
    ASSERT_GE(width, 100.f);
    ASSERT_LE(width, 160.f);
}

TEST_F(LayoutTextTest, WidthMaxFromMaxLength)
{
    setBasicBody(kTacoText);
    ASSERT_EQ(0, getBasicText()->width(std::numeric_limits<unsigned>::max(),
        std::numeric_limits<unsigned>::max(), LayoutUnit(), LTR, false));
}

TEST_F(LayoutTextTest, WidthWithHugeLengthAvoidsOverflow)
{
    // The test case from http://crbug.com/647820 uses a 288-length string, so for posterity we follow that closely.
    setBodyInnerHTML("<div id='target'>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "</div>");
    // Width may vary by platform and we just want to make sure it's something roughly reasonable.
    float width = getBasicText()->width(23u, 4294967282u, LayoutUnit(2.59375), RTL, false);
    ASSERT_GE(width, 100.f);
    ASSERT_LE(width, 300.f);
}

TEST_F(LayoutTextTest, WidthFromBeyondLength)
{
    setBasicBody("x");
    ASSERT_EQ(0u, getBasicText()->width(1u, 1u, LayoutUnit(), LTR, false));
}

TEST_F(LayoutTextTest, WidthLengthBeyondLength)
{
    setBasicBody("x");
    // Width may vary by platform and we just want to make sure it's something roughly reasonable.
    float width = getBasicText()->width(0u, 2u, LayoutUnit(), LTR, false);
    ASSERT_GE(width, 4.f);
    ASSERT_LE(width, 20.f);
}

TEST_F(LayoutTextTest, ConsecutiveWhitespaceRenderersInsertAfterWS)
{
    setBodyInnerHTML("<span></span> <span id='span'></span>");

    Element* span = document().getElementById("span");
    Node* whitespace1 = span->previousSibling();
    EXPECT_TRUE(whitespace1->layoutObject());

    Node* whitespace2 = Text::create(document(), " ");
    document().body()->insertBefore(whitespace2, span, ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(whitespace1->layoutObject());
    EXPECT_FALSE(whitespace2->layoutObject());
}

TEST_F(LayoutTextTest, ConsecutiveWhitespaceRenderersInsertBeforeWS)
{
    setBodyInnerHTML("<span id='span'></span> <span></span>");

    Element* span = document().getElementById("span");
    Node* whitespace2 = span->nextSibling();
    EXPECT_TRUE(whitespace2->layoutObject());

    Node* whitespace1 = Text::create(document(), " ");
    document().body()->insertBefore(whitespace1, whitespace2, ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(whitespace1->layoutObject());
    EXPECT_FALSE(whitespace2->layoutObject());
}

TEST_F(LayoutTextTest, ConsecutiveWhitespaceRenderersDisplayNone)
{
    setBodyInnerHTML("<span></span> <span style='display:none'></span> <span></span>");

    // First <span>
    Node* child = document().body()->firstChild();
    ASSERT_TRUE(child);

    // First whitespace node
    child = child->nextSibling();
    ASSERT_TRUE(child);
    EXPECT_TRUE(child->isTextNode());
    EXPECT_TRUE(child->layoutObject());

    // <span display:none>
    child = child->nextSibling();
    ASSERT_TRUE(child);
    EXPECT_FALSE(child->layoutObject());

    // Second whitespace node
    child = child->nextSibling();
    ASSERT_TRUE(child);
    EXPECT_TRUE(child->isTextNode());
    EXPECT_FALSE(child->layoutObject());
}

TEST_F(LayoutTextTest, ConsecutiveWhitespaceRenderersComment)
{
    setBodyInnerHTML("<span></span> <!-- --> <span></span>");

    // First <span>
    Node* child = document().body()->firstChild();
    ASSERT_TRUE(child);

    // First whitespace node
    child = child->nextSibling();
    ASSERT_TRUE(child);
    EXPECT_TRUE(child->isTextNode());
    EXPECT_TRUE(child->layoutObject());

    // Comment node
    child = child->nextSibling();
    ASSERT_TRUE(child);
    EXPECT_FALSE(child->layoutObject());

    // Second whitespace node
    child = child->nextSibling();
    ASSERT_TRUE(child);
    EXPECT_TRUE(child->isTextNode());
    EXPECT_FALSE(child->layoutObject());
}

} // namespace blink

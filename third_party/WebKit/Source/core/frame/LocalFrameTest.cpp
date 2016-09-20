// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrame.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutObject.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/DragImage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LocalFrameTest : public ::testing::Test {
protected:
    LocalFrameTest() = default;
    ~LocalFrameTest() override = default;

    Document& document() const { return m_dummyPageHolder->document(); }
    LocalFrame& frame() const { return *document().frame(); }

    void setBodyContent(const std::string& bodyContent)
    {
        document().body()->setInnerHTML(String::fromUTF8(bodyContent.c_str()), ASSERT_NO_EXCEPTION);
        updateAllLifecyclePhases();
    }

    void updateAllLifecyclePhases()
    {
        document().view()->updateAllLifecyclePhases();
    }

private:
    void SetUp() override
    {
        m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    }

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(LocalFrameTest, nodeImage)
{
    setBodyContent(
        "<style>"
        "#sample { width: 100px; height: 100px; }"
        "</style>"
        "<div id=sample></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(IntSize(100, 100), image->size());
}

TEST_F(LocalFrameTest, nodeImageWithNestedElement)
{
    setBodyContent(
        "<style>"
        "div { -webkit-user-drag: element }"
        "span:-webkit-drag { color: #0F0 }"
        "</style>"
        "<div id=sample><span>Green when dragged</span></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(Color(0, 255, 0),
        sample->firstChild()->layoutObject()->resolveColor(CSSPropertyColor))
        << "Descendants node should have :-webkit-drag.";
}

TEST_F(LocalFrameTest, nodeImageWithPsuedoClassWebKitDrag)
{
    setBodyContent(
        "<style>"
        "#sample { width: 100px; height: 100px; }"
        "#sample:-webkit-drag { width: 200px; height: 200px; }"
        "</style>"
        "<div id=sample></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(IntSize(200, 200), image->size())
        << ":-webkit-drag should affect dragged image.";
}

TEST_F(LocalFrameTest, nodeImageWithoutDraggedLayoutObject)
{
    setBodyContent(
        "<style>"
        "#sample { width: 100px; height: 100px; }"
        "#sample:-webkit-drag { display:none }"
        "</style>"
        "<div id=sample></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(nullptr, image.get())
        << ":-webkit-drag blows away layout object";
}

TEST_F(LocalFrameTest, nodeImageWithChangingLayoutObject)
{
    setBodyContent(
        "<style>"
        "#sample { color: blue; }"
        "#sample:-webkit-drag { display: inline-block; color: red; }"
        "</style>"
        "<span id=sample>foo</span>");
    Element* sample = document().getElementById("sample");
    updateAllLifecyclePhases();
    LayoutObject* beforeLayoutObject = sample->layoutObject();
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);

    EXPECT_TRUE(sample->layoutObject() != beforeLayoutObject)
        << ":-webkit-drag causes sample to have different layout object.";
    EXPECT_EQ(Color(255, 0, 0),
        sample->layoutObject()->resolveColor(CSSPropertyColor))
        << "#sample has :-webkit-drag.";

    // Layout w/o :-webkit-drag
    updateAllLifecyclePhases();

    EXPECT_EQ(Color(0, 0, 255),
        sample->layoutObject()->resolveColor(CSSPropertyColor))
        << "#sample doesn't have :-webkit-drag.";
}

} // namespace blink

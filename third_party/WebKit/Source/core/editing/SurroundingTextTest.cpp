// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/SurroundingText.h"

#include "core/dom/Position.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/VisibleSelection.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class SurroundingTextTest : public ::testing::Test {
protected:
    Document& document() const { return m_dummyPageHolder->document(); }
    void setHTML(const String&);
    VisibleSelection select(int offset) { return select(offset, offset); }
    VisibleSelection select(int start, int end);

private:
    virtual void SetUp() OVERRIDE;

    OwnPtr<DummyPageHolder> m_dummyPageHolder;
};

void SurroundingTextTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

void SurroundingTextTest::setHTML(const String& content)
{
    document().body()->setInnerHTML(content, ASSERT_NO_EXCEPTION);
}

VisibleSelection SurroundingTextTest::select(int start, int end)
{
    Element* element = document().getElementById("selection");
    VisibleSelection selection;
    selection.setBase(Position(toText(element->firstChild()), start));
    selection.setExtent(Position(toText(element->firstChild()), end));
    return selection;
}

TEST_F(SurroundingTextTest, BasicCaretSelection)
{
    setHTML(String("<p id='selection'>foo bar</p>"));

    {
        VisibleSelection selection = select(0);
        SurroundingText surroundingText(selection.start(), 1);

        EXPECT_EQ("f", surroundingText.content());
        EXPECT_EQ(0u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(0);
        SurroundingText surroundingText(selection.start(), 5);

        // maxlength/2 is used on the left and right.
        EXPECT_EQ("foo", surroundingText.content().simplifyWhiteSpace());
        EXPECT_EQ(1u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(0);
        SurroundingText surroundingText(selection.start(), 42);

        EXPECT_EQ("foo bar", surroundingText.content().simplifyWhiteSpace());
        EXPECT_EQ(1u, surroundingText.positionOffsetInContent());
    }

    {
        // FIXME: if the selection is at the end of the text, SurroundingText
        // will return nothing.
        VisibleSelection selection = select(7);
        SurroundingText surroundingText(selection.start(), 42);

        EXPECT_EQ(0u, surroundingText.content().length());
        EXPECT_EQ(0u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(6);
        SurroundingText surroundingText(selection.start(), 2);

        EXPECT_EQ("ar", surroundingText.content());
        EXPECT_EQ(1u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(6);
        SurroundingText surroundingText(selection.start(), 42);

        EXPECT_EQ("foo bar", surroundingText.content().simplifyWhiteSpace());
        EXPECT_EQ(7u, surroundingText.positionOffsetInContent());
    }
}

TEST_F(SurroundingTextTest, TreeCaretSelection)
{
    setHTML(String("<div>This is outside of <p id='selection'>foo bar</p> the selected node</div>"));

    {
        VisibleSelection selection = select(0);
        SurroundingText surroundingText(selection.start(), 1);

        EXPECT_EQ("f", surroundingText.content());
        EXPECT_EQ(0u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(0);
        SurroundingText surroundingText(selection.start(), 5);

        EXPECT_EQ("foo", surroundingText.content().simplifyWhiteSpace());
        EXPECT_EQ(1u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(0);
        SurroundingText surroundingText(selection.start(), 1337);

        EXPECT_EQ("This is outside of foo bar the selected node", surroundingText.content().simplifyWhiteSpace());
        EXPECT_EQ(20u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(6);
        SurroundingText surroundingText(selection.start(), 2);

        EXPECT_EQ("ar", surroundingText.content());
        EXPECT_EQ(1u, surroundingText.positionOffsetInContent());
    }

    {
        VisibleSelection selection = select(6);
        SurroundingText surroundingText(selection.start(), 1337);

        EXPECT_EQ("This is outside of foo bar the selected node", surroundingText.content().simplifyWhiteSpace());
        EXPECT_EQ(26u, surroundingText.positionOffsetInContent());
    }
}

} // anonymous namespace

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/StyledMarkupSerializer.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/markup.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/testing/WTFTestHelpers.h"
#include <gtest/gtest.h>
#include <string>

namespace blink {

// This is smoke test of |StyledMarkupSerializer|. Full testing will be done
// in layout tests.
class StyledMarkupSerializerTest : public ::testing::Test {
protected:
    void SetUp() override;

    HTMLDocument& document() const { return *m_document; }

    template <typename Tree>
    std::string serialize();

    void setBodyContent(const char*);
    PassRefPtrWillBeRawPtr<ShadowRoot> setShadowContent(const char*);

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    HTMLDocument* m_document;
};

void StyledMarkupSerializerTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

template <typename Tree>
std::string StyledMarkupSerializerTest::serialize()
{
    using PositionType = typename Tree::PositionType;
    PositionType start = PositionType(m_document->body(), PositionType::PositionIsBeforeChildren);
    PositionType end = PositionType(m_document->body(), PositionType::PositionIsAfterChildren);
    return createMarkup(start, end).utf8().data();
}

static PassRefPtrWillBeRawPtr<ShadowRoot> createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRoot(ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    return shadowRoot.release();
}

void StyledMarkupSerializerTest::setBodyContent(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

PassRefPtrWillBeRawPtr<ShadowRoot> StyledMarkupSerializerTest::setShadowContent(const char* shadowContent)
{
    return createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
}

TEST_F(StyledMarkupSerializerTest, TextOnly)
{
    const char* bodyContent = "Hello world!";
    setBodyContent(bodyContent);
    const char* expectedResult = "<span style=\"display: inline !important; float: none;\">Hello world!</span>";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, BlockFormatting)
{
    const char* bodyContent = "<div>Hello world!</div>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInput)
{
    const char* bodyContent = "<input value='foo'>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<input value=\"foo\">";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInputRange)
{
    const char* bodyContent = "<input type=range>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<input type=\"range\">";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlSelect)
{
    const char* bodyContent = "<select><option value=\"1\">one</option><option value=\"2\">two</option></select>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlTextArea)
{
    const char* bodyContent = "<textarea>foo bar</textarea>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<textarea></textarea>";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>())
        << "contents of TEXTAREA element should not be appeared.";
}

TEST_F(StyledMarkupSerializerTest, HeadingFormatting)
{
    const char* bodyContent = "<h4>Hello world!</h4>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, InlineFormatting)
{
    const char* bodyContent = "<b>Hello world!</b>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, Mixed)
{
    const char* bodyContent = "<i>foo<b>bar</b>baz</i>";
    setBodyContent(bodyContent);
    EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeDistributeOrder)
{
    const char* bodyContent = "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
    const char* shadowContent = "<a><content select=#two></content><content select=#one></content></a>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>", serialize<EditingStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeNested)
{
    const char* bodyContent = "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
    const char* shadowContent1 = "<a><content select=#two></content><b id=host2></b><content select=#one></content></a>";
    const char* shadowContent2 = "NESTED";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot1 = setShadowContent(shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host2", shadowContent2);

    EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>", serialize<EditingStrategy>())
        << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNone)
{
    const char* bodyContent = "<b>00<i style='display:none'>11</i>22</b>";
    setBodyContent(bodyContent);
    const char* expectedResult = "<b>0022</b>";
    EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
}

} // namespace blink

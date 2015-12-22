// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebNode.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class WebNodeTest : public testing::Test {
protected:
    Document& document()
    {
        return m_pageHolder->document();
    }

    void setInnerHTML(const String& html)
    {
        document().documentElement()->setInnerHTML(html, ASSERT_NO_EXCEPTION);
    }

    WebNode root()
    {
        return WebNode(document().documentElement());
    }

private:
    void SetUp() override;

    OwnPtr<DummyPageHolder> m_pageHolder;
};

void WebNodeTest::SetUp()
{
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(WebNodeTest, QuerySelectorMatches)
{
    setInnerHTML("<div id=x><span class=a></span></div>");
    WebExceptionCode ec;
    WebElement element = root().querySelector(".a", ec);
    EXPECT_EQ(0, ec);
    EXPECT_FALSE(element.isNull());
    EXPECT_TRUE(element.hasHTMLTagName("span"));
}

TEST_F(WebNodeTest, QuerySelectorDoesNotMatch)
{
    setInnerHTML("<div id=x><span class=a></span></div>");
    WebExceptionCode ec;
    WebElement element = root().querySelector("section", ec);
    EXPECT_EQ(0, ec);
    EXPECT_TRUE(element.isNull());
}

TEST_F(WebNodeTest, QuerySelectorError)
{
    setInnerHTML("<div></div>");
    WebExceptionCode ec;
    WebElement element = root().querySelector("@invalid-selector", ec);
    EXPECT_NE(0, ec);
    EXPECT_TRUE(element.isNull());
}

TEST_F(WebNodeTest, QuerySelectorAllMatches)
{
    setInnerHTML("<div id=x><span class=a></span></div>");
    WebExceptionCode ec;
    WebVector<WebElement> results;
    root().querySelectorAll(".a, #x", results, ec);
    EXPECT_EQ(0, ec);
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results[0].hasHTMLTagName("div"));
    EXPECT_TRUE(results[1].hasHTMLTagName("span"));
}

TEST_F(WebNodeTest, QuerySelectorAllDoesNotMatch)
{
    setInnerHTML("<div id=x><span class=a></span></div>");
    WebExceptionCode ec;
    WebVector<WebElement> results;
    root().querySelectorAll(".bar, #foo", results, ec);
    EXPECT_EQ(0, ec);
    EXPECT_TRUE(results.isEmpty());
}

TEST_F(WebNodeTest, QuerySelectorAllError)
{
    setInnerHTML("<div></div>");
    WebExceptionCode ec;
    WebVector<WebElement> results;
    root().querySelectorAll("@invalid-selector", results, ec);
    EXPECT_NE(0, ec);
    EXPECT_TRUE(results.isEmpty());
}

TEST_F(WebNodeTest, GetElementsByHTMLTagName)
{
    setInnerHTML("<body><LABEL></LABEL><svg xmlns='http://www.w3.org/2000/svg'><label></label></svg></body>");
    // WebNode::getElementsByHTMLTagName returns only HTML elements.
    WebElementCollection collection = root().getElementsByHTMLTagName("label");
    EXPECT_EQ(1u, collection.length());
    EXPECT_TRUE(collection.firstItem().hasHTMLTagName("label"));
    // The argument should be lower-case.
    collection = root().getElementsByHTMLTagName("LABEL");
    EXPECT_EQ(0u, collection.length());
}

} // namespace blink

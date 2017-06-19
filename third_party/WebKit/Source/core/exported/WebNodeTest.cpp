// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebNode.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class WebNodeTest : public testing::Test {
 protected:
  Document& GetDocument() { return page_holder_->GetDocument(); }

  void SetInnerHTML(const String& html) {
    GetDocument().documentElement()->setInnerHTML(html);
  }

  WebNode Root() { return WebNode(GetDocument().documentElement()); }

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> page_holder_;
};

void WebNodeTest::SetUp() {
  page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

TEST_F(WebNodeTest, QuerySelectorMatches) {
  SetInnerHTML("<div id=x><span class=a></span></div>");
  WebElement element = Root().QuerySelector(".a");
  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element.HasHTMLTagName("span"));
}

TEST_F(WebNodeTest, QuerySelectorDoesNotMatch) {
  SetInnerHTML("<div id=x><span class=a></span></div>");
  WebElement element = Root().QuerySelector("section");
  EXPECT_TRUE(element.IsNull());
}

TEST_F(WebNodeTest, QuerySelectorError) {
  SetInnerHTML("<div></div>");
  WebElement element = Root().QuerySelector("@invalid-selector");
  EXPECT_TRUE(element.IsNull());
}

TEST_F(WebNodeTest, GetElementsByHTMLTagName) {
  SetInnerHTML(
      "<body><LABEL></LABEL><svg "
      "xmlns='http://www.w3.org/2000/svg'><label></label></svg></body>");
  // WebNode::getElementsByHTMLTagName returns only HTML elements.
  WebElementCollection collection = Root().GetElementsByHTMLTagName("label");
  EXPECT_EQ(1u, collection.length());
  EXPECT_TRUE(collection.FirstItem().HasHTMLTagName("label"));
  // The argument should be lower-case.
  collection = Root().GetElementsByHTMLTagName("LABEL");
  EXPECT_EQ(0u, collection.length());
}

}  // namespace blink

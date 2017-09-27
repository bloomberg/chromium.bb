// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ShadowRoot.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const char kBlockWithContinuations[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div id='testElement'>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

static const char kEmptyBlock[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body> <form id='testElement'> </form> </body>";

static const char kEmptyInline[] =
    "<body> <span id='testElement'> </span> </body>";

static const char kBlockWithDisplayNone[] =
    "<head> <style> form {display: none;} </style> </head>"
    "<body>"
    "  <form id='testElement'>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

static const char kBlockWithContent[] =
    "<div id='testElement'>"
    "  <div>Hello</div> "
    "</div>";

static const char kBlockWithText[] =
    "<div id='testElement'>"
    "  <div>Hello</div> "
    "</div>";

static const char kBlockWithInlines[] =
    "<div id='testElement'>"
    "  <span>Hello</span> "
    "</div>";

static const char kBlockWithEmptyInlines[] =
    "<div id='testElement'>"
    "  <span></span> "
    "</div>";

class WebElementTest : public ::testing::Test {
 protected:
  Document& GetDocument() { return page_holder_->GetDocument(); }
  void InsertHTML(String html);
  WebElement TestElement();

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> page_holder_;
};

void WebElementTest::InsertHTML(String html) {
  GetDocument().documentElement()->SetInnerHTMLFromString(html);
}

WebElement WebElementTest::TestElement() {
  Element* element = GetDocument().getElementById("testElement");
  DCHECK(element);
  return WebElement(element);
}

void WebElementTest::SetUp() {
  page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

TEST_F(WebElementTest, HasNonEmptyLayoutSize) {
  InsertHTML(kEmptyBlock);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kEmptyInline);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithDisplayNone);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithEmptyInlines);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithContinuations);
  EXPECT_TRUE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithInlines);
  EXPECT_TRUE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithContent);
  EXPECT_TRUE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithText);
  EXPECT_TRUE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kEmptyBlock);
  ShadowRoot& root =
      GetDocument().getElementById("testElement")->CreateShadowRootInternal();
  root.SetInnerHTMLFromString("<div>Hello World</div>");
  EXPECT_TRUE(TestElement().HasNonEmptyLayoutSize());
}

TEST_F(WebElementTest, IsEditable) {
  InsertHTML("<div id=testElement></div>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<div id=testElement contenteditable=true></div>");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML(
      "<div style='-webkit-user-modify: read-write'>"
      "  <div id=testElement></div>"
      "</div>");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML(
      "<div style='-webkit-user-modify: read-write'>"
      "  <div id=testElement style='-webkit-user-modify: read-only'></div>"
      "</div>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<input id=testElement>");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML("<input id=testElement readonly>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<input id=testElement disabled>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<fieldset disabled><div><input id=testElement></div></fieldset>");
  EXPECT_FALSE(TestElement().IsEditable());
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ShadowRoot.h"
#include "core/testing/PageTestBase.h"
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

static const char kBlockWithEmptyZeroSizedSVG[] =
    "<div id='testElement'>"
    "  <svg height='0'><g><rect width='100' height='100'/></g></svg> "
    "</div>";

static const char kBlockWithInlines[] =
    "<div id='testElement'>"
    "  <span>Hello</span> "
    "</div>";

static const char kBlockWithEmptyInlines[] =
    "<div id='testElement'>"
    "  <span></span> "
    "</div>";

static const char kBlockWithEmptyFirstChild[] =
    "<div id='testElement'>"
    "  <div style='position: absolute'></div> "
    "  <div style='position: absolute'>Hello</div> "
    "</div>";

class WebElementTest : public PageTestBase {
 protected:
  void InsertHTML(String html);
  WebElement TestElement();
};

void WebElementTest::InsertHTML(String html) {
  GetDocument().documentElement()->SetInnerHTMLFromString(html);
}

WebElement WebElementTest::TestElement() {
  Element* element = GetDocument().getElementById("testElement");
  DCHECK(element);
  return WebElement(element);
}

TEST_F(WebElementTest, HasNonEmptyLayoutSize) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  InsertHTML(kEmptyBlock);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kEmptyInline);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithDisplayNone);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithEmptyInlines);
  EXPECT_FALSE(TestElement().HasNonEmptyLayoutSize());

  InsertHTML(kBlockWithEmptyZeroSizedSVG);
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

  InsertHTML(kBlockWithEmptyFirstChild);
  EXPECT_TRUE(TestElement().HasNonEmptyLayoutSize());
}

TEST_F(WebElementTest, IsEditable) {
  InsertHTML("<div id=testElement></div>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<div id=testElement contenteditable=true></div>");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML(R"HTML(
    <div style='-webkit-user-modify: read-write'>
      <div id=testElement></div>
    </div>
  )HTML");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML(R"HTML(
    <div style='-webkit-user-modify: read-write'>
      <div id=testElement style='-webkit-user-modify: read-only'></div>
    </div>
  )HTML");
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

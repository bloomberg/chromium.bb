// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_element.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_init.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

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

TEST_F(WebElementTest, IsAutonomousCustomElement) {
  InsertHTML("<x-undefined id=testElement></x-undefined>");
  EXPECT_FALSE(TestElement().IsAutonomousCustomElement());
  InsertHTML("<div id=testElement></div>");
  EXPECT_FALSE(TestElement().IsAutonomousCustomElement());

  GetDocument().GetSettings()->SetScriptEnabled(true);
  auto* script = GetDocument().CreateRawElement(HTMLNames::scriptTag);
  script->setTextContent(R"JS(
    customElements.define('v1-custom', class extends HTMLElement {});
    document.body.appendChild(document.createElement('v1-custom'));
    customElements.define('v1-builtin',
                          class extends HTMLButtonElement {},
                          { extends:'button' });
    document.body.appendChild(
        document.createElement('button', { is: 'v1-builtin' }));

    document.registerElement('v0-custom');
    document.body.appendChild(document.createElement('v0-custom'));
    document.registerElement('v0-typext', {
        prototype: Object.create(HTMLInputElement.prototype),
        extends: 'input' });
    document.body.appendChild(document.createElement('input', 'v0-typeext'));
  )JS");
  GetDocument().body()->appendChild(script);
  auto* v0typeext = GetDocument().body()->lastChild();
  EXPECT_FALSE(WebElement(ToElement(v0typeext)).IsAutonomousCustomElement());
  auto* v0autonomous = v0typeext->previousSibling();
  EXPECT_TRUE(WebElement(ToElement(v0autonomous)).IsAutonomousCustomElement());
  auto* v1builtin = v0autonomous->previousSibling();
  EXPECT_FALSE(WebElement(ToElement(v1builtin)).IsAutonomousCustomElement());
  auto* v1autonomous = v1builtin->previousSibling();
  EXPECT_TRUE(WebElement(ToElement(v1autonomous)).IsAutonomousCustomElement());
}

TEST_F(WebElementTest, ShadowRoot) {
  InsertHTML("<input id=testElement>");
  EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
      << "ShadowRoot() should not return a UA ShadowRoot.";

  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    InsertHTML("<div id=testElement></div>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById("testElement");
    element->createShadowRoot(script_state, ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(TestElement().ShadowRoot().IsNull())
        << "Should return V0 ShadowRoot.";
  }

  {
    InsertHTML("<span id=testElement></span>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById("testElement");
    ShadowRootInit init;
    init.setMode("open");
    element->attachShadow(script_state, init, ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(TestElement().ShadowRoot().IsNull())
        << "Should return V1 open ShadowRoot.";
  }

  {
    InsertHTML("<p id=testElement></p>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById("testElement");
    ShadowRootInit init;
    init.setMode("closed");
    element->attachShadow(script_state, init, ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(TestElement().ShadowRoot().IsNull())
        << "Should return V1 closed ShadowRoot.";
  }
}

}  // namespace blink

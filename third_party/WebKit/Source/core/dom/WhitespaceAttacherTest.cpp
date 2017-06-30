// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/WhitespaceAttacher.h"

#include <gtest/gtest.h>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/ShadowRootInit.h"
#include "core/layout/LayoutText.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

class WhitespaceAttacherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  ShadowRoot& AttachShadow(Element& host);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

ShadowRoot& WhitespaceAttacherTest::AttachShadow(Element& host) {
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host.attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                        init, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(shadow_root);
  return *shadow_root;
}

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedBlock) {
  GetDocument().body()->setInnerHTML("<div id=block></div> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("block");
  Text* text = ToText(div->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(
      text->CreateTextLayoutObject(GetDocument().body()->ComputedStyleRef()));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedInline) {
  GetDocument().body()->setInnerHTML("<span id=inline></span> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Text* text = ToText(span->nextSibling());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceAfterReattachedWhitespace) {
  GetDocument().body()->setInnerHTML("<span id=inline></span> <!-- --> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Text* first_whitespace = ToText(span->nextSibling());
  Text* second_whitespace =
      ToText(first_whitespace->nextSibling()->nextSibling());
  EXPECT_TRUE(first_whitespace->GetLayoutObject());
  EXPECT_FALSE(second_whitespace->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Force LayoutText on the second whitespace to see that the reattach works.
  second_whitespace->SetLayoutObject(second_whitespace->CreateTextLayoutObject(
      GetDocument().body()->ComputedStyleRef()));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(second_whitespace);
  EXPECT_TRUE(second_whitespace->GetLayoutObject());

  attacher.DidReattachText(first_whitespace);
  EXPECT_TRUE(first_whitespace->GetLayoutObject());
  EXPECT_FALSE(second_whitespace->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitBlockAfterReattachedWhitespace) {
  GetDocument().body()->setInnerHTML("<div id=block></div> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("block");
  Text* text = ToText(div->nextSibling());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  WhitespaceAttacher attacher;
  attacher.DidReattachText(text);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidVisitElement(div);
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitInlineAfterReattachedWhitespace) {
  GetDocument().body()->setInnerHTML("<span id=inline></span> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Text* text = ToText(span->nextSibling());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidReattachText(text);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidVisitElement(span);
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, VisitTextAfterReattachedWhitespace) {
  GetDocument().body()->setInnerHTML("Text<!-- --> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Text* text = ToText(GetDocument().body()->firstChild());
  Text* whitespace = ToText(text->nextSibling()->nextSibling());
  EXPECT_TRUE(text->GetLayoutObject());
  EXPECT_TRUE(whitespace->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  whitespace->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidReattachText(whitespace);
  EXPECT_FALSE(whitespace->GetLayoutObject());

  attacher.DidVisitText(text);
  EXPECT_TRUE(text->GetLayoutObject());
  EXPECT_TRUE(whitespace->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, ReattachWhitespaceInsideBlockExitingScope) {
  GetDocument().body()->setInnerHTML("<div id=block> </div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("block");
  Text* text = ToText(div->firstChild());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  {
    WhitespaceAttacher attacher;
    attacher.DidReattachText(text);
    EXPECT_FALSE(text->GetLayoutObject());

    // Force LayoutText to see that the reattach works.
    text->SetLayoutObject(
        text->CreateTextLayoutObject(div->ComputedStyleRef()));
  }
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, ReattachWhitespaceInsideInlineExitingScope) {
  GetDocument().body()->setInnerHTML("<span id=inline> </span>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Text* text = ToText(span->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  {
    WhitespaceAttacher attacher;
    attacher.DidReattachText(text);
    EXPECT_FALSE(text->GetLayoutObject());
  }
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceAfterReattachedBlock) {
  GetDocument().body()->setInnerHTML("<div id=host> </div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root = AttachShadow(*host);
  shadow_root.setInnerHTML("<div id=block></div><slot></slot>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = shadow_root.getElementById("block");
  Text* text = ToText(host->firstChild());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(text->CreateTextLayoutObject(host->ComputedStyleRef()));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceAfterReattachedInline) {
  GetDocument().body()->setInnerHTML("<div id=host> </div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root = AttachShadow(*host);
  shadow_root.setInnerHTML("<span id=inline></span><slot></slot>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = shadow_root.getElementById("inline");
  Text* text = ToText(host->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceInDisplayContentsAfterReattachedBlock) {
  GetDocument().body()->setInnerHTML(
      "<div id=block></div><span style='display:contents'> </span>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("block");
  Element* contents = ToElement(div->nextSibling());
  Text* text = ToText(contents->firstChild());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(
      text->CreateTextLayoutObject(contents->ComputedStyleRef()));

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceInDisplayContentsAfterReattachedInline) {
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span><span style='display:contents'> </span>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Element* contents = ToElement(span->nextSibling());
  Text* text = ToText(contents->firstChild());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceAfterEmptyDisplayContentsAfterReattachedBlock) {
  GetDocument().body()->setInnerHTML(
      "<div id=block></div><span style='display:contents'></span> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("block");
  Element* contents = ToElement(div->nextSibling());
  Text* text = ToText(contents->nextSibling());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(
      text->CreateTextLayoutObject(contents->ComputedStyleRef()));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest,
       WhitespaceAfterDisplayContentsWithDisplayNoneChildAfterReattachedBlock) {
  GetDocument().body()->setInnerHTML(
      "<div id=block></div><span style='display:contents'>"
      "<span style='display:none'></span></span> ");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("block");
  Element* contents = ToElement(div->nextSibling());
  Text* text = ToText(contents->nextSibling());
  EXPECT_FALSE(contents->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Force LayoutText to see that the reattach works.
  text->SetLayoutObject(
      text->CreateTextLayoutObject(contents->ComputedStyleRef()));

  WhitespaceAttacher attacher;
  attacher.DidVisitText(text);
  attacher.DidVisitElement(contents);
  EXPECT_TRUE(text->GetLayoutObject());

  attacher.DidReattachElement(div, div->GetLayoutObject());
  EXPECT_FALSE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, WhitespaceDeepInsideDisplayContents) {
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span><span style='display:contents'>"
      "<span style='display:none'></span>"
      "<span id=inner style='display:contents'> </span></span>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Element* contents = ToElement(span->nextSibling());
  Text* text = ToText(GetDocument().getElementById("inner")->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, MultipleDisplayContents) {
  GetDocument().body()->setInnerHTML(
      "<span id=inline></span>"
      "<span style='display:contents'></span>"
      "<span style='display:contents'></span>"
      "<span style='display:contents'> </span>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("inline");
  Element* first_contents = ToElement(span->nextSibling());
  Element* second_contents = ToElement(first_contents->nextSibling());
  Element* last_contents = ToElement(second_contents->nextSibling());
  Text* text = ToText(last_contents->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(last_contents);
  attacher.DidVisitElement(second_contents);
  attacher.DidVisitElement(first_contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

TEST_F(WhitespaceAttacherTest, SlottedWhitespaceInsideDisplayContents) {
  GetDocument().body()->setInnerHTML("<div id=host> </div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root = AttachShadow(*host);
  shadow_root.setInnerHTML(
      "<span id=inline></span>"
      "<div style='display:contents'><slot></slot></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = shadow_root.getElementById("inline");
  Element* contents = ToElement(span->nextSibling());
  Text* text = ToText(host->firstChild());
  EXPECT_TRUE(text->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // Clear LayoutText to see that the reattach works.
  text->SetLayoutObject(nullptr);

  WhitespaceAttacher attacher;
  attacher.DidVisitElement(contents);
  EXPECT_FALSE(text->GetLayoutObject());

  attacher.DidReattachElement(span, span->GetLayoutObject());
  EXPECT_TRUE(text->GetLayoutObject());
}

}  // namespace blink

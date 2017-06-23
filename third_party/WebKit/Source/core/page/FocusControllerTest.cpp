// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/FocusController.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FocusControllerTest : public testing::Test {
 public:
  Document& GetDocument() const { return page_holder_->GetDocument(); }
  FocusController& GetFocusController() const {
    return GetDocument().GetPage()->GetFocusController();
  }
  DummyPageHolder* PageHolder() const { return page_holder_.get(); }

 private:
  void SetUp() override { page_holder_ = DummyPageHolder::Create(); }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(FocusControllerTest, SetInitialFocus) {
  GetDocument().body()->setInnerHTML("<input><textarea>");
  Element* input = ToElement(GetDocument().body()->firstChild());
  // Set sequential focus navigation point before the initial focus.
  input->focus();
  input->blur();
  GetFocusController().SetInitialFocus(kWebFocusTypeForward);
  EXPECT_EQ(input, GetDocument().FocusedElement())
      << "We should ignore sequential focus navigation starting point in "
         "setInitialFocus().";
}

TEST_F(FocusControllerTest, DoNotCrash1) {
  GetDocument().body()->setInnerHTML(
      "<div id='host'></div>This test is for crbug.com/609012<p id='target' "
      "tabindex='0'></p>");
  // <div> with shadow root
  Element* host = ToElement(GetDocument().body()->firstChild());
  ShadowRootInit init;
  init.setMode("open");
  host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()), init,
                     ASSERT_NO_EXCEPTION);
  // "This test is for crbug.com/609012"
  Node* text = host->nextSibling();
  // <p>
  Element* target = ToElement(text->nextSibling());

  // Set sequential focus navigation point at text node.
  GetDocument().SetSequentialFocusNavigationStartingPoint(text);

  GetFocusController().AdvanceFocus(kWebFocusTypeForward);
  EXPECT_EQ(target, GetDocument().FocusedElement())
      << "This should not hit assertion and finish properly.";
}

TEST_F(FocusControllerTest, DoNotCrash2) {
  GetDocument().body()->setInnerHTML(
      "<p id='target' tabindex='0'></p>This test is for crbug.com/609012<div "
      "id='host'></div>");
  // <p>
  Element* target = ToElement(GetDocument().body()->firstChild());
  // "This test is for crbug.com/609012"
  Node* text = target->nextSibling();
  // <div> with shadow root
  Element* host = ToElement(text->nextSibling());
  ShadowRootInit init;
  init.setMode("open");
  host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()), init,
                     ASSERT_NO_EXCEPTION);

  // Set sequential focus navigation point at text node.
  GetDocument().SetSequentialFocusNavigationStartingPoint(text);

  GetFocusController().AdvanceFocus(kWebFocusTypeBackward);
  EXPECT_EQ(target, GetDocument().FocusedElement())
      << "This should not hit assertion and finish properly.";
}

TEST_F(FocusControllerTest, SetActiveOnInactiveDocument) {
  // Test for crbug.com/700334
  GetDocument().Shutdown();
  // Document::shutdown() detaches document from its frame, and thus
  // document().page() becomes nullptr.
  // Use DummyPageHolder's page to retrieve FocusController.
  PageHolder()->GetPage().GetFocusController().SetActive(true);
}

// This test is for crbug.com/733218
TEST_F(FocusControllerTest, SVGFocusableElementInForm) {
  GetDocument().body()->setInnerHTML(
      "<form>"
      "<input id='first'>"
      "<svg width='100px' height='100px' tabindex='0'>"
      "<circle cx='50' cy='50' r='30' />"
      "</svg>"
      "<input id='last'>"
      "</form>");

  Element* form = ToElement(GetDocument().body()->firstChild());
  Element* first = ToElement(form->firstChild());
  Element* last = ToElement(form->lastChild());

  Element* next = GetFocusController().NextFocusableElementInForm(
      first, kWebFocusTypeForward);
  EXPECT_EQ(next, last)
      << "SVG Element should be skipped even when focusable in form.";

  Element* prev = GetFocusController().NextFocusableElementInForm(
      next, kWebFocusTypeBackward);
  EXPECT_EQ(prev, first)
      << "SVG Element should be skipped even when focusable in form.";
}

}  // namespace blink

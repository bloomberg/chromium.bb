// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/AXObject.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class AXObjectTest : public testing::Test {
 protected:
  Document& GetDocument() { return page_holder_->GetDocument(); }

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> page_holder_;
};

void AXObjectTest::SetUp() {
  page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

TEST_F(AXObjectTest, IsARIAWidget) {
  String test_content =
      "<body>"
      "<span id=\"plain\">plain</span><br>"
      "<span id=\"button\" role=\"button\">button</span><br>"
      "<span id=\"button-parent\" "
      "role=\"button\"><span>button-parent</span></span><br>"
      "<span id=\"button-caps\" role=\"BUTTON\">button-caps</span><br>"
      "<span id=\"button-second\" role=\"another-role "
      "button\">button-second</span><br>"
      "<span id=\"aria-bogus\" aria-bogus=\"bogus\">aria-bogus</span><br>"
      "<span id=\"aria-selected\" aria-selected>aria-selected</span><br>"
      "<span id=\"haspopup\" "
      "aria-haspopup=\"true\">aria-haspopup-true</span><br>"
      "<div id=\"focusable\" tabindex=\"1\">focusable</div><br>"
      "<div tabindex=\"2\"><div "
      "id=\"focusable-parent\">focusable-parent</div></div><br>"
      "</body>";

  GetDocument().documentElement()->setInnerHTML(test_content);
  GetDocument().UpdateStyleAndLayout();
  Element* root(GetDocument().documentElement());
  EXPECT_FALSE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("plain")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-parent")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-caps")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-second")));
  EXPECT_FALSE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("aria-bogus")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("aria-selected")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("haspopup")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("focusable")));
  EXPECT_TRUE(AXObject::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("focusable-parent")));
}

}  // namespace blink

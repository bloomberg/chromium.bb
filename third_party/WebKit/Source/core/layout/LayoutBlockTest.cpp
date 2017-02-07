// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementTraversal.h"
#include "core/layout/LayoutBlock.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutBlockTest : public RenderingTest {};

TEST_F(LayoutBlockTest, LayoutNameCalledWithNullStyle) {
  LayoutObject* obj = LayoutBlockFlow::createAnonymous(&document());
  EXPECT_FALSE(obj->style());
  EXPECT_STREQ("LayoutBlockFlow (anonymous)",
               obj->decoratedName().ascii().data());
  obj->destroy();
}

TEST_F(LayoutBlockTest, WidthAvailableToChildrenChanged) {
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(false);
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div id='list' style='overflow-y:auto; width:150px; height:100px'>"
      "  <div style='height:20px'>Item</div>"
      "  <div style='height:20px'>Item</div>"
      "  <div style='height:20px'>Item</div>"
      "  <div style='height:20px'>Item</div>"
      "  <div style='height:20px'>Item</div>"
      "  <div style='height:20px'>Item</div>"
      "</div>");
  Element* listElement = document().getElementById("list");
  ASSERT_TRUE(listElement);
  LayoutBox* listBox = toLayoutBox(listElement->layoutObject());
  Element* itemElement = ElementTraversal::firstChild(*listElement);
  ASSERT_TRUE(itemElement);
  ASSERT_GT(listBox->verticalScrollbarWidth(), 0);
  ASSERT_EQ(itemElement->offsetWidth(),
            150 - listBox->verticalScrollbarWidth());

  DummyExceptionStateForTesting exceptionState;
  listElement->style()->setCSSText("width:150px;height:100px;", exceptionState);
  ASSERT_FALSE(exceptionState.hadException());
  document().view()->updateAllLifecyclePhases();
  ASSERT_EQ(listBox->verticalScrollbarWidth(), 0);
  ASSERT_EQ(itemElement->offsetWidth(), 150);
}

}  // namespace blink

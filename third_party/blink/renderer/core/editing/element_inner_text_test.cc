// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class ElementInnerTest : public EditingTestBase {};

// http://crbug.com/877498
TEST_F(ElementInnerTest, ListItemWithLeadingWhiteSpace) {
  SetBodyContent("<li id=target> abc</li>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/877470
TEST_F(ElementInnerTest, SVGElementAsTableCell) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-cell'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/878725
TEST_F(ElementInnerTest, SVGElementAsTableRow) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-row'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

}  // namespace blink

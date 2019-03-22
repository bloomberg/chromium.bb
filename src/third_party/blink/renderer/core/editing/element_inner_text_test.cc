// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ElementInnerTest : public testing::WithParamInterface<bool>,
                         private ScopedLayoutNGForTest,
                         public EditingTestBase {
 protected:
  ElementInnerTest() : ScopedLayoutNGForTest(GetParam()) {}

  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(All, ElementInnerTest, testing::Bool());

// http://crbug.com/877498
TEST_P(ElementInnerTest, ListItemWithLeadingWhiteSpace) {
  SetBodyContent("<li id=target> abc</li>");
  Element& target = *GetDocument().getElementById("target");
  if (!LayoutNGEnabled()) {
    // TODO(crbug.com/908339) Actual result should be "abc", no leading space.
    EXPECT_EQ(" abc", target.innerText());
    return;
  }
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/877470
TEST_P(ElementInnerTest, SVGElementAsTableCell) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-cell'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/878725
TEST_P(ElementInnerTest, SVGElementAsTableRow) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-row'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

}  // namespace blink

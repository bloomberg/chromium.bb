// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/editing_strategy.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class EditingStrategyTest : public EditingTestBase {};

TEST_F(EditingStrategyTest, caretMaxOffset) {
  const char* body_content =
      "<p id='host'>00<b id='one'>1</b><b id='two'>22</b>333</p>";
  const char* shadow_content =
      "<content select=#two></content><content select=#one></content>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* host = GetDocument().getElementById("host");
  Node* one = GetDocument().getElementById("one");
  Node* two = GetDocument().getElementById("two");

  EXPECT_EQ(4, EditingStrategy::CaretMaxOffset(*host));
  EXPECT_EQ(1, EditingStrategy::CaretMaxOffset(*one));
  EXPECT_EQ(1, EditingStrategy::CaretMaxOffset(*one->firstChild()));
  EXPECT_EQ(2, EditingStrategy::CaretMaxOffset(*two->firstChild()));

  EXPECT_EQ(2, EditingInFlatTreeStrategy::CaretMaxOffset(*host));
  EXPECT_EQ(1, EditingInFlatTreeStrategy::CaretMaxOffset(*one));
  EXPECT_EQ(1, EditingInFlatTreeStrategy::CaretMaxOffset(*one->firstChild()));
  EXPECT_EQ(2, EditingInFlatTreeStrategy::CaretMaxOffset(*two->firstChild()));
}

}  // namespace blink

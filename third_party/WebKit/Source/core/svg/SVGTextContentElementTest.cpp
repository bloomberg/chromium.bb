// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGTextContentElement.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"

namespace blink {

class SVGTextContentElementTest : public EditingTestBase {};

TEST_F(SVGTextContentElementTest, selectSubStringNotCrash) {
  SetBodyContent("<svg><text style='visibility:hidden;'>Text</text></svg>");
  SVGTextContentElement* elem =
      ToSVGTextContentElement(GetDocument().QuerySelector("text"));
  VisiblePosition start = VisiblePosition::FirstPositionInNode(
      *const_cast<SVGTextContentElement*>(elem));
  EXPECT_TRUE(start.IsNull());
  // Pass if selecting hidden text is not crashed.
  elem->selectSubString(0, 1, ASSERT_NO_EXCEPTION);
}

}  // namespace blink

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Text.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/html/HTMLPreElement.h"
#include "core/layout/LayoutText.h"

namespace blink {

// TODO(xiaochengh): Use a new testing base class.
class TextTest : public EditingTestBase {};

TEST_F(TextTest, SetDataToChangeFirstLetterTextNode) {
  SetBodyContent(
      "<style>pre::first-letter {color:red;}</style><pre "
      "id=sample>a<span>b</span></pre>");

  Node* sample = GetDocument().GetElementById("sample");
  Text* text = ToText(sample->firstChild());
  text->setData(" ");
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(text->GetLayoutObject()->IsTextFragment());
}

TEST_F(TextTest, RemoveFirstLetterPseudoElementWhenNoLetter) {
  SetBodyContent("<style>*::first-letter{font:icon;}</style><pre>AB\n</pre>");

  Element* pre = GetDocument().QuerySelector("pre");
  Text* text = ToText(pre->FirstChild());

  Range* range = Range::Create(GetDocument(), text, 0, text, 2);
  range->deleteContents(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(text->GetLayoutObject()->IsTextFragment());
}

}  // namespace blink

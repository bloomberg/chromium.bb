// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingStyle.h"

#include "core/css/CSSPropertyValueSet.h"
#include "core/dom/Document.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"

namespace blink {

class EditingStyleTest : public EditingTestBase {};

TEST_F(EditingStyleTest, mergeInlineStyleOfElement) {
  SetBodyContent(
      "<span id=s1 style='--A:var(---B)'>1</span>"
      "<span id=s2 style='float:var(--C)'>2</span>");
  UpdateAllLifecyclePhases();

  EditingStyle* editing_style =
      EditingStyle::Create(ToHTMLElement(GetDocument().getElementById("s2")));
  editing_style->MergeInlineStyleOfElement(
      ToHTMLElement(GetDocument().getElementById("s1")),
      EditingStyle::kOverrideValues);

  EXPECT_FALSE(editing_style->Style()->HasProperty(CSSPropertyFloat))
      << "Don't merge a property with unresolved value";
  EXPECT_EQ("var(---B)",
            editing_style->Style()->GetPropertyValue(AtomicString("--A")))
      << "Keep unresolved value on merging style";
}

}  // namespace blink

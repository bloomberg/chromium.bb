// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/PlainTextRange.h"

#include "core/editing/EphemeralRange.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class PlainTextRangeTest : public EditingTestBase {
 protected:
  Element* InsertHTMLElement(const char* element_code, const char* element_id);
};

TEST_F(PlainTextRangeTest, RangeContainingTableCellBoundary) {
  SetBodyInnerHTML(
      "<table id='sample' contenteditable><tr><td>a</td><td "
      "id='td2'>b</td></tr></table>");
  Element* table = GetElementById("sample");

  PlainTextRange plain_text_range(2, 2);
  const EphemeralRange& range = plain_text_range.CreateRange(*table);
  EXPECT_EQ(
      "<table contenteditable id=\"sample\"><tbody><tr><td>a</td><td "
      "id=\"td2\">|b</td></tr></tbody></table>",
      GetCaretTextFromBody(range.StartPosition()));
  EXPECT_TRUE(range.IsCollapsed());
}

}  // namespace blink

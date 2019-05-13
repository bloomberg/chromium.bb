// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class AbstractInlineTextBoxTest : public testing::WithParamInterface<bool>,
                                  private ScopedLayoutNGForTest,
                                  public RenderingTest {
 public:
  AbstractInlineTextBoxTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, AbstractInlineTextBoxTest, testing::Bool());

TEST_P(AbstractInlineTextBoxTest, GetTextWithTrailingWhiteSpace) {
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div style="width: 10ch"><label id=label>abc: <input></label></div>)HTML");

  const Element& label = *GetDocument().getElementById("label");
  LayoutText& layout_text =
      *ToLayoutText(label.firstChild()->GetLayoutObject());
  scoped_refptr<AbstractInlineTextBox> inline_text_box =
      layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("abc: ", inline_text_box->GetText());
}

}  // namespace blink

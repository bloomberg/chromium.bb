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

TEST_P(AbstractInlineTextBoxTest, GetTextWithCollapsedWhiteSpace) {
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div id="target">abc </div>)HTML");

  const Element& target = *GetDocument().getElementById("target");
  LayoutText& layout_text =
      *ToLayoutText(target.firstChild()->GetLayoutObject());
  scoped_refptr<AbstractInlineTextBox> inline_text_box =
      layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("abc", inline_text_box->GetText());
}

// For DumpAccessibilityTreeTest.AccessibilityInputTextValue/blink
TEST_P(AbstractInlineTextBoxTest, GetTextWithLineBreakAtCollapsedWhiteSpace) {
  // Line break at space between <label> and <input>.
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div style="width: 10ch"><label id=label>abc:</label> <input></div>)HTML");

  const Element& label = *GetDocument().getElementById("label");
  LayoutText& layout_text =
      *ToLayoutText(label.firstChild()->GetLayoutObject());
  scoped_refptr<AbstractInlineTextBox> inline_text_box =
      layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("abc:", inline_text_box->GetText());
}

// For "web_tests/accessibility/inline-text-change-style.html"
TEST_P(AbstractInlineTextBoxTest,
       GetTextWithLineBreakAtMiddleCollapsedWhiteSpace) {
  // Line break at a space after "012".
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div id="target" style="width: 0ch">012 345</div>)HTML");

  const Element& target = *GetDocument().getElementById("target");
  LayoutText& layout_text =
      *ToLayoutText(target.firstChild()->GetLayoutObject());
  scoped_refptr<AbstractInlineTextBox> inline_text_box =
      layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("012 ", inline_text_box->GetText());
}

// DumpAccessibilityTreeTest.AccessibilitySpanLineBreak/blink
TEST_P(AbstractInlineTextBoxTest,
       GetTextWithLineBreakAtSpanCollapsedWhiteSpace) {
  // Line break at a space in <span>.
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <p id="t1" style="width: 0ch">012<span id="t2"> </span>345</p>)HTML");

  const Element& target1 = *GetDocument().getElementById("t1");
  LayoutText& layout_text1 =
      *ToLayoutText(target1.firstChild()->GetLayoutObject());
  scoped_refptr<AbstractInlineTextBox> inline_text_box1 =
      layout_text1.FirstAbstractInlineTextBox();

  EXPECT_EQ("012", inline_text_box1->GetText());

  const Element& target2 = *GetDocument().getElementById("t2");
  LayoutText& layout_text2 =
      *ToLayoutText(target2.firstChild()->GetLayoutObject());
  scoped_refptr<AbstractInlineTextBox> inline_text_box2 =
      layout_text2.FirstAbstractInlineTextBox();

  EXPECT_FALSE(inline_text_box2) << "We don't have inline box when <span> "
                                    "contains only collapsed white spaces.";
}

// For DumpAccessibilityTreeTest.AccessibilityInputTypes/blink
TEST_P(AbstractInlineTextBoxTest, GetTextWithLineBreakAtTrailingWhiteSpace) {
  // Line break at a space of "abc: ".
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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/cached_text_input_info.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

class CachedTextInputInfoTest : public EditingTestBase {
 protected:
  CachedTextInputInfo& GetCachedTextInputInfo() {
    return GetInputMethodController().GetCachedTextInputInfoForTesting();
  }

  InputMethodController& GetInputMethodController() {
    return GetFrame().GetInputMethodController();
  }
};

TEST_F(CachedTextInputInfoTest, Basic) {
  GetFrame().Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable id=\"sample\">a|b</div>"));
  const Element& sample = *GetElementById("sample");

  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("ab", GetCachedTextInputInfo().GetText());

  To<Text>(sample.firstChild())->appendData("X");
  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abX", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1194349
TEST_F(CachedTextInputInfoTest, PlaceholderBRInTextArea) {
  SetBodyContent("<textarea id=target>abc\n</textarea>");
  auto& target = *To<TextControlElement>(GetElementById("target"));

  // Inner editor is <div>abc<br></div>.
  GetFrame().Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position::LastPositionInNode(*target.InnerEditorElement()))
          .Build());

  EXPECT_EQ(PlainTextRange(4, 4),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abc\n", GetCachedTextInputInfo().GetText())
      << "We should not emit a newline for placeholder <br>";
}

// http://crbug.com/1197801
TEST_F(CachedTextInputInfoTest, PlaceholderBROnlyInTextArea) {
  SetBodyContent("<textarea id=target></textarea>");
  auto& target = *To<TextControlElement>(GetElementById("target"));
  target.focus();
  GetDocument().execCommand("insertparagraph", false, "", ASSERT_NO_EXCEPTION);
  GetDocument().execCommand("delete", false, "", ASSERT_NO_EXCEPTION);

  // Inner editor is <div><br></div>.
  GetFrame().Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position::LastPositionInNode(*target.InnerEditorElement()))
          .Build());

  EXPECT_EQ(PlainTextRange(0, 0),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("", GetCachedTextInputInfo().GetText());
}

TEST_F(CachedTextInputInfoTest, RelayoutBoundary) {
  InsertStyleElement(
      "#sample { contain: strict; width: 100px; height: 100px; }");
  GetFrame().Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<div contenteditable><div id=\"sample\">^a|b</div>"));
  const Element& sample = *GetElementById("sample");
  ASSERT_TRUE(sample.GetLayoutObject()->IsRelayoutBoundary());

  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("ab", GetCachedTextInputInfo().GetText());

  To<Text>(sample.firstChild())->appendData("X");
  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abX", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1228635
TEST_F(CachedTextInputInfoTest, VisibilityHiddenToVisible) {
  GetFrame().Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<div contenteditable id=sample>"
      "<b id=target style='visibility: hidden'>A</b><b>^Z|</b></div>"));

  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("Z", GetCachedTextInputInfo().GetText())
      << "Texts within visibility:hidden are excluded";

  Element& target = *GetElementById("target");
  target.style()->setProperty(GetDocument().GetExecutionContext(), "visibility",
                              "visible", "", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(PlainTextRange(1, 2),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("AZ", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1228635
TEST_F(CachedTextInputInfoTest, VisibilityVisibleToHidden) {
  GetFrame().Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<div contenteditable id=sample>"
      "<b id=target style='visibility: visible'>A</b><b>^Z|</b></div>"));

  EXPECT_EQ(PlainTextRange(1, 2),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("AZ", GetCachedTextInputInfo().GetText());

  Element& target = *GetElementById("target");
  target.style()->setProperty(GetDocument().GetExecutionContext(), "visibility",
                              "hidden", "", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("Z", GetCachedTextInputInfo().GetText())
      << "Texts within visibility:hidden are excluded";
}

}  // namespace blink

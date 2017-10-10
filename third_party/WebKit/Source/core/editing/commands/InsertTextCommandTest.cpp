// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertTextCommand.h"

#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class InsertTextCommandTest : public EditingTestBase {};

// http://crbug.com/714311
TEST_F(InsertTextCommandTest, WithTypingStyle) {
  SetBodyContent("<div contenteditable=true><option id=sample></option></div>");
  Element* const sample = GetDocument().getElementById("sample");
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(sample, 0)).Build());
  // Register typing style to make |InsertTextCommand| to attempt to apply
  // style to inserted text.
  GetDocument().execCommand("fontSizeDelta", false, "+3", ASSERT_NO_EXCEPTION);
  CompositeEditCommand* const command =
      InsertTextCommand::Create(GetDocument(), "x");
  command->Apply();

  EXPECT_EQ(
      "<div contenteditable=\"true\"><option id=\"sample\">x</option></div>",
      GetDocument().body()->InnerHTMLAsString())
      << "Content of OPTION is distributed into shadow node as text"
         "without applying typing style.";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertChar) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "B", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<p contenteditable><span>\taB|c</span></p>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()))
      << "We should not split Text node";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertCharToWhiteSpacePre) {
  Selection().SetSelection(SetSelectionTextToBody(
      "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "B", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable>"
      "<span style=\"white-space:pre\">\ta</span>"
      "B|"
      "<span style=\"white-space:pre\">c</span>"
      "</p>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()))
      << "This is a just record current behavior. We should not split SPAN.";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertSpace) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "  ", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<p contenteditable><span>\ta\xC2\xA0 |c</span></p>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()))
      << "We should insert U+0020 without splitting SPAN";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertSpaceToWhiteSpacePre) {
  Selection().SetSelection(SetSelectionTextToBody(
      "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "  ", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable>"
      "<span style=\"white-space:pre\">\ta</span>"
      "\xC2\xA0\xC2\xA0|"
      "<span style=\"white-space:pre\">c</span></p>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()))
      << "We should insert U+0020 without splitting SPAN";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertTab) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable>"
      "<span>\ta<span style=\"white-space:pre\">\t|</span>c</span>"
      "</p>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertTabToWhiteSpacePre) {
  Selection().SetSelection(SetSelectionTextToBody(
      "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable><span style=\"white-space:pre\">\ta\t|c</span></p>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/752860
TEST_F(InsertTextCommandTest, WhitespaceFixupBeforeParagraph) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux ^bar|<p>baz</p>"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space after "qux" should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux^ bar|<p>baz</p>"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux^bar| <p>baz</p>"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space after "bar" was already being collapsed before the edit. It
  // should not have been converted to a no-break space.
  EXPECT_EQ("<div contenteditable>qux|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux^bar |<p>baz</p>"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux\t^bar|<p>baz</p>"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The tab should have been converted to a no-break space (U+00A0) to prevent
  // it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

TEST_F(InsertTextCommandTest, WhitespaceFixupAfterParagraph) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar| qux"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space before "qux" should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>|\xC2\xA0qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar |qux"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>\xC2\xA0|qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p> ^bar|qux"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space before "bar" was already being collapsed before the edit. It
  // should not have been converted to a no-break space.
  EXPECT_EQ("<div contenteditable><p>baz</p>|qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^ bar|qux"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>\xC2\xA0|qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar|\tqux"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The tab should have been converted to a no-break space (U+00A0) to prevent
  // it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>|\xC2\xA0qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

}  // namespace blink

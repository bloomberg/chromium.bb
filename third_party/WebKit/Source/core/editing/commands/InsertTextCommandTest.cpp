// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertTextCommand.h"

#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/editing/testing/SelectionSample.h"

namespace blink {

class InsertTextCommandTest : public EditingTestBase {};

// http://crbug.com/714311
TEST_F(InsertTextCommandTest, WithTypingStyle) {
  SetBodyContent("<div contenteditable=true><option id=sample></option></div>");
  Element* const sample = GetDocument().getElementById("sample");
  Selection().SetSelectionAndEndTyping(
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
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "B", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<p contenteditable><span>\taB|c</span></p>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()))
      << "We should not split Text node";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertCharToWhiteSpacePre) {
  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
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
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "  ", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<p contenteditable><span>\ta\xC2\xA0 |c</span></p>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()))
      << "We should insert U+0020 without splitting SPAN";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertSpaceToWhiteSpacePre) {
  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
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
  Selection().SetSelectionAndEndTyping(
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
  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"));
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable><span style=\"white-space:pre\">\ta\t|c</span></p>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/752860
TEST_F(InsertTextCommandTest, WhitespaceFixupBeforeParagraph) {
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable>qux ^bar|<p>baz</p>"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space after "qux" should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable>qux^ bar|<p>baz</p>"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable>qux^bar| <p>baz</p>"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space after "bar" was already being collapsed before the edit. It
  // should not have been converted to a no-break space.
  EXPECT_EQ("<div contenteditable>qux|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable>qux^bar |<p>baz</p>"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable>qux\t^bar|<p>baz</p>"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The tab should have been converted to a no-break space (U+00A0) to prevent
  // it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

TEST_F(InsertTextCommandTest, WhitespaceFixupAfterParagraph) {
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar| qux"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space before "qux" should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>|\xC2\xA0qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar |qux"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>\xC2\xA0|qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable><p>baz</p> ^bar|qux"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space before "bar" was already being collapsed before the edit. It
  // should not have been converted to a no-break space.
  EXPECT_EQ("<div contenteditable><p>baz</p>|qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^ bar|qux"));
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>\xC2\xA0|qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));

  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar|\tqux"));
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The tab should have been converted to a no-break space (U+00A0) to prevent
  // it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>|\xC2\xA0qux</div>",
            GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/779376
TEST_F(InsertTextCommandTest, NoVisibleSelectionAfterDeletingSelection) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  InsertStyleElement(
      "ruby {display: inline-block; height: 100%}"
      "navi {float: left}");
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable>"
                             "  <ruby><strike>"
                             "    <navi></navi>"
                             "    <rtc>^&#xbbc3;&#xff17;&#x8e99;&#x1550;</rtc>"
                             "  </strike></ruby>"
                             "  <hr>|"
                             "</div>"));
  // Shouldn't crash inside
  GetDocument().execCommand("insertText", false, "x", ASSERT_NO_EXCEPTION);
  // This is only for recording the current behavior, which can be changed.
  EXPECT_EQ(
      "<div contenteditable>"
      "  <ruby><strike>"
      "    <navi></navi>"
      "    ^</strike></ruby>"
      "|</div>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/778901
TEST_F(InsertTextCommandTest, CheckTabSpanElementNoCrash) {
  InsertStyleElement(
      "head {-webkit-text-stroke-color: black; display: list-item;}");
  Element* head = GetDocument().QuerySelector("head");
  Element* style = GetDocument().QuerySelector("style");
  Element* body = GetDocument().body();
  body->parentNode()->appendChild(style);
  GetDocument().setDesignMode("on");

  Selection().SetSelectionAndEndTyping(SelectionInDOMTree::Builder()
                                           .Collapse(Position(head, 0))
                                           .Extend(Position(body, 0))
                                           .Build());

  // Shouldn't crash inside
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);

  // This only records the current behavior, which is not necessarily correct.
  EXPECT_EQ(
      "<body><span style=\"white-space:pre\">\t|</span></body>"
      "<style>"
      "head {-webkit-text-stroke-color: black; display: list-item;}"
      "</style>",
      SelectionSample::GetSelectionText(*GetDocument().documentElement(),
                                        Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/792548
TEST_F(InsertTextCommandTest, AnchorElementWithBlockCrash) {
  GetDocument().setDesignMode("on");
  SetBodyContent("<a href=\"www\" style=\"display:block\">");
  // We need the below DOM with selection.
  // <a href=\"www\" style=\"display:block\">
  //   <a href=\"www\" style=\"display: inline !important;\">
  //   <i>^home|</i>
  //   </a>
  // </a>
  // Since the HTML parser rejects it as there are nested <a> elements.
  // We are contructing the remaining DOM manually.
  Element* const anchor = GetDocument().QuerySelector("a");
  Element* nested_anchor = GetDocument().createElement("a");
  Element* iElement = GetDocument().createElement("i");

  nested_anchor->setAttribute("href", "www");
  iElement->SetInnerHTMLFromString("home");

  anchor->AppendChild(nested_anchor);
  nested_anchor->AppendChild(iElement);

  Node* const iElement_text_node = iElement->firstChild();
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(iElement_text_node, 0),
                            Position(iElement_text_node, 4))
          .Build());
  // Crash happens here with when '\n' is inserted.
  GetDocument().execCommand("inserttext", false, "a\n", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<i style=\"display: block;\">"
      "<a href=\"www\" style=\"display: block;\">a</a>"
      "</i>|",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

}  // namespace blink

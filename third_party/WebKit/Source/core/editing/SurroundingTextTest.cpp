// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SurroundingText.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "core/html/HTMLElement.h"
#include "core/html/TextControlElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SurroundingTextTest : public ::testing::Test {
 protected:
  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }
  void SetHTML(const String&);
  VisibleSelection Select(int offset) { return Select(offset, offset); }
  VisibleSelection Select(int start, int end);

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void SurroundingTextTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

void SurroundingTextTest::SetHTML(const String& content) {
  GetDocument().body()->setInnerHTML(content);
  GetDocument().UpdateStyleAndLayout();
}

VisibleSelection SurroundingTextTest::Select(int start, int end) {
  Element* element = GetDocument().getElementById("selection");
  return CreateVisibleSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(ToText(element->firstChild()), start))
          .Extend(Position(ToText(element->firstChild()), end))
          .Build());
}

TEST_F(SurroundingTextTest, BasicCaretSelection) {
  SetHTML(String("<p id='selection'>foo bar</p>"));

  {
    VisibleSelection selection = Select(0);
    SurroundingText surrounding_text(selection.Start(), 1);

    EXPECT_EQ("f", surrounding_text.Content());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(0u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0);
    SurroundingText surrounding_text(selection.Start(), 5);

    // maxlength/2 is used on the left and right.
    EXPECT_EQ("foo", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0);
    SurroundingText surrounding_text(selection.Start(), 42);

    EXPECT_EQ("foo bar", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(7);
    SurroundingText surrounding_text(selection.Start(), 42);

    EXPECT_EQ("foo bar", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(8u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(6);
    SurroundingText surrounding_text(selection.Start(), 2);

    EXPECT_EQ("ar", surrounding_text.Content());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(6);
    SurroundingText surrounding_text(selection.Start(), 42);

    EXPECT_EQ("foo bar", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(7u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(7u, surrounding_text.EndOffsetInContent());
  }
}

TEST_F(SurroundingTextTest, BasicRangeSelection) {
  SetHTML(String("<p id='selection'>Lorem ipsum dolor sit amet</p>"));

  {
    VisibleSelection selection = Select(0, 5);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 1);

    EXPECT_EQ("Lorem ", surrounding_text.Content());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(5u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0, 5);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 5);

    EXPECT_EQ("Lorem ip", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0, 5);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 42);

    EXPECT_EQ("Lorem ipsum dolor sit amet",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(6, 11);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 2);

    EXPECT_EQ(" ipsum ", surrounding_text.Content());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(6, 11);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 42);

    EXPECT_EQ("Lorem ipsum dolor sit amet",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(7u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(12u, surrounding_text.EndOffsetInContent());
  }

  {
    // Last word.
    VisibleSelection selection = Select(22, 26);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 8);

    EXPECT_EQ("sit amet", surrounding_text.Content());
    EXPECT_EQ(4u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInContent());
  }
}

TEST_F(SurroundingTextTest, TreeCaretSelection) {
  SetHTML(
      String("<div>This is outside of <p id='selection'>foo bar</p> the "
             "selected node</div>"));

  {
    VisibleSelection selection = Select(0);
    SurroundingText surrounding_text(selection.Start(), 1);

    EXPECT_EQ("f", surrounding_text.Content());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(0u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0);
    SurroundingText surrounding_text(selection.Start(), 5);

    EXPECT_EQ("foo", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0);
    SurroundingText surrounding_text(selection.Start(), 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(20u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(20u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(6);
    SurroundingText surrounding_text(selection.Start(), 2);

    EXPECT_EQ("ar", surrounding_text.Content());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(6);
    SurroundingText surrounding_text(selection.Start(), 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(26u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(26u, surrounding_text.EndOffsetInContent());
  }
}

TEST_F(SurroundingTextTest, TreeRangeSelection) {
  SetHTML(
      String("<div>This is outside of <p id='selection'>foo bar</p> the "
             "selected node</div>"));

  {
    VisibleSelection selection = Select(0, 1);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 1);

    EXPECT_EQ("fo", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0, 3);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 12);

    EXPECT_EQ("e of foo bar", surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(5u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0, 3);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(20u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(23u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(4, 7);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 12);

    EXPECT_EQ("foo bar the se",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(5u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInContent());
  }

  {
    VisibleSelection selection = Select(0, 7);
    SurroundingText surrounding_text(
        *CreateRange(FirstEphemeralRangeOf(selection)), 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              surrounding_text.Content().SimplifyWhiteSpace());
    EXPECT_EQ(20u, surrounding_text.StartOffsetInContent());
    EXPECT_EQ(27u, surrounding_text.EndOffsetInContent());
  }
}

TEST_F(SurroundingTextTest, TextAreaSelection) {
  SetHTML(
      String("<p>First paragraph</p>"
             "<textarea id='selection'>abc def ghi</textarea>"
             "<p>Second paragraph</p>"));

  TextControlElement* text_ctrl =
      (TextControlElement*)GetDocument().getElementById("selection");

  text_ctrl->SetSelectionRange(4, 7);
  VisibleSelection selection = CreateVisibleSelection(text_ctrl->Selection());

  SurroundingText surrounding_text(
      *CreateRange(FirstEphemeralRangeOf(selection)), 20);

  EXPECT_EQ("abc def ghi", surrounding_text.Content().SimplifyWhiteSpace());
  EXPECT_EQ(4u, surrounding_text.StartOffsetInContent());
  EXPECT_EQ(7u, surrounding_text.EndOffsetInContent());
}

TEST_F(SurroundingTextTest, EmptyInputElementWithChild) {
  SetHTML(String("<input type=\"text\" id=\"input_name\"/>"));

  TextControlElement* input_element =
      (TextControlElement*)GetDocument().getElementById("input_name");
  input_element->SetInnerEditorValue("John Smith");
  GetDocument().UpdateStyleAndLayout();

  // BODY
  //   INPUT
  //     #shadow-root
  // *      DIV id="inner-editor" (editable)
  //          #text "John Smith"

  const Element* inner_editor = input_element->InnerEditorElement();
  const Position start = Position(inner_editor, 0);
  const Position end = Position(inner_editor, 0);

  // Surrounding text should not crash. See http://crbug.com/758438.
  SurroundingText surrounding_text(*CreateRange(EphemeralRange(start, end)), 8);
  EXPECT_TRUE(surrounding_text.Content().IsEmpty());
}

}  // namespace blink

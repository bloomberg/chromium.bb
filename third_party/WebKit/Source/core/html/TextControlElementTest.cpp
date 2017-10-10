// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/TextControlElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/HTMLTextAreaElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TextControlElementTest : public ::testing::Test {
 protected:
  void SetUp() override;

  DummyPageHolder& Page() const { return *dummy_page_holder_; }
  Document& GetDocument() const { return *document_; }
  TextControlElement& TextControl() const { return *text_control_; }
  HTMLInputElement& Input() const { return *input_; }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;

  Persistent<Document> document_;
  Persistent<TextControlElement> text_control_;
  Persistent<HTMLInputElement> input_;
};

void TextControlElementTest::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  dummy_page_holder_ =
      DummyPageHolder::Create(IntSize(800, 600), &page_clients);

  document_ = &dummy_page_holder_->GetDocument();
  document_->documentElement()->SetInnerHTMLFromString(
      "<body><textarea id=textarea></textarea><input id=input /></body>");
  document_->View()->UpdateAllLifecyclePhases();
  text_control_ = ToTextControlElement(document_->getElementById("textarea"));
  text_control_->focus();
  input_ = ToHTMLInputElement(document_->getElementById("input"));
}

TEST_F(TextControlElementTest, SetSelectionRange) {
  EXPECT_EQ(0u, TextControl().selectionStart());
  EXPECT_EQ(0u, TextControl().selectionEnd());

  TextControl().SetInnerEditorValue("Hello, text form.");
  EXPECT_EQ(0u, TextControl().selectionStart());
  EXPECT_EQ(0u, TextControl().selectionEnd());

  TextControl().SetSelectionRange(1, 3);
  EXPECT_EQ(1u, TextControl().selectionStart());
  EXPECT_EQ(3u, TextControl().selectionEnd());
}

TEST_F(TextControlElementTest, SetSelectionRangeDoesNotCauseLayout) {
  Input().focus();
  Input().setValue("Hello, input form.");
  Input().SetSelectionRange(1, 1);

  // Force layout if document().updateStyleAndLayoutIgnorePendingStylesheets()
  // is called.
  GetDocument().body()->AppendChild(GetDocument().createTextNode("foo"));
  const int start_layout_count = Page().GetFrameView().LayoutCount();
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  Input().SetSelectionRange(2, 2);
  EXPECT_EQ(start_layout_count, Page().GetFrameView().LayoutCount());
}

TEST_F(TextControlElementTest, IndexForPosition) {
  HTMLInputElement* input =
      ToHTMLInputElement(GetDocument().getElementById("input"));
  input->setValue("Hello");
  HTMLElement* inner_editor = input->InnerEditorElement();
  EXPECT_EQ(5u, TextControlElement::IndexForPosition(
                    inner_editor,
                    Position(inner_editor, PositionAnchorType::kAfterAnchor)));
}

}  // namespace blink

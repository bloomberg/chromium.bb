// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/TextControlElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

namespace blink {

class TextControlElementTest : public ::testing::Test {
 protected:
  void SetUp() override;

  DummyPageHolder& page() const { return *m_dummyPageHolder; }
  Document& document() const { return *m_document; }
  TextControlElement& textControl() const { return *m_textControl; }
  HTMLInputElement& input() const { return *m_input; }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;

  Persistent<Document> m_document;
  Persistent<TextControlElement> m_textControl;
  Persistent<HTMLInputElement> m_input;
};

void TextControlElementTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

  m_document = &m_dummyPageHolder->document();
  m_document->documentElement()->setInnerHTML(
      "<body><textarea id=textarea></textarea><input id=input /></body>");
  m_document->view()->updateAllLifecyclePhases();
  m_textControl = toTextControlElement(m_document->getElementById("textarea"));
  m_textControl->focus();
  m_input = toHTMLInputElement(m_document->getElementById("input"));
}

TEST_F(TextControlElementTest, SetSelectionRange) {
  EXPECT_EQ(0u, textControl().selectionStart());
  EXPECT_EQ(0u, textControl().selectionEnd());

  textControl().setInnerEditorValue("Hello, text form.");
  EXPECT_EQ(0u, textControl().selectionStart());
  EXPECT_EQ(0u, textControl().selectionEnd());

  textControl().setSelectionRange(1, 3);
  EXPECT_EQ(1u, textControl().selectionStart());
  EXPECT_EQ(3u, textControl().selectionEnd());
}

TEST_F(TextControlElementTest, SetSelectionRangeDoesNotCauseLayout) {
  input().focus();
  input().setValue("Hello, input form.");
  input().setSelectionRange(1, 1);

  // Force layout if document().updateStyleAndLayoutIgnorePendingStylesheets()
  // is called.
  document().body()->appendChild(document().createTextNode("foo"));
  const int startLayoutCount = page().frameView().layoutCount();
  EXPECT_TRUE(document().needsLayoutTreeUpdate());
  input().setSelectionRange(2, 2);
  EXPECT_EQ(startLayoutCount, page().frameView().layoutCount());
}

TEST_F(TextControlElementTest, IndexForPosition) {
  HTMLInputElement* input =
      toHTMLInputElement(document().getElementById("input"));
  input->setValue("Hello");
  HTMLElement* innerEditor = input->innerEditorElement();
  EXPECT_EQ(5u, TextControlElement::indexForPosition(
                    innerEditor,
                    Position(innerEditor, PositionAnchorType::AfterAnchor)));
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLFormControlElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/page/ValidationMessageClient.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
class MockValidationMessageClient
    : public GarbageCollectedFinalized<MockValidationMessageClient>,
      public ValidationMessageClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockValidationMessageClient);

 public:
  void ShowValidationMessage(const Element& anchor,
                             const String&,
                             TextDirection,
                             const String&,
                             TextDirection) override {
    anchor_ = anchor;
  }

  void HideValidationMessage(const Element& anchor) override {
    if (anchor_ == &anchor)
      anchor_ = nullptr;
  }

  bool IsValidationMessageVisible(const Element& anchor) override {
    return anchor_ == &anchor;
  }

  void WillUnloadDocument(const Document&) override {}
  void DocumentDetached(const Document&) override {}
  void WillBeDestroyed() override {}
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(anchor_);
    ValidationMessageClient::Trace(visitor);
  }

 private:
  Member<const Element> anchor_;
};
}

class HTMLFormControlElementTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Page& GetPage() const { return dummy_page_holder_->GetPage(); }
  Document& GetDocument() const { return *document_; }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Document> document_;
};

void HTMLFormControlElementTest::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  dummy_page_holder_ =
      DummyPageHolder::Create(IntSize(800, 600), &page_clients);

  document_ = &dummy_page_holder_->GetDocument();
  document_->SetMimeType("text/html");
}

TEST_F(HTMLFormControlElementTest, customValidationMessageTextDirection) {
  GetDocument().documentElement()->setInnerHTML(
      "<body><input pattern='abc' value='def' id=input></body>",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  HTMLInputElement* input =
      toHTMLInputElement(GetDocument().getElementById("input"));
  input->setCustomValidity(
      String::FromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));
  input->setAttribute(
      HTMLNames::titleAttr,
      AtomicString::FromUTF8("\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x89"));

  String message = input->validationMessage().StripWhiteSpace();
  String sub_message = input->ValidationSubMessage().StripWhiteSpace();
  TextDirection message_dir = TextDirection::kRtl;
  TextDirection sub_message_dir = TextDirection::kLtr;

  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kRtl, message_dir);
  EXPECT_EQ(TextDirection::kLtr, sub_message_dir);

  input->GetLayoutObject()->MutableStyleRef().SetDirection(TextDirection::kRtl);
  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kRtl, message_dir);
  EXPECT_EQ(TextDirection::kLtr, sub_message_dir);

  input->setCustomValidity(String::FromUTF8("Main message."));
  message = input->validationMessage().StripWhiteSpace();
  sub_message = input->ValidationSubMessage().StripWhiteSpace();
  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kLtr, message_dir);
  EXPECT_EQ(TextDirection::kLtr, sub_message_dir);

  input->setCustomValidity(String());
  message = input->validationMessage().StripWhiteSpace();
  sub_message = input->ValidationSubMessage().StripWhiteSpace();
  input->FindCustomValidationMessageTextDirection(message, message_dir,
                                                  sub_message, sub_message_dir);
  EXPECT_EQ(TextDirection::kLtr, message_dir);
  EXPECT_EQ(TextDirection::kRtl, sub_message_dir);
}

TEST_F(HTMLFormControlElementTest, UpdateValidationMessageSkippedIfPrinting) {
  GetDocument().documentElement()->setInnerHTML(
      "<body><input required id=input></body>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ValidationMessageClient* validation_message_client =
      new MockValidationMessageClient();
  GetPage().SetValidationMessageClient(validation_message_client);
  Page::OrdinaryPages().insert(&GetPage());

  HTMLInputElement* input =
      toHTMLInputElement(GetDocument().getElementById("input"));
  ScopedPageSuspender suspender;  // print() suspends the page.
  input->reportValidity();
  EXPECT_FALSE(validation_message_client->IsValidationMessageVisible(*input));
}

}  // namespace blink

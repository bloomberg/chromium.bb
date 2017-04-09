// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/document_metadata/CopylessPasteExtractor.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

class CopylessPasteExtractorTest : public ::testing::Test {
 public:
  CopylessPasteExtractorTest()
      : content_(
            "\n"
            "\n"
            "{\"@type\": \"NewsArticle\","
            "\"headline\": \"Special characters for ya >_<;\"\n"
            "}\n"
            "\n") {}

 protected:
  void SetUp() override;

  void TearDown() override { ThreadState::Current()->CollectAllGarbage(); }

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }

  String Extract() { return CopylessPasteExtractor::Extract(GetDocument()); }

  void SetHtmlInnerHTML(const String&);

  String content_;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void CopylessPasteExtractorTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

void CopylessPasteExtractorTest::SetHtmlInnerHTML(const String& html_content) {
  GetDocument().documentElement()->setInnerHTML((html_content));
}

TEST_F(CopylessPasteExtractorTest, empty) {
  String extracted = Extract();
  String expected = "[]";
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, basic) {
  SetHtmlInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">" +
      content_ +
      "</script>"
      "</body>");

  String extracted = Extract();
  String expected = "[" + content_ + "]";
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, header) {
  SetHtmlInnerHTML(
      "<head>"
      "<script type=\"application/ld+json\">" +
      content_ +
      "</script>"
      "</head>");

  String extracted = Extract();
  String expected = "[" + content_ + "]";
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, multiple) {
  SetHtmlInnerHTML(
      "<head>"
      "<script type=\"application/ld+json\">" +
      content_ +
      "</script>"
      "</head>"
      "<body>"
      "<script type=\"application/ld+json\">" +
      content_ +
      "</script>"
      "<script type=\"application/ld+json\">" +
      content_ +
      "</script>"
      "</body>");

  String extracted = Extract();
  String expected = "[" + content_ + "," + content_ + "," + content_ + "]";
  EXPECT_EQ(expected, extracted);
}

}  // namespace

}  // namespace blink

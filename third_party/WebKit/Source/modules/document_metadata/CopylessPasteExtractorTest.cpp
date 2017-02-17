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
      : m_content(
            "\n"
            "\n"
            "{\"@type\": \"NewsArticle\","
            "\"headline\": \"Special characters for ya >_<;\"\n"
            "}\n"
            "\n") {}

 protected:
  void SetUp() override;

  void TearDown() override { ThreadState::current()->collectAllGarbage(); }

  Document& document() const { return m_dummyPageHolder->document(); }

  String extract() { return CopylessPasteExtractor::extract(document()); }

  void setHtmlInnerHTML(const String&);

  String m_content;

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void CopylessPasteExtractorTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

void CopylessPasteExtractorTest::setHtmlInnerHTML(const String& htmlContent) {
  document().documentElement()->setInnerHTML((htmlContent));
}

TEST_F(CopylessPasteExtractorTest, empty) {
  String extracted = extract();
  String expected = "[]";
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, basic) {
  setHtmlInnerHTML(
      "<body>"
      "<script type=\"application/ld+json\">" +
      m_content +
      "</script>"
      "</body>");

  String extracted = extract();
  String expected = "[" + m_content + "]";
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, header) {
  setHtmlInnerHTML(
      "<head>"
      "<script type=\"application/ld+json\">" +
      m_content +
      "</script>"
      "</head>");

  String extracted = extract();
  String expected = "[" + m_content + "]";
  EXPECT_EQ(expected, extracted);
}

TEST_F(CopylessPasteExtractorTest, multiple) {
  setHtmlInnerHTML(
      "<head>"
      "<script type=\"application/ld+json\">" +
      m_content +
      "</script>"
      "</head>"
      "<body>"
      "<script type=\"application/ld+json\">" +
      m_content +
      "</script>"
      "<script type=\"application/ld+json\">" +
      m_content +
      "</script>"
      "</body>");

  String extracted = extract();
  String expected = "[" + m_content + "," + m_content + "," + m_content + "]";
  EXPECT_EQ(expected, extracted);
}

}  // namespace

}  // namespace blink

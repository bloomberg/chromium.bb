// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLLinkElement.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLLinkElementTest : public ::testing::Test {
 protected:
  void SetUp() override;
  Document& document() const { return m_dummyPageHolder->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void HTMLLinkElementTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

// This tests that we should ignore empty string value
// in href attribute value of the link element.
TEST_F(HTMLLinkElementTest, EmptyHrefAttribute) {
  document().documentElement()->setInnerHTML(
      "<head>"
      "<link rel=\"icon\" type=\"image/ico\" href=\"\" />"
      "</head>");
  HTMLLinkElement* linkElement =
      toElement<HTMLLinkElement>(document().head()->firstChild());
  EXPECT_EQ(KURL(), linkElement->href());
}

}  // namespace blink

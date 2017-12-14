// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLLinkElement.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/testing/PageTestBase.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLLinkElementTest : public PageTestBase {};

// This tests that we should ignore empty string value
// in href attribute value of the link element.
TEST_F(HTMLLinkElementTest, EmptyHrefAttribute) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<head>"
      "<link rel=\"icon\" type=\"image/ico\" href=\"\" />"
      "</head>");
  HTMLLinkElement* link_element =
      ToElement<HTMLLinkElement>(GetDocument().head()->firstChild());
  EXPECT_EQ(NullURL(), link_element->Href());
}

}  // namespace blink

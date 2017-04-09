// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElement.h"

#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Test setting feature policy via the Element attribute (HTML codepath).
TEST(HTMLIFrameElementTest, SetAllowAttribute) {
  Document* document = Document::Create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);

  iframe->setAttribute(HTMLNames::allowAttr, "fullscreen");
  EXPECT_EQ("fullscreen", iframe->allow()->value());
  iframe->setAttribute(HTMLNames::allowAttr, "fullscreen vibrate");
  EXPECT_EQ("fullscreen vibrate", iframe->allow()->value());
}

// Test setting feature policy via the DOMTokenList (JS codepath).
TEST(HTMLIFrameElementTest, SetAllowAttributeJS) {
  Document* document = Document::Create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);

  iframe->allow()->setValue("fullscreen");
  EXPECT_EQ("fullscreen", iframe->getAttribute(HTMLNames::allowAttr));
}

}  // namespace blink

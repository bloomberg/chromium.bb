// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLOutputElement.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLLinkElementSizesAttributeTest,
     setHTMLForProperty_updatesForAttribute) {
  Document* document = Document::CreateForTest();
  HTMLOutputElement* element = HTMLOutputElement::Create(*document);
  EXPECT_EQ(g_null_atom, element->getAttribute(HTMLNames::forAttr));
  element->htmlFor()->setValue("  strawberry ");
  EXPECT_EQ("  strawberry ", element->getAttribute(HTMLNames::forAttr));
}

TEST(HTMLOutputElementTest, setForAttribute_updatesHTMLForPropertyValue) {
  Document* document = Document::CreateForTest();
  HTMLOutputElement* element = HTMLOutputElement::Create(*document);
  DOMTokenList* for_tokens = element->htmlFor();
  EXPECT_EQ(g_null_atom, for_tokens->value());
  element->setAttribute(HTMLNames::forAttr, "orange grape");
  EXPECT_EQ("orange grape", for_tokens->value());
}

}  // namespace blink

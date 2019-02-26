// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_output_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

TEST(HTMLLinkElementSizesAttributeTest,
     setHTMLForProperty_updatesForAttribute) {
  Document* document = Document::CreateForTest();
  HTMLOutputElement* element = HTMLOutputElement::Create(*document);
  EXPECT_EQ(g_null_atom, element->getAttribute(html_names::kForAttr));
  element->htmlFor()->setValue("  strawberry ");
  EXPECT_EQ("  strawberry ", element->getAttribute(html_names::kForAttr));
}

TEST(HTMLOutputElementTest, setForAttribute_updatesHTMLForPropertyValue) {
  Document* document = Document::CreateForTest();
  HTMLOutputElement* element = HTMLOutputElement::Create(*document);
  DOMTokenList* for_tokens = element->htmlFor();
  EXPECT_EQ(g_null_atom, for_tokens->value());
  element->setAttribute(html_names::kForAttr, "orange grape");
  EXPECT_EQ("orange grape", for_tokens->value());
}

}  // namespace blink

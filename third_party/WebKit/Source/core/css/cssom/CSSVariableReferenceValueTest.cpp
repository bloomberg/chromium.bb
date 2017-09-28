// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"

#include "core/css/cssom/CSSUnparsedValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSVariableReferenceValueTest, EmptyList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  CSSUnparsedValue* unparsed_value = CSSUnparsedValue::Create(fragments);

  CSSStyleVariableReferenceValue* variable_reference_value =
      CSSStyleVariableReferenceValue::Create("test", unparsed_value);

  EXPECT_EQ(variable_reference_value->variable(), "test");
  EXPECT_EQ(variable_reference_value->fallback(), unparsed_value);
}

TEST(CSSVariableReferenceValueTest, MixedList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.push_back(StringOrCSSVariableReferenceValue::FromString("string"));
  fragments.push_back(
      StringOrCSSVariableReferenceValue::FromCSSVariableReferenceValue(
          CSSStyleVariableReferenceValue::Create(
              "Variable", CSSUnparsedValue::FromString("Fallback"))));
  fragments.push_back(StringOrCSSVariableReferenceValue());

  CSSUnparsedValue* unparsed_value = CSSUnparsedValue::Create(fragments);

  CSSStyleVariableReferenceValue* variable_reference_value =
      CSSStyleVariableReferenceValue::Create("test", unparsed_value);

  EXPECT_EQ(variable_reference_value->variable(), "test");
  EXPECT_EQ(variable_reference_value->fallback(), unparsed_value);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"

#include "core/css/cssom/CSSUnparsedValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

StringOrCSSVariableReferenceValue getStringOrCSSVariableReferenceValue(
    String str) {
  StringOrCSSVariableReferenceValue temp;
  temp.setString(str);
  return temp;
}

StringOrCSSVariableReferenceValue getStringOrCSSVariableReferenceValue(
    CSSStyleVariableReferenceValue* ref) {
  StringOrCSSVariableReferenceValue temp;
  temp.setCSSVariableReferenceValue(ref);
  return temp;
}

CSSUnparsedValue* unparsedValueFromString(String str) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.append(getStringOrCSSVariableReferenceValue(str));
  return CSSUnparsedValue::create(fragments);
}

TEST(CSSVariableReferenceValueTest, EmptyList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;

  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

  CSSStyleVariableReferenceValue* ref =
      CSSStyleVariableReferenceValue::create("test", unparsedValue);

  EXPECT_EQ(ref->variable(), "test");
  EXPECT_EQ(ref->fallback(), unparsedValue);
}

TEST(CSSVariableReferenceValueTest, MixedList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;

  StringOrCSSVariableReferenceValue x =
      getStringOrCSSVariableReferenceValue("Str");
  StringOrCSSVariableReferenceValue y = getStringOrCSSVariableReferenceValue(
      CSSStyleVariableReferenceValue::create(
          "Variable", unparsedValueFromString("Fallback")));
  StringOrCSSVariableReferenceValue z;

  fragments.append(x);
  fragments.append(y);
  fragments.append(z);

  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

  CSSStyleVariableReferenceValue* ref =
      CSSStyleVariableReferenceValue::create("test", unparsedValue);

  EXPECT_EQ(ref->variable(), "test");
  EXPECT_EQ(ref->fallback(), unparsedValue);
}

}  // namespace

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnparsedValue.h"

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

CSSUnparsedValue* UnparsedValueFromCSSVariableReferenceValue(
    CSSStyleVariableReferenceValue* variable_reference_value) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.push_back(
      StringOrCSSVariableReferenceValue::FromCSSVariableReferenceValue(
          variable_reference_value));
  return CSSUnparsedValue::Create(fragments);
}

}  // namespace

TEST(CSSUnparsedValueTest, EmptyList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  CSSUnparsedValue* unparsed_value = CSSUnparsedValue::Create(fragments);

  EXPECT_EQ(unparsed_value->length(), 0UL);
}

TEST(CSSUnparsedValueTest, ListOfStrings) {
  CSSUnparsedValue* unparsed_value = CSSUnparsedValue::FromString("string");

  EXPECT_EQ(unparsed_value->length(), 1UL);

  EXPECT_TRUE(unparsed_value->fragmentAtIndex(0).IsString());
  EXPECT_FALSE(unparsed_value->fragmentAtIndex(0).IsNull());
  EXPECT_FALSE(
      unparsed_value->fragmentAtIndex(0).IsCSSVariableReferenceValue());

  EXPECT_EQ(unparsed_value->fragmentAtIndex(0).GetAsString(), "string");
}

TEST(CSSUnparsedValueTest, ListOfCSSVariableReferenceValues) {
  CSSStyleVariableReferenceValue* variable_reference_value =
      CSSStyleVariableReferenceValue::Create(
          "Ref", CSSUnparsedValue::FromString("string"));

  CSSUnparsedValue* unparsed_value =
      UnparsedValueFromCSSVariableReferenceValue(variable_reference_value);

  EXPECT_EQ(unparsed_value->length(), 1UL);

  EXPECT_FALSE(unparsed_value->fragmentAtIndex(0).IsString());
  EXPECT_FALSE(unparsed_value->fragmentAtIndex(0).IsNull());
  EXPECT_TRUE(unparsed_value->fragmentAtIndex(0).IsCSSVariableReferenceValue());

  EXPECT_EQ(unparsed_value->fragmentAtIndex(0).GetAsCSSVariableReferenceValue(),
            variable_reference_value);
}

TEST(CSSUnparsedValueTest, MixedList) {
  CSSStyleVariableReferenceValue* variable_reference_value =
      CSSStyleVariableReferenceValue::Create(
          "Ref", CSSUnparsedValue::FromString("string"));

  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.push_back(StringOrCSSVariableReferenceValue::FromString("string"));
  fragments.push_back(
      StringOrCSSVariableReferenceValue::FromCSSVariableReferenceValue(
          variable_reference_value));
  fragments.push_back(StringOrCSSVariableReferenceValue());

  CSSUnparsedValue* unparsed_value = CSSUnparsedValue::Create(fragments);

  EXPECT_EQ(unparsed_value->length(), fragments.size());

  EXPECT_TRUE(unparsed_value->fragmentAtIndex(0).IsString());
  EXPECT_FALSE(
      unparsed_value->fragmentAtIndex(0).IsCSSVariableReferenceValue());
  EXPECT_EQ(unparsed_value->fragmentAtIndex(0).GetAsString(), "string");

  EXPECT_TRUE(unparsed_value->fragmentAtIndex(1).IsCSSVariableReferenceValue());
  EXPECT_FALSE(unparsed_value->fragmentAtIndex(1).IsString());
  EXPECT_EQ(unparsed_value->fragmentAtIndex(1).GetAsCSSVariableReferenceValue(),
            variable_reference_value);

  EXPECT_TRUE(unparsed_value->fragmentAtIndex(2).IsNull());
}

}  // namespace blink

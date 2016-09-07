// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnparsedValue.h"

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

StringOrCSSVariableReferenceValue getStringOrCSSVariableReferenceValue(String str)
{
    StringOrCSSVariableReferenceValue temp;
    temp.setString(str);
    return temp;
}

StringOrCSSVariableReferenceValue getStringOrCSSVariableReferenceValue(CSSStyleVariableReferenceValue* ref)
{
    StringOrCSSVariableReferenceValue temp;
    temp.setCSSVariableReferenceValue(ref);
    return temp;
}

CSSUnparsedValue* unparsedValueFromString(String str)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    fragments.append(getStringOrCSSVariableReferenceValue(str));
    return CSSUnparsedValue::create(fragments);
}

CSSUnparsedValue* unparsedValueFromCSSVariableReferenceValue(CSSStyleVariableReferenceValue* ref)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    fragments.append(getStringOrCSSVariableReferenceValue(ref));
    return CSSUnparsedValue::create(fragments);
}

TEST(CSSUnparsedValueTest, EmptyList)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;

    CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

    EXPECT_EQ(unparsedValue->size(), 0UL);
}

TEST(CSSUnparsedValueTest, ListOfString)
{
    CSSUnparsedValue* unparsedValue = unparsedValueFromString("Str");

    EXPECT_EQ(unparsedValue->size(), 1UL);

    EXPECT_TRUE(unparsedValue->fragmentAtIndex(0).isString());
    EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isNull());
    EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isCSSVariableReferenceValue());

    EXPECT_EQ(unparsedValue->fragmentAtIndex(0).getAsString(), "Str");
}

TEST(CSSUnparsedValueTest, ListOfCSSVariableReferenceValue)
{
    CSSStyleVariableReferenceValue* ref = CSSStyleVariableReferenceValue::create("Ref", unparsedValueFromString("Str"));

    CSSUnparsedValue* unparsedValue = unparsedValueFromCSSVariableReferenceValue(ref);

    EXPECT_EQ(unparsedValue->size(), 1UL);

    EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isString());
    EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isNull());
    EXPECT_TRUE(unparsedValue->fragmentAtIndex(0).isCSSVariableReferenceValue());

    EXPECT_EQ(unparsedValue->fragmentAtIndex(0).getAsCSSVariableReferenceValue(), ref);
}

TEST(CSSUnparsedValueTest, MixedContents)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;

    StringOrCSSVariableReferenceValue x = getStringOrCSSVariableReferenceValue("Str");

    CSSStyleVariableReferenceValue* ref = CSSStyleVariableReferenceValue::create("Ref", unparsedValueFromString("Str"));
    StringOrCSSVariableReferenceValue y = getStringOrCSSVariableReferenceValue(ref);

    StringOrCSSVariableReferenceValue z;

    fragments.append(x);
    fragments.append(y);
    fragments.append(z);

    CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

    EXPECT_EQ(unparsedValue->size(), fragments.size());

    EXPECT_TRUE(unparsedValue->fragmentAtIndex(0).isString());
    EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isCSSVariableReferenceValue());
    EXPECT_EQ(unparsedValue->fragmentAtIndex(0).getAsString(), "Str");

    EXPECT_TRUE(unparsedValue->fragmentAtIndex(1).isCSSVariableReferenceValue());
    EXPECT_FALSE(unparsedValue->fragmentAtIndex(1).isString());
    EXPECT_EQ(unparsedValue->fragmentAtIndex(1).getAsCSSVariableReferenceValue(), ref);

    EXPECT_TRUE(unparsedValue->fragmentAtIndex(2).isNull());
}

} // namespace

} // namespace blink

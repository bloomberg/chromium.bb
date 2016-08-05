// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTokenStreamValue.h"

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

CSSTokenStreamValue* tokenStreamValueFromString(String str)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    fragments.append(getStringOrCSSVariableReferenceValue(str));
    return CSSTokenStreamValue::create(fragments);
}

CSSTokenStreamValue* tokenStreamValueFromCSSVariableReferenceValue(CSSStyleVariableReferenceValue* ref)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    fragments.append(getStringOrCSSVariableReferenceValue(ref));
    return CSSTokenStreamValue::create(fragments);
}

TEST(CSSTokenStreamValueTest, EmptyList)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;

    CSSTokenStreamValue* tokenStreamValue = CSSTokenStreamValue::create(fragments);

    EXPECT_EQ(tokenStreamValue->size(), 0UL);
}

TEST(CSSTokenStreamValueTest, ListOfString)
{
    CSSTokenStreamValue* tokenStreamValue = tokenStreamValueFromString("Str");

    EXPECT_EQ(tokenStreamValue->size(), 1UL);

    EXPECT_TRUE(tokenStreamValue->fragmentAtIndex(0).isString());
    EXPECT_FALSE(tokenStreamValue->fragmentAtIndex(0).isNull());
    EXPECT_FALSE(tokenStreamValue->fragmentAtIndex(0).isCSSVariableReferenceValue());

    EXPECT_EQ(tokenStreamValue->fragmentAtIndex(0).getAsString(), "Str");
}

TEST(CSSTokenStreamValueTest, ListOfCSSVariableReferenceValue)
{
    CSSStyleVariableReferenceValue* ref = CSSStyleVariableReferenceValue::create("Ref");

    CSSTokenStreamValue* tokenStreamValue = tokenStreamValueFromCSSVariableReferenceValue(ref);

    EXPECT_EQ(tokenStreamValue->size(), 1UL);

    EXPECT_FALSE(tokenStreamValue->fragmentAtIndex(0).isString());
    EXPECT_FALSE(tokenStreamValue->fragmentAtIndex(0).isNull());
    EXPECT_TRUE(tokenStreamValue->fragmentAtIndex(0).isCSSVariableReferenceValue());

    EXPECT_EQ(tokenStreamValue->fragmentAtIndex(0).getAsCSSVariableReferenceValue(), ref);
}

TEST(CSSTokenStreamValueTest, MixedContents)
{
    HeapVector<StringOrCSSVariableReferenceValue> fragments;

    StringOrCSSVariableReferenceValue x = getStringOrCSSVariableReferenceValue("Str");

    CSSStyleVariableReferenceValue* ref = CSSStyleVariableReferenceValue::create("Ref");
    StringOrCSSVariableReferenceValue y = getStringOrCSSVariableReferenceValue(ref);

    StringOrCSSVariableReferenceValue z;

    fragments.append(x);
    fragments.append(y);
    fragments.append(z);

    CSSTokenStreamValue* tokenStreamValue = CSSTokenStreamValue::create(fragments);

    EXPECT_EQ(tokenStreamValue->size(), fragments.size());

    EXPECT_TRUE(tokenStreamValue->fragmentAtIndex(0).isString());
    EXPECT_FALSE(tokenStreamValue->fragmentAtIndex(0).isCSSVariableReferenceValue());
    EXPECT_EQ(tokenStreamValue->fragmentAtIndex(0).getAsString(), "Str");

    EXPECT_TRUE(tokenStreamValue->fragmentAtIndex(1).isCSSVariableReferenceValue());
    EXPECT_FALSE(tokenStreamValue->fragmentAtIndex(1).isString());
    EXPECT_EQ(tokenStreamValue->fragmentAtIndex(1).getAsCSSVariableReferenceValue(), ref);

    EXPECT_TRUE(tokenStreamValue->fragmentAtIndex(2).isNull());
}

} // namespace

} // namespace blink

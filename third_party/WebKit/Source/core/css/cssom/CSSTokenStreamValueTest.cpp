// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTokenStreamValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

CSSTokenStreamValue* tokenStreamValueFromString(String str)
{
    Vector<String> fragments;
    fragments.append(str);
    return CSSTokenStreamValue::create(fragments);
}

TEST(CSSTokenStreamValueTest, EmptyList)
{
    Vector<String> fragments;

    CSSTokenStreamValue* tokenStreamValue = CSSTokenStreamValue::create(fragments);

    EXPECT_EQ(tokenStreamValue->size(), 0UL);
}

TEST(CSSTokenStreamValueTest, ListOfString)
{
    CSSTokenStreamValue* tokenStreamValue = tokenStreamValueFromString("Str");

    EXPECT_EQ(tokenStreamValue->size(), 1UL);
    EXPECT_EQ(tokenStreamValue->fragmentAtIndex(0), "Str");
}

} // namespace

} // namespace blink

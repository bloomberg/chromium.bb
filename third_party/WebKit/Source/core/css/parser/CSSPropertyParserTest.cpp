// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParser.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static int computeNumberOfTracks(CSSValueList* valueList)
{
    int numberOfTracks = 0;
    for (auto& value : *valueList) {
        if (value->isGridLineNamesValue())
            continue;
        ++numberOfTracks;
    }
    return numberOfTracks;
}

TEST(CSSPropertyParserTest, GridTrackLimit1)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(999999, 20px)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit2)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(999999, 20px)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit3)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(1000000, 10%)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit4)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(1000000, 10%)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit5)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(1000000, [first] min-content [last])");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit6)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(1000000, [first] min-content [last])");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit7)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(1000001, auto)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit8)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(1000001, auto)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit9)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(400000, 2em minmax(10px, max-content) 0.5fr)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit10)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(400000, 2em minmax(10px, max-content) 0.5fr)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit11)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit12)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit13)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(100000000000000000000, 10% 1fr)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit14)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(100000000000000000000, 10% 1fr)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit15)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateColumns, "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

TEST(CSSPropertyParserTest, GridTrackLimit16)
{
    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyGridTemplateRows, "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)");
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isValueList());
    EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), 999999);
}

} // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSPropertyParser.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "wtf/dtoa/utils.h"

#include <gtest/gtest.h>

namespace blink {

static unsigned computeNumberOfTracks(CSSValueList* valueList)
{
    unsigned numberOfTracks = 0;
    for (CSSValueListIterator i = valueList; i.hasMore(); i.advance()) {
        if (i.value()->isGridLineNamesValue())
            continue;
        ++numberOfTracks;
    }
    return numberOfTracks;
}

TEST(CSSPropertyParserTest, GridTrackLimits)
{
    struct {
        const CSSPropertyID propertyID;
        const char* input;
        const size_t output;
    } testCases[] = {
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(999999, 20px);", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(999999, 20px);", 999999},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000000, 10%);", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000000, 10%);", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000000, (first) min-content (last));", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000000, (first) min-content (last));", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(1000001, auto);", 1000000},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(1000001, auto);", 1000000},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(400000, 2em minmax(10px, max-content) 0.5fr);", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(400000, 2em minmax(10px, max-content) 0.5fr);", 999999},
        {CSSPropertyGridTemplateColumns, "grid-template-columns: repeat(600000, (first) 3vh 10% 2fr (nav) 10px auto 1fr 6em (last));", 999999},
        {CSSPropertyGridTemplateRows, "grid-template-rows: repeat(600000, (first) 3vh 10% 2fr (nav) 10px auto 1fr 6em (last));", 999999},
    };

    CSSParser parser(strictCSSParserContext());
    RefPtrWillBeRawPtr<MutableStylePropertySet> propertySet = MutableStylePropertySet::create();

    for (unsigned i = 0; i < ARRAY_SIZE(testCases); ++i) {
        ASSERT_TRUE(parser.parseDeclaration(propertySet.get(), testCases[i].input, 0, 0));
        RefPtrWillBeRawPtr<CSSValue> value = propertySet->getPropertyCSSValue(testCases[i].propertyID);

        ASSERT_TRUE(value->isValueList());
        EXPECT_EQ(computeNumberOfTracks(toCSSValueList(value.get())), testCases[i].output);
    }
}

} // namespace blink

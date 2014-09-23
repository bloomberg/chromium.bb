// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/BisonCSSParser.h"

#include "core/css/MediaList.h"
#include "core/css/StyleRule.h"
#include "wtf/dtoa/utils.h"
#include "wtf/text/StringBuilder.h"

#include <gtest/gtest.h>

namespace blink {

static void testMediaQuery(const char* expected, MediaQuerySet& querySet)
{
    const WillBeHeapVector<OwnPtrWillBeMember<MediaQuery> >& queryVector = querySet.queryVector();
    size_t queryVectorSize = queryVector.size();
    StringBuilder output;

    for (size_t i = 0; i < queryVectorSize; ) {
        String queryText = queryVector[i]->cssText();
        output.append(queryText);
        ++i;
        if (i >= queryVectorSize)
            break;
        output.appendLiteral(", ");
    }
    ASSERT_STREQ(expected, output.toString().ascii().data());
}

TEST(BisonCSSParserTest, MediaQuery)
{
    struct {
        const char* input;
        const char* output;
    } testCases[] = {
        {"@media s} {}", "not all"},
        {"@media } {}", "not all"},
        {"@media tv {}", "tv"},
        {"@media tv, screen {}", "tv, screen"},
        {"@media s}, tv {}", "not all, tv"},
        {"@media tv, screen and (}) {}", "tv, not all"},
    };

    BisonCSSParser parser(strictCSSParserContext());

    for (unsigned i = 0; i < ARRAY_SIZE(testCases); ++i) {
        RefPtrWillBeRawPtr<StyleRuleBase> rule = parser.parseRule(nullptr, String(testCases[i].input));

        EXPECT_TRUE(rule->isMediaRule());
        testMediaQuery(testCases[i].output, *static_cast<StyleRuleMedia*>(rule.get())->mediaQueries());
    }
}

} // namespace blink

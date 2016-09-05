// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/ActiveStyleSheets.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserMode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ActiveStyleSheetsTest : public ::testing::Test {
public:
    static CSSStyleSheet* createSheet()
    {
        StyleSheetContents* contents = StyleSheetContents::create(CSSParserContext(HTMLStandardMode, nullptr));
        contents->ensureRuleSet(MediaQueryEvaluator(), RuleHasDocumentSecurityOrigin);
        return CSSStyleSheet::create(contents);
    }
};

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_NoChange)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    EXPECT_EQ(NoActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    EXPECT_EQ(0u, changedRuleSets.size());

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

    newSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

    EXPECT_EQ(NoActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    EXPECT_EQ(0u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToEmpty)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();

    newSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsAppended, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    EXPECT_EQ(2u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToNonEmpty)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsAppended, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    EXPECT_EQ(1u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Mutated)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();
    CSSStyleSheet* sheet3 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    sheet2->contents()->clearRuleSet();
    sheet2->contents()->ensureRuleSet(MediaQueryEvaluator(), RuleHasDocumentSecurityOrigin);

    EXPECT_NE(oldSheets[1].second, &sheet2->contents()->ruleSet());

    newSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    ASSERT_EQ(2u, changedRuleSets.size());
    EXPECT_EQ(&sheet2->contents()->ruleSet(), changedRuleSets[0]);
    EXPECT_EQ(oldSheets[1].second, changedRuleSets[1]);
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Inserted)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();
    CSSStyleSheet* sheet3 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    newSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    ASSERT_EQ(1u, changedRuleSets.size());
    EXPECT_EQ(&sheet2->contents()->ruleSet(), changedRuleSets[0]);
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Removed)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();
    CSSStyleSheet* sheet3 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    newSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    ASSERT_EQ(1u, changedRuleSets.size());
    EXPECT_EQ(&sheet2->contents()->ruleSet(), changedRuleSets[0]);
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemovedAll)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();
    CSSStyleSheet* sheet3 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    EXPECT_EQ(3u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_InsertedAndRemoved)
{
    ActiveStyleSheetVector oldSheets;
    ActiveStyleSheetVector newSheets;
    HeapVector<Member<RuleSet>> changedRuleSets;

    CSSStyleSheet* sheet1 = createSheet();
    CSSStyleSheet* sheet2 = createSheet();
    CSSStyleSheet* sheet3 = createSheet();

    oldSheets.append(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
    oldSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

    newSheets.append(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
    newSheets.append(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

    EXPECT_EQ(ActiveSheetsChanged, compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
    ASSERT_EQ(2u, changedRuleSets.size());
    EXPECT_EQ(&sheet1->contents()->ruleSet(), changedRuleSets[0]);
    EXPECT_EQ(&sheet3->contents()->ruleSet(), changedRuleSets[1]);
}

} // namespace blink

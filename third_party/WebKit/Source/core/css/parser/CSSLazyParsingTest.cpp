// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "platform/testing/HistogramTester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSLazyParsingTest : public testing::Test {
 public:
  bool hasParsedProperties(StyleRule* rule) {
    return rule->hasParsedProperties();
  }

  StyleRule* ruleAt(StyleSheetContents* sheet, size_t index) {
    return toStyleRule(sheet->childRules()[index]);
  }

 protected:
  HistogramTester m_histogramTester;
  Persistent<StyleSheetContents> m_cachedContents;
};

TEST_F(CSSLazyParsingTest, Simple) {
  CSSParserContext* context = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "body { background-color: red; }";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);
  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_FALSE(hasParsedProperties(rule));
  rule->properties();
  EXPECT_TRUE(hasParsedProperties(rule));
}

// Avoiding lazy parsing for trivially empty blocks helps us perform the
// shouldConsiderForMatchingRules optimization.
TEST_F(CSSLazyParsingTest, DontLazyParseEmpty) {
  CSSParserContext* context = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "body {  }";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);
  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_TRUE(hasParsedProperties(rule));
  EXPECT_FALSE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Avoid parsing rules with ::before or ::after to avoid causing
// collectFeatures() when we trigger parsing for attr();
TEST_F(CSSLazyParsingTest, DontLazyParseBeforeAfter) {
  CSSParserContext* context = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText =
      "p::before { content: 'foo' } p .class::after { content: 'bar' } ";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  EXPECT_TRUE(hasParsedProperties(ruleAt(styleSheet, 0)));
  EXPECT_TRUE(hasParsedProperties(ruleAt(styleSheet, 1)));
}

// Test for crbug.com/664115 where |shouldConsiderForMatchingRules| would flip
// from returning false to true if the lazy property was parsed. This is a
// dangerous API because callers will expect the set of matching rules to be
// identical if the stylesheet is not mutated.
TEST_F(CSSLazyParsingTest, ShouldConsiderForMatchingRulesDoesntChange1) {
  CSSParserContext* context = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "p::first-letter { ,badness, } ";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_FALSE(hasParsedProperties(rule));
  EXPECT_TRUE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));

  // Parse the rule.
  rule->properties();

  // Now, we should still consider this for matching rules even if it is empty.
  EXPECT_TRUE(hasParsedProperties(rule));
  EXPECT_TRUE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Test the same thing as above, with a property that does not get lazy parsed,
// to ensure that we perform the optimization where possible.
TEST_F(CSSLazyParsingTest, ShouldConsiderForMatchingRulesSimple) {
  CSSParserContext* context = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "p::before { ,badness, } ";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_TRUE(hasParsedProperties(rule));
  EXPECT_FALSE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Regression test for crbug.com/660290 where we change the underlying owning
// document from the StyleSheetContents without changing the UseCounter. This
// test ensures that the new UseCounter is used when doing new parsing work.
TEST_F(CSSLazyParsingTest, ChangeDocuments) {
  std::unique_ptr<DummyPageHolder> dummyHolder =
      DummyPageHolder::create(IntSize(500, 500));
  CSSParserContext* context = CSSParserContext::create(
      HTMLStandardMode, CSSParserContext::DynamicProfile,
      &dummyHolder->document());
  m_cachedContents = StyleSheetContents::create(context);
  {
    CSSStyleSheet* sheet =
        CSSStyleSheet::create(m_cachedContents, dummyHolder->document());
    DCHECK(sheet);

    String sheetText = "body { background-color: red; } p { color: orange;  }";
    CSSParser::parseSheet(context, m_cachedContents, sheetText,
                          true /* lazy parse */);

    // Parse the first property set with the first document as owner.
    StyleRule* rule = ruleAt(m_cachedContents, 0);
    EXPECT_FALSE(hasParsedProperties(rule));
    rule->properties();
    EXPECT_TRUE(hasParsedProperties(rule));

    EXPECT_EQ(&dummyHolder->document(),
              m_cachedContents->singleOwnerDocument());
    UseCounter& useCounter1 = dummyHolder->document().page()->useCounter();
    EXPECT_TRUE(useCounter1.isCounted(CSSPropertyBackgroundColor));
    EXPECT_FALSE(useCounter1.isCounted(CSSPropertyColor));

    // Change owner document.
    m_cachedContents->unregisterClient(sheet);
    dummyHolder.reset();
  }
  // Ensure no stack references to oilpan objects.
  ThreadState::current()->collectAllGarbage();

  std::unique_ptr<DummyPageHolder> dummyHolder2 =
      DummyPageHolder::create(IntSize(500, 500));
  CSSStyleSheet* sheet2 =
      CSSStyleSheet::create(m_cachedContents, dummyHolder2->document());

  EXPECT_EQ(&dummyHolder2->document(), m_cachedContents->singleOwnerDocument());

  // Parse the second property set with the second document as owner.
  StyleRule* rule2 = ruleAt(m_cachedContents, 1);
  EXPECT_FALSE(hasParsedProperties(rule2));
  rule2->properties();
  EXPECT_TRUE(hasParsedProperties(rule2));

  UseCounter& useCounter2 = dummyHolder2->document().page()->useCounter();
  EXPECT_TRUE(sheet2);
  EXPECT_TRUE(useCounter2.isCounted(CSSPropertyColor));
  EXPECT_FALSE(useCounter2.isCounted(CSSPropertyBackgroundColor));
}

TEST_F(CSSLazyParsingTest, SimpleRuleUsagePercent) {
  CSSParserContext* context = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  std::string usageMetric = "Style.LazyUsage.Percent";
  std::string totalRulesMetric = "Style.TotalLazyRules";
  std::string totalRulesFullUsageMetric = "Style.TotalLazyRules.FullUsage";
  m_histogramTester.expectTotalCount(usageMetric, 0);

  String sheetText =
      "body { background-color: red; }"
      "p { color: blue; }"
      "a { color: yellow; }"
      "#id { color: blue; }"
      "div { color: grey; }";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  m_histogramTester.expectTotalCount(totalRulesMetric, 1);
  m_histogramTester.expectUniqueSample(totalRulesMetric, 5, 1);

  // Only log the full usage metric when all the rules have been actually
  // parsed.
  m_histogramTester.expectTotalCount(totalRulesFullUsageMetric, 0);

  m_histogramTester.expectTotalCount(usageMetric, 1);
  m_histogramTester.expectUniqueSample(usageMetric,
                                       CSSLazyParsingState::UsageGe0, 1);

  ruleAt(styleSheet, 0)->properties();
  m_histogramTester.expectTotalCount(usageMetric, 2);
  m_histogramTester.expectBucketCount(usageMetric,
                                      CSSLazyParsingState::UsageGt10, 1);

  ruleAt(styleSheet, 1)->properties();
  m_histogramTester.expectTotalCount(usageMetric, 3);
  m_histogramTester.expectBucketCount(usageMetric,
                                      CSSLazyParsingState::UsageGt25, 1);

  ruleAt(styleSheet, 2)->properties();
  m_histogramTester.expectTotalCount(usageMetric, 4);
  m_histogramTester.expectBucketCount(usageMetric,
                                      CSSLazyParsingState::UsageGt50, 1);

  ruleAt(styleSheet, 3)->properties();
  m_histogramTester.expectTotalCount(usageMetric, 5);
  m_histogramTester.expectBucketCount(usageMetric,
                                      CSSLazyParsingState::UsageGt75, 1);

  // Only log the full usage metric when all the rules have been actually
  // parsed.
  m_histogramTester.expectTotalCount(totalRulesFullUsageMetric, 0);

  // Parsing the last rule bumps both Gt90 and All buckets.
  ruleAt(styleSheet, 4)->properties();
  m_histogramTester.expectTotalCount(usageMetric, 7);
  m_histogramTester.expectBucketCount(usageMetric,
                                      CSSLazyParsingState::UsageGt90, 1);
  m_histogramTester.expectBucketCount(usageMetric,
                                      CSSLazyParsingState::UsageAll, 1);

  m_histogramTester.expectTotalCount(totalRulesFullUsageMetric, 1);
  m_histogramTester.expectUniqueSample(totalRulesFullUsageMetric, 5, 1);
}

}  // namespace blink

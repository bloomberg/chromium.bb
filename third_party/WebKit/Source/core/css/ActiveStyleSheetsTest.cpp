// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/ActiveStyleSheets.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ActiveStyleSheetsTest : public ::testing::Test {
 protected:
  static CSSStyleSheet* createSheet(const String& cssText = String()) {
    StyleSheetContents* contents =
        StyleSheetContents::create(CSSParserContext::create(HTMLStandardMode));
    contents->parseString(cssText);
    contents->ensureRuleSet(MediaQueryEvaluator(),
                            RuleHasDocumentSecurityOrigin);
    return CSSStyleSheet::create(contents);
  }
};

class ApplyRulesetsTest : public ActiveStyleSheetsTest {
 protected:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  }

  Document& document() { return m_dummyPageHolder->document(); }
  StyleEngine& styleEngine() { return document().styleEngine(); }
  ShadowRoot& attachShadow(Element& host);

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

ShadowRoot& ApplyRulesetsTest::attachShadow(Element& host) {
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadowRoot = host.attachShadow(
      ScriptState::forMainWorld(document().frame()), init, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(shadowRoot);
  return *shadowRoot;
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_NoChange) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  EXPECT_EQ(NoActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(0u, changedRuleSets.size());

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  EXPECT_EQ(NoActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(0u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToEmpty) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsAppended,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(2u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToNonEmpty) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsAppended,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(1u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Mutated) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  sheet2->contents()->clearRuleSet();
  sheet2->contents()->ensureRuleSet(MediaQueryEvaluator(),
                                    RuleHasDocumentSecurityOrigin);

  EXPECT_NE(oldSheets[1].second, &sheet2->contents()->ruleSet());

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(2u, changedRuleSets.size());
  EXPECT_TRUE(changedRuleSets.contains(&sheet2->contents()->ruleSet()));
  EXPECT_TRUE(changedRuleSets.contains(oldSheets[1].second));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Inserted) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(1u, changedRuleSets.size());
  EXPECT_TRUE(changedRuleSets.contains(&sheet2->contents()->ruleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Removed) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(1u, changedRuleSets.size());
  EXPECT_TRUE(changedRuleSets.contains(&sheet2->contents()->ruleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemovedAll) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(3u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_InsertedAndRemoved) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(2u, changedRuleSets.size());
  EXPECT_TRUE(changedRuleSets.contains(&sheet1->contents()->ruleSet()));
  EXPECT_TRUE(changedRuleSets.contains(&sheet3->contents()->ruleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddNullRuleSet) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet2, nullptr));

  EXPECT_EQ(NoActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(0u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemoveNullRuleSet) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, nullptr));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));

  EXPECT_EQ(NoActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(0u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddRemoveNullRuleSet) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, nullptr));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet3, nullptr));

  EXPECT_EQ(NoActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(0u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_RemoveNullRuleSetAndAppend) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();
  CSSStyleSheet* sheet3 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, nullptr));

  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet3, &sheet3->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(1u, changedRuleSets.size());
  EXPECT_TRUE(changedRuleSets.contains(&sheet3->contents()->ruleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_ReorderedImportSheets) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  // It is possible to have CSSStyleSheet pointers re-orderered for html imports
  // because their documents, and hence their stylesheets are persisted on
  // remove / insert. This test is here to show that the active sheet comparison
  // is not able to see that anything changed.
  //
  // Imports are handled by forcing re-append and recalc of the document scope
  // when html imports are removed.
  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  oldSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));

  EXPECT_EQ(NoActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(0u, changedRuleSets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_DisableAndAppend) {
  ActiveStyleSheetVector oldSheets;
  ActiveStyleSheetVector newSheets;
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  CSSStyleSheet* sheet1 = createSheet();
  CSSStyleSheet* sheet2 = createSheet();

  oldSheets.push_back(std::make_pair(sheet1, &sheet1->contents()->ruleSet()));
  newSheets.push_back(std::make_pair(sheet1, nullptr));
  newSheets.push_back(std::make_pair(sheet2, &sheet2->contents()->ruleSet()));

  EXPECT_EQ(ActiveSheetsChanged,
            compareActiveStyleSheets(oldSheets, newSheets, changedRuleSets));
  EXPECT_EQ(2u, changedRuleSets.size());
}

TEST_F(ApplyRulesetsTest, AddUniversalRuleToDocument) {
  document().view()->updateAllLifecyclePhases();

  CSSStyleSheet* sheet = createSheet("body * { color:red }");

  ActiveStyleSheetVector newStyleSheets;
  newStyleSheets.push_back(
      std::make_pair(sheet, &sheet->contents()->ruleSet()));

  styleEngine().applyRuleSetChanges(document(), ActiveStyleSheetVector(),
                                    newStyleSheets);

  EXPECT_EQ(SubtreeStyleChange, document().getStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddUniversalRuleToShadowTree) {
  document().body()->setInnerHTML("<div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadowRoot = attachShadow(*host);
  document().view()->updateAllLifecyclePhases();

  CSSStyleSheet* sheet = createSheet("body * { color:red }");

  ActiveStyleSheetVector newStyleSheets;
  newStyleSheets.push_back(
      std::make_pair(sheet, &sheet->contents()->ruleSet()));

  styleEngine().applyRuleSetChanges(shadowRoot, ActiveStyleSheetVector(),
                                    newStyleSheets);

  EXPECT_FALSE(document().needsStyleRecalc());
  EXPECT_EQ(SubtreeStyleChange, host->getStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddShadowV0BoundaryCrossingRuleToDocument) {
  document().view()->updateAllLifecyclePhases();

  CSSStyleSheet* sheet = createSheet(".a /deep/ .b { color:red }");

  ActiveStyleSheetVector newStyleSheets;
  newStyleSheets.push_back(
      std::make_pair(sheet, &sheet->contents()->ruleSet()));

  styleEngine().applyRuleSetChanges(document(), ActiveStyleSheetVector(),
                                    newStyleSheets);

  EXPECT_EQ(SubtreeStyleChange, document().getStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddShadowV0BoundaryCrossingRuleToShadowTree) {
  document().body()->setInnerHTML("<div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadowRoot = attachShadow(*host);
  document().view()->updateAllLifecyclePhases();

  CSSStyleSheet* sheet = createSheet(".a /deep/ .b { color:red }");

  ActiveStyleSheetVector newStyleSheets;
  newStyleSheets.push_back(
      std::make_pair(sheet, &sheet->contents()->ruleSet()));

  styleEngine().applyRuleSetChanges(shadowRoot, ActiveStyleSheetVector(),
                                    newStyleSheets);

  EXPECT_FALSE(document().needsStyleRecalc());
  EXPECT_EQ(SubtreeStyleChange, host->getStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddFontFaceRuleToDocument) {
  document().view()->updateAllLifecyclePhases();

  CSSStyleSheet* sheet =
      createSheet("@font-face { font-family: ahum; src: url(ahum.ttf) }");

  ActiveStyleSheetVector newStyleSheets;
  newStyleSheets.push_back(
      std::make_pair(sheet, &sheet->contents()->ruleSet()));

  styleEngine().applyRuleSetChanges(document(), ActiveStyleSheetVector(),
                                    newStyleSheets);

  EXPECT_EQ(SubtreeStyleChange, document().getStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddFontFaceRuleToShadowTree) {
  document().body()->setInnerHTML("<div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadowRoot = attachShadow(*host);
  document().view()->updateAllLifecyclePhases();

  CSSStyleSheet* sheet =
      createSheet("@font-face { font-family: ahum; src: url(ahum.ttf) }");

  ActiveStyleSheetVector newStyleSheets;
  newStyleSheets.push_back(
      std::make_pair(sheet, &sheet->contents()->ruleSet()));

  styleEngine().applyRuleSetChanges(shadowRoot, ActiveStyleSheetVector(),
                                    newStyleSheets);

  EXPECT_FALSE(document().needsStyleRecalc());
  EXPECT_FALSE(document().childNeedsStyleRecalc());
  EXPECT_FALSE(document().needsStyleInvalidation());
  EXPECT_FALSE(document().childNeedsStyleInvalidation());
}

TEST_F(ApplyRulesetsTest, RemoveSheetFromShadowTree) {
  document().body()->setInnerHTML("<div id=host></div>");
  Element* host = document().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadowRoot = attachShadow(*host);
  shadowRoot.setInnerHTML("<style>::slotted(#dummy){color:pink}</style>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_EQ(1u, styleEngine().treeBoundaryCrossingScopes().size());
  ASSERT_EQ(1u, shadowRoot.styleSheets().length());

  StyleSheet* sheet = shadowRoot.styleSheets().item(0);
  ASSERT_TRUE(sheet);
  ASSERT_TRUE(sheet->isCSSStyleSheet());

  CSSStyleSheet* cssSheet = toCSSStyleSheet(sheet);
  ActiveStyleSheetVector oldStyleSheets;
  oldStyleSheets.push_back(
      std::make_pair(cssSheet, &cssSheet->contents()->ruleSet()));
  styleEngine().applyRuleSetChanges(shadowRoot, oldStyleSheets,
                                    ActiveStyleSheetVector());

  EXPECT_TRUE(styleEngine().treeBoundaryCrossingScopes().isEmpty());
}

}  // namespace blink

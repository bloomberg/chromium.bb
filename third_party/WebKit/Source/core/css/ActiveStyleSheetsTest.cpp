// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/ActiveStyleSheets.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleEngine.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/MediaQueryParser.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/ShadowRootInit.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ActiveStyleSheetsTest : public ::testing::Test {
 protected:
  static CSSStyleSheet* CreateSheet(const String& css_text = String()) {
    StyleSheetContents* contents =
        StyleSheetContents::Create(CSSParserContext::Create(kHTMLStandardMode));
    contents->ParseString(css_text);
    contents->EnsureRuleSet(MediaQueryEvaluator(),
                            kRuleHasDocumentSecurityOrigin);
    return CSSStyleSheet::Create(contents);
  }
};

class ApplyRulesetsTest : public ActiveStyleSheetsTest {
 protected:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }
  ShadowRoot& AttachShadow(Element& host);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

ShadowRoot& ApplyRulesetsTest::AttachShadow(Element& host) {
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host.attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                        init, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(shadow_root);
  return *shadow_root;
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_NoChange) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToEmpty) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsAppended,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToNonEmpty) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsAppended,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Mutated) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  sheet2->Contents()->ClearRuleSet();
  sheet2->Contents()->EnsureRuleSet(MediaQueryEvaluator(),
                                    kRuleHasDocumentSecurityOrigin);

  EXPECT_NE(old_sheets[1].second, &sheet2->Contents()->GetRuleSet());

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(old_sheets[1].second));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Inserted) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Removed) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemovedAll) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(3u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_InsertedAndRemoved) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddNullRuleSet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(sheet2, nullptr));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemoveNullRuleSet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddRemoveNullRuleSet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(sheet3, nullptr));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_RemoveNullRuleSetAndAppend) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_ReorderedImportSheets) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  // It is possible to have CSSStyleSheet pointers re-orderered for html imports
  // because their documents, and hence their stylesheets are persisted on
  // remove / insert. This test is here to show that the active sheet comparison
  // is not able to see that anything changed.
  //
  // Imports are handled by forcing re-append and recalc of the document scope
  // when html imports are removed.
  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_DisableAndAppend) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(sheet1, nullptr));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddRemoveNonMatchingMQ) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());

  CSSStyleSheet* sheet1 = CreateSheet();
  RefPtr<MediaQuerySet> mq =
      MediaQueryParser::ParseMediaQuerySet("(min-width: 9000px)");
  sheet1->SetMediaQueries(mq);
  sheet1->MatchesMediaQueries(MediaQueryEvaluator());

  new_sheets.push_back(std::make_pair(sheet1, nullptr));

  EXPECT_EQ(
      kActiveSheetsAppended,
      CompareActiveStyleSheets(old_sheets, new_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(new_sheets, old_sheets, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ApplyRulesetsTest, AddUniversalRuleToDocument) {
  GetDocument().View()->UpdateAllLifecyclePhases();

  CSSStyleSheet* sheet = CreateSheet("body * { color:red }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(GetDocument(), ActiveStyleSheetVector(),
                                       new_style_sheets);

  EXPECT_EQ(kSubtreeStyleChange, GetDocument().GetStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddUniversalRuleToShadowTree) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root = AttachShadow(*host);
  GetDocument().View()->UpdateAllLifecyclePhases();

  CSSStyleSheet* sheet = CreateSheet("body * { color:red }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(shadow_root, ActiveStyleSheetVector(),
                                       new_style_sheets);

  EXPECT_FALSE(GetDocument().NeedsStyleRecalc());
  EXPECT_EQ(kSubtreeStyleChange, host->GetStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddFontFaceRuleToDocument) {
  GetDocument().View()->UpdateAllLifecyclePhases();

  CSSStyleSheet* sheet =
      CreateSheet("@font-face { font-family: ahum; src: url(ahum.ttf) }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(GetDocument(), ActiveStyleSheetVector(),
                                       new_style_sheets);

  EXPECT_EQ(kSubtreeStyleChange, GetDocument().GetStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddFontFaceRuleToShadowTree) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root = AttachShadow(*host);
  GetDocument().View()->UpdateAllLifecyclePhases();

  CSSStyleSheet* sheet =
      CreateSheet("@font-face { font-family: ahum; src: url(ahum.ttf) }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(shadow_root, ActiveStyleSheetVector(),
                                       new_style_sheets);

  EXPECT_FALSE(GetDocument().NeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
}

TEST_F(ApplyRulesetsTest, RemoveSheetFromShadowTree) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root = AttachShadow(*host);
  shadow_root.setInnerHTML("<style>::slotted(#dummy){color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(GetStyleEngine().TreeBoundaryCrossingScopes().IsEmpty());
  ASSERT_EQ(1u, shadow_root.StyleSheets().length());

  StyleSheet* sheet = shadow_root.StyleSheets().item(0);
  ASSERT_TRUE(sheet);
  ASSERT_TRUE(sheet->IsCSSStyleSheet());

  CSSStyleSheet* css_sheet = ToCSSStyleSheet(sheet);
  ActiveStyleSheetVector old_style_sheets;
  old_style_sheets.push_back(
      std::make_pair(css_sheet, &css_sheet->Contents()->GetRuleSet()));
  GetStyleEngine().ApplyRuleSetChanges(shadow_root, old_style_sheets,
                                       ActiveStyleSheetVector());

  EXPECT_TRUE(GetStyleEngine().TreeBoundaryCrossingScopes().IsEmpty());
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StyleEngine.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/ShadowRootInit.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StyleEngineTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }

  bool IsDocumentStyleSheetCollectionClean() {
    return !GetStyleEngine().ShouldUpdateDocumentStyleSheetCollection();
  }

  enum RuleSetInvalidation {
    kRuleSetInvalidationsScheduled,
    kRuleSetInvalidationFullRecalc
  };
  RuleSetInvalidation ScheduleInvalidationsForRules(TreeScope&,
                                                    const String& css_text);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void StyleEngineTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

StyleEngineTest::RuleSetInvalidation
StyleEngineTest::ScheduleInvalidationsForRules(TreeScope& tree_scope,
                                               const String& css_text) {
  StyleSheetContents* sheet =
      StyleSheetContents::Create(CSSParserContext::Create(kHTMLStandardMode));
  sheet->ParseString(css_text);
  HeapHashSet<Member<RuleSet>> rule_sets;
  RuleSet& rule_set = sheet->EnsureRuleSet(MediaQueryEvaluator(),
                                           kRuleHasDocumentSecurityOrigin);
  rule_set.CompactRulesIfNeeded();
  if (rule_set.NeedsFullRecalcForRuleSetInvalidation())
    return kRuleSetInvalidationFullRecalc;
  rule_sets.insert(&rule_set);
  GetStyleEngine().ScheduleInvalidationsForRuleSets(tree_scope, rule_sets);
  return kRuleSetInvalidationsScheduled;
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject) {
  StyleSheetContents* parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_sheet->ParseString("div {}");
  GetStyleEngine().InjectAuthorSheet(parsed_sheet);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(IsDocumentStyleSheetCollectionClean());
}

TEST_F(StyleEngineTest, AnalyzedInject) {
  GetDocument().body()->setInnerHTML(
      "<style>div { color: red }</style><div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  const unsigned initial_count = GetStyleEngine().StyleForElementCount();

  StyleSheetContents* green_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  green_parsed_sheet->ParseString("#t1 { color: green }");
  WebStyleSheetId green_id =
      GetStyleEngine().InjectAuthorSheet(green_parsed_sheet);
  EXPECT_EQ(1u, green_id);
  EXPECT_EQ(1u, GetStyleEngine().InjectedAuthorStyleSheets().size());
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  StyleSheetContents* blue_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  blue_parsed_sheet->ParseString("#t1 { color: blue }");
  WebStyleSheetId blue_id =
      GetStyleEngine().InjectAuthorSheet(blue_parsed_sheet);
  EXPECT_EQ(2u, blue_id);
  EXPECT_EQ(2u, GetStyleEngine().InjectedAuthorStyleSheets().size());
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(2u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 255),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  GetStyleEngine().RemoveInjectedAuthorSheet(green_id);
  EXPECT_EQ(1u, GetStyleEngine().InjectedAuthorStyleSheets().size());
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(3u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 255),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  GetStyleEngine().RemoveInjectedAuthorSheet(blue_id);
  EXPECT_EQ(0u, GetStyleEngine().InjectedAuthorStyleSheets().size());
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(4u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));
}

TEST_F(StyleEngineTest, TextToSheetCache) {
  HTMLStyleElement* element = HTMLStyleElement::Create(GetDocument(), false);

  String sheet_text("div {}");
  TextPosition min_pos = TextPosition::MinimumPosition();
  StyleEngineContext context;

  CSSStyleSheet* sheet1 =
      GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that the first sheet is not using a cached StyleSheetContents.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());

  CSSStyleSheet* sheet2 =
      GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that the second sheet uses the cached StyleSheetContents for the
  // first.
  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());
  EXPECT_TRUE(sheet2->Contents()->IsUsedFromTextCache());

  sheet1 = nullptr;
  sheet2 = nullptr;
  element = nullptr;

  // Garbage collection should clear the weak reference in the
  // StyleSheetContents cache.
  ThreadState::Current()->CollectAllGarbage();

  element = HTMLStyleElement::Create(GetDocument(), false);
  sheet1 = GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that we did not use a cached StyleSheetContents after the garbage
  // collection.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());
}

TEST_F(StyleEngineTest, RuleSetInvalidationTypeSelectors) {
  GetDocument().body()->setInnerHTML(
      "<div>"
      "  <span></span>"
      "  <div></div>"
      "</div>"
      "<b></b><b></b><b></b><b></b>"
      "<i id=i>"
      "  <i>"
      "    <b></b>"
      "  </i>"
      "</i>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "span { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "body div { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);

  EXPECT_EQ(kRuleSetInvalidationFullRecalc,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "div * { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();

  before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "#i b { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationCustomPseudo) {
  GetDocument().body()->setInnerHTML(
      "<style>progress { -webkit-appearance:none }</style>"
      "<progress></progress>"
      "<div></div><div></div><div></div><div></div><div></div><div></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                GetDocument(), "::-webkit-progress-bar { background: green }"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(3u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHost) {
  GetDocument().body()->setInnerHTML(
      "<div id=nohost></div><div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->setInnerHTML("<div></div><div></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ":host(#nohost), #nohost { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(ScheduleInvalidationsForRules(*shadow_root,
                                          ":host(#host) { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  EXPECT_EQ(ScheduleInvalidationsForRules(*shadow_root,
                                          ":host(*) { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ":host(*) :hover { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationSlotted) {
  GetDocument().body()->setInnerHTML(
      "<div id=host>"
      "  <span slot=other class=s1></span>"
      "  <span class=s2></span>"
      "  <span class=s1></span>"
      "  <span></span>"
      "</div>");

  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->setInnerHTML("<slot name=other></slot><slot></slot>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, "::slotted(.s1) { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(ScheduleInvalidationsForRules(*shadow_root,
                                          "::slotted(*) { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHostContext) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->setInnerHTML("<div></div><div class=a></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(
      ScheduleInvalidationsForRules(
          *shadow_root, ":host-context(.nomatch) .a { background: green}"),
      kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ":host-context(:hover) { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ":host-context(#host) { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationV0BoundaryCrossing) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->setInnerHTML("<div></div><div class=a></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ".a ::content span { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ".a /deep/ span { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ".a::shadow span { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, HasViewportDependentMediaQueries) {
  GetDocument().body()->setInnerHTML(
      "<style>div {}</style>"
      "<style id='sheet' media='(min-width: 200px)'>"
      "  div {}"
      "</style>");

  Element* style_element = GetDocument().getElementById("sheet");

  for (unsigned i = 0; i < 10; i++) {
    GetDocument().body()->RemoveChild(style_element);
    GetDocument().View()->UpdateAllLifecyclePhases();
    GetDocument().body()->AppendChild(style_element);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  EXPECT_TRUE(
      GetDocument().GetStyleEngine().HasViewportDependentMediaQueries());

  GetDocument().body()->RemoveChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(
      GetDocument().GetStyleEngine().HasViewportDependentMediaQueries());
}

TEST_F(StyleEngineTest, StyleMediaAttributeStyleChange) {
  GetDocument().body()->setInnerHTML(
      "<style id='s1' media='(max-width: 1px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::HTMLNames::mediaAttr, "(max-width: 2000px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));
}

TEST_F(StyleEngineTest, StyleMediaAttributeNoStyleChange) {
  GetDocument().body()->setInnerHTML(
      "<style id='s1' media='(max-width: 1000px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::HTMLNames::mediaAttr, "(max-width: 2000px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));
}

TEST_F(StyleEngineTest, ModifyStyleRuleMatchedPropertiesCache) {
  // Test that the MatchedPropertiesCache is cleared when a StyleRule is
  // modified. The MatchedPropertiesCache caches results based on
  // StylePropertySet pointers. When a mutable StylePropertySet is modified,
  // the pointer doesn't change, yet the declarations do.

  GetDocument().body()->setInnerHTML(
      "<style id='s1'>#t1 { color: blue }</style>"
      "<div id='t1'>Green</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 255),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  CSSStyleSheet* sheet = ToCSSStyleSheet(GetDocument().StyleSheets().item(0));
  ASSERT_TRUE(sheet);
  ASSERT_TRUE(sheet->cssRules());
  CSSStyleRule* style_rule = ToCSSStyleRule(sheet->cssRules()->item(0));
  ASSERT_TRUE(style_rule);
  ASSERT_TRUE(style_rule->style());

  // Modify the StylePropertySet once to make it a mutable set. Subsequent
  // modifications will not change the StylePropertySet pointer and cache hash
  // value will be the same.
  style_rule->style()->setProperty("color", "red", "", ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));

  style_rule->style()->setProperty("color", "green", "", ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0),
            t1->GetComputedStyle()->VisitedDependentColor(CSSPropertyColor));
}

TEST_F(StyleEngineTest, ScheduleInvalidationAfterSubtreeRecalc) {
  GetDocument().body()->setInnerHTML(
      "<style id='s1'>"
      "  .t1 span { color: green }"
      "  .t2 span { color: green }"
      "</style>"
      "<style id='s2'>div { background: lime }</style>"
      "<div id='t1'></div>"
      "<div id='t2'></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);

  // Sanity test.
  t1->setAttribute(blink::HTMLNames::classAttr, "t1");
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();

  // platformColorsChanged() triggers SubtreeStyleChange on document(). If that
  // for some reason should change, this test will start failing and the
  // SubtreeStyleChange must be set another way.
  // Calling setNeedsStyleRecalc() explicitly with an arbitrary reason instead
  // requires us to CORE_EXPORT the reason strings.
  GetStyleEngine().PlatformColorsChanged();

  // Check that no invalidations sets are scheduled when the document node is
  // already SubtreeStyleChange.
  t2->setAttribute(blink::HTMLNames::classAttr, "t2");
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  HTMLStyleElement* s2 = toHTMLStyleElement(GetDocument().getElementById("s2"));
  ASSERT_TRUE(s2);
  s2->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_TRUE(GetDocument().NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().PlatformColorsChanged();
  s2->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  HTMLStyleElement* s1 = toHTMLStyleElement(GetDocument().getElementById("s1"));
  ASSERT_TRUE(s1);
  s1->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(t1->NeedsStyleInvalidation());
  EXPECT_TRUE(t2->NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().PlatformColorsChanged();
  s1->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(t1->NeedsStyleInvalidation());
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, NoScheduledRuleSetInvalidationsOnNewShadow) {
  GetDocument().body()->setInnerHTML("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  GetDocument().View()->UpdateAllLifecyclePhases();
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->setInnerHTML(
      "<style>"
      "  span { color: green }"
      "  t1 { color: green }"
      "</style>"
      "<div id='t1'></div>"
      "<span></span>");

  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyHttpEquivDefaultStyle) {
  GetDocument().body()->setInnerHTML(
      "<style>div { color:pink }</style><div id=container></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  container->setInnerHTML("<meta http-equiv='default-style' content=''>");
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  container->setInnerHTML(
      "<meta http-equiv='default-style' content='preferred'>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_Document) {
  GetDocument().body()->setInnerHTML("<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetDocument().body()->setInnerHTML(
      "<style>span { color: green }</style><style>div { color: pink }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& second_sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(2u, second_sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetStyleEngine().MarkAllTreeScopesDirty();
  GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_ShadowRoot) {
  GetDocument().body()->setInnerHTML("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  GetDocument().View()->UpdateAllLifecyclePhases();
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->setInnerHTML("<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(*shadow_root);
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  shadow_root->setInnerHTML(
      "<style>span { color: green }</style><style>div { color: pink }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& second_sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(*shadow_root);
  EXPECT_EQ(2u, second_sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetStyleEngine().MarkAllTreeScopesDirty();
  GetStyleEngine().StyleSheetsForStyleSheetList(*shadow_root);
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

}  // namespace blink

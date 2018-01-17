// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/StyleEngine.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/ShadowRootInit.h"
#include "core/dom/ViewportDescription.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/FloatSize.h"
#include "platform/heap/Heap.h"
#include "public/platform/WebFloatRect.h"
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
      StyleSheetContents::Create(CSSParserContext::Create(
          kHTMLStandardMode, SecureContextMode::kInsecureContext));
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
  GetStyleEngine().InjectSheet(parsed_sheet);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(IsDocumentStyleSheetCollectionClean());
}

TEST_F(StyleEngineTest, AnalyzedInject) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
     @font-face {
      font-family: 'Cool Font';
      src: local(monospace);
      font-weight: bold;
     }
     :root {
      --stop-color: black !important;
      --go-color: white;
     }
     #t1 { color: red !important }
     #t2 { color: black }
     #t4 { font-family: 'Cool Font'; font-weight: bold; font-style: italic }
     #t5 { animation-name: dummy-animation }
     #t6 { color: var(--stop-color); }
     #t7 { color: var(--go-color); }
     .red { color: red; }
    </style>
    <div id='t1'>Green</div>
    <div id='t2'>White</div>
    <div id='t3' style='color: black !important'>White</div>
    <div id='t4'>I look cool.</div>
    <div id='t5'>I animate!</div>
    <div id='t6'>Stop!</div>
    <div id='t7'>Go</div>
    <div id='t8' style='color: white !important'>screen: Red; print: Black</div>
    <div id='t9' class='red'>Green</div>
    <div id='t10' style='color: black !important'>Black</div>
    <div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  Element* t3 = GetDocument().getElementById("t3");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);
  ASSERT_TRUE(t3);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t2->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t3->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  const unsigned initial_count = GetStyleEngine().StyleForElementCount();

  StyleSheetContents* green_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  green_parsed_sheet->ParseString(
      "#t1 { color: green !important }"
      "#t2 { color: white !important }"
      "#t3 { color: white }");
  WebStyleSheetId green_id =
      GetStyleEngine().InjectSheet(green_parsed_sheet,
                                   WebDocument::kUserOrigin);
  EXPECT_EQ(1u, green_id);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(3u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Important user rules override both regular and important author rules.
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t3->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  StyleSheetContents* blue_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  blue_parsed_sheet->ParseString(
      "#t1 { color: blue !important }"
      "#t2 { color: silver }"
      "#t3 { color: silver !important }");
  WebStyleSheetId blue_id =
      GetStyleEngine().InjectSheet(blue_parsed_sheet, WebDocument::kUserOrigin);
  EXPECT_EQ(2u, blue_id);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(6u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Only important user rules override previously set important user rules.
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  // Important user rules override inline author rules.
  EXPECT_EQ(
      MakeRGB(192, 192, 192),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(green_id);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(9u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Regular user rules do not override author rules.
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t2->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(192, 192, 192),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(blue_id);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(12u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t2->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t3->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  // @font-face rules

  Element* t4 = GetDocument().getElementById("t4");
  ASSERT_TRUE(t4);
  ASSERT_TRUE(t4->GetComputedStyle());

  // There's only one font and it's bold and normal.
  EXPECT_EQ(1u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  CSSSegmentedFontFace* font_face =
      GetStyleEngine().GetFontSelector()->GetFontFaceCache()
      ->Get(t4->GetComputedStyle()->GetFontDescription(),
            AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  FontSelectionCapabilities capabilities =
      font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({BoldWeightValue(), BoldWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));

  StyleSheetContents* font_face_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  font_face_parsed_sheet->ParseString(
      "@font-face {"
      " font-family: 'Cool Font';"
      " src: local(monospace);"
      " font-weight: bold;"
      " font-style: italic;"
      "}"
    );
  WebStyleSheetId font_face_id =
      GetStyleEngine().InjectSheet(font_face_parsed_sheet,
                                   WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // After injecting a more specific font, now there are two and the
  // bold-italic one is selected.
  EXPECT_EQ(2u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()
              ->Get(t4->GetComputedStyle()->GetFontDescription(),
                    AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({BoldWeightValue(), BoldWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({ItalicSlopeValue(), ItalicSlopeValue()}));

  HTMLStyleElement* style_element = HTMLStyleElement::Create(
      GetDocument(), false);
  style_element->SetInnerHTMLFromString(
      "@font-face {"
      " font-family: 'Cool Font';"
      " src: local(monospace);"
      " font-weight: normal;"
      " font-style: italic;"
      "}"
    );
  GetDocument().body()->AppendChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Now there are three fonts, but the newest one does not override the older,
  // better matching one.
  EXPECT_EQ(3u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()
              ->Get(t4->GetComputedStyle()->GetFontDescription(),
                    AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({BoldWeightValue(), BoldWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({ItalicSlopeValue(), ItalicSlopeValue()}));

  GetStyleEngine().RemoveInjectedSheet(font_face_id);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // After removing the injected style sheet we're left with a bold-normal and
  // a normal-italic font, and the latter is selected by the matching algorithm
  // as font-style trumps font-weight.
  EXPECT_EQ(2u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()
              ->Get(t4->GetComputedStyle()->GetFontDescription(),
                    AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({NormalWeightValue(), NormalWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({ItalicSlopeValue(), ItalicSlopeValue()}));

  // @keyframes rules

  Element* t5 = GetDocument().getElementById("t5");
  ASSERT_TRUE(t5);

  // There's no @keyframes rule named dummy-animation
  ASSERT_FALSE(GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation")));

  StyleSheetContents* keyframes_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  keyframes_parsed_sheet->ParseString("@keyframes dummy-animation { from {} }");
  WebStyleSheetId keyframes_id =
      GetStyleEngine().InjectSheet(keyframes_parsed_sheet,
                                   WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // After injecting the style sheet, a @keyframes rule named dummy-animation
  // is found with one keyframe.
  StyleRuleKeyframes* keyframes =
      GetStyleEngine().Resolver()->FindKeyframesRule(
          t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  style_element = HTMLStyleElement::Create(GetDocument(), false);
  style_element->SetInnerHTMLFromString(
      "@keyframes dummy-animation { from {} to {} }");
  GetDocument().body()->AppendChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Author @keyframes rules take precedence; now there are two keyframes (from
  // and to).
  keyframes = GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(2u, keyframes->Keyframes().size());

  GetDocument().body()->RemoveChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  keyframes = GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  GetStyleEngine().RemoveInjectedSheet(keyframes_id);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Injected @keyframes rules are no longer available once removed.
  ASSERT_FALSE(GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation")));

  // Custom properties

  Element* t6 = GetDocument().getElementById("t6");
  Element* t7 = GetDocument().getElementById("t7");
  ASSERT_TRUE(t6);
  ASSERT_TRUE(t7);
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  StyleSheetContents* custom_properties_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  custom_properties_parsed_sheet->ParseString(
      ":root {"
      " --stop-color: red !important;"
      " --go-color: green;"
      "}");
  WebStyleSheetId custom_properties_id =
      GetStyleEngine().InjectSheet(custom_properties_parsed_sheet,
                                   WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(custom_properties_id);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Media queries

  Element* t8 = GetDocument().getElementById("t8");
  ASSERT_TRUE(t8);
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  StyleSheetContents* media_queries_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  media_queries_parsed_sheet->ParseString(
      "@media screen {"
      " #t8 {"
      "  color: red !important;"
      " }"
      "}"
      "@media print {"
      " #t8 {"
      "  color: black !important;"
      " }"
      "}");
  WebStyleSheetId media_queries_sheet_id =
      GetStyleEngine().InjectSheet(media_queries_parsed_sheet,
                                   WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t8->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  FloatSize page_size(400, 400);
  GetDocument().GetFrame()->SetPrinting(true, page_size, page_size, 1);
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t8->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  GetDocument().GetFrame()->SetPrinting(false, FloatSize(), FloatSize(), 0);
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t8->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(media_queries_sheet_id);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Author style sheets

  Element* t9 = GetDocument().getElementById("t9");
  Element* t10 = GetDocument().getElementById("t10");
  ASSERT_TRUE(t9);
  ASSERT_TRUE(t10);
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t9->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t10->GetComputedStyle()->VisitedDependentColor(
                                   GetCSSPropertyColor()));

  StyleSheetContents* parsed_author_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_author_sheet->ParseString(
      "#t9 {"
      " color: green;"
      "}"
      "#t10 {"
      " color: white !important;"
      "}");
  WebStyleSheetId author_sheet_id =
      GetStyleEngine().InjectSheet(parsed_author_sheet,
                                   WebDocument::kAuthorOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());

  // Specificity works within author origin.
  EXPECT_EQ(MakeRGB(0, 128, 0), t9->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  // Important author rules do not override important inline author rules.
  EXPECT_EQ(MakeRGB(0, 0, 0), t10->GetComputedStyle()->VisitedDependentColor(
                                   GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(author_sheet_id);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t9->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t10->GetComputedStyle()->VisitedDependentColor(
                                   GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, IgnoreInvalidPropertyValue) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<section><div id='t1'>Red</div></section>"
      "<style id='s1'>div { color: red; } section div#t1 { color:rgb(0");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
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
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div>
      <span></span>
      <div></div>
    </div>
    <b></b><b></b><b></b><b></b>
    <i id=i>
      <i>
        <b></b>
      </i>
    </i>
  )HTML");

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
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>progress { -webkit-appearance:none }</style>
    <progress></progress>
    <div></div><div></div><div></div><div></div><div></div><div></div>
  )HTML");

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
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=nohost></div><div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->SetInnerHTMLFromString("<div></div><div></div><div></div>");
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
                                          ":host(div) { background: green}"),
            kRuleSetInvalidationsScheduled);

  EXPECT_EQ(ScheduleInvalidationsForRules(*shadow_root,
                                          ":host(*) { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ":host(*) :hover { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationSlotted) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div id=host>
      <span slot=other class=s1></span>
      <span class=s2></span>
      <span class=s1></span>
      <span></span>
    </div>
  )HTML");

  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->SetInnerHTMLFromString("<slot name=other></slot><slot></slot>");
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
  GetDocument().body()->SetInnerHTMLFromString("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->SetInnerHTMLFromString(
      "<div></div><div class=a></div><div></div>");
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
  GetDocument().body()->SetInnerHTMLFromString("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->SetInnerHTMLFromString(
      "<div></div><div class=a></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(ScheduleInvalidationsForRules(
                *shadow_root, ".a ::content span { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, HasViewportDependentMediaQueries) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>div {}</style>
    <style id='sheet' media='(min-width: 200px)'>
      div {}
    </style>
  )HTML");

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
  GetDocument().body()->SetInnerHTMLFromString(
      "<style id='s1' media='(max-width: 1px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::HTMLNames::mediaAttr, "(max-width: 2000px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, StyleMediaAttributeNoStyleChange) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style id='s1' media='(max-width: 1000px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::HTMLNames::mediaAttr, "(max-width: 2000px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, ModifyStyleRuleMatchedPropertiesCache) {
  // Test that the MatchedPropertiesCache is cleared when a StyleRule is
  // modified. The MatchedPropertiesCache caches results based on
  // CSSPropertyValueSet pointers. When a mutable CSSPropertyValueSet is
  // modified, the pointer doesn't change, yet the declarations do.

  GetDocument().body()->SetInnerHTMLFromString(
      "<style id='s1'>#t1 { color: blue }</style>"
      "<div id='t1'>Green</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  CSSStyleSheet* sheet = ToCSSStyleSheet(GetDocument().StyleSheets().item(0));
  ASSERT_TRUE(sheet);
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(sheet->cssRules(exception_state));
  CSSStyleRule* style_rule =
      ToCSSStyleRule(sheet->cssRules(exception_state)->item(0));
  ASSERT_FALSE(exception_state.HadException());
  ASSERT_TRUE(style_rule);
  ASSERT_TRUE(style_rule->style());

  // Modify the CSSPropertyValueSet once to make it a mutable set. Subsequent
  // modifications will not change the CSSPropertyValueSet pointer and cache
  // hash value will be the same.
  style_rule->style()->setProperty(&GetDocument(), "color", "red", "",
                                   ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  style_rule->style()->setProperty(&GetDocument(), "color", "green", "",
                                   ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, ScheduleInvalidationAfterSubtreeRecalc) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style id='s1'>
      .t1 span { color: green }
      .t2 span { color: green }
    </style>
    <style id='s2'>div { background: lime }</style>
    <div id='t1'></div>
    <div id='t2'></div>
  )HTML");
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
  HTMLStyleElement* s2 = ToHTMLStyleElement(GetDocument().getElementById("s2"));
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
  HTMLStyleElement* s1 = ToHTMLStyleElement(GetDocument().getElementById("s1"));
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
  GetDocument().body()->SetInnerHTMLFromString("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  GetDocument().View()->UpdateAllLifecyclePhases();
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->SetInnerHTMLFromString(R"HTML(
    <style>
      span { color: green }
      t1 { color: green }
    </style>
    <div id='t1'></div>
    <span></span>
  )HTML");

  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyHttpEquivDefaultStyle) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>div { color:pink }</style><div id=container></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  container->SetInnerHTMLFromString(
      "<meta http-equiv='default-style' content=''>");
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  container->SetInnerHTMLFromString(
      "<meta http-equiv='default-style' content='preferred'>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_Document) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetDocument().body()->SetInnerHTMLFromString(
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
  GetDocument().body()->SetInnerHTMLFromString("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  GetDocument().View()->UpdateAllLifecyclePhases();
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host->attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         init, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(shadow_root);

  shadow_root->SetInnerHTMLFromString("<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(*shadow_root);
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  shadow_root->SetInnerHTMLFromString(
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

class StyleEngineClient : public FrameTestHelpers::TestWebViewClient {
 public:
  StyleEngineClient() : device_scale_factor_(1.f) {}
  void ConvertWindowToViewport(WebFloatRect* rect) override {
    rect->x *= device_scale_factor_;
    rect->y *= device_scale_factor_;
    rect->width *= device_scale_factor_;
    rect->height *= device_scale_factor_;
  }
  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

 private:
  float device_scale_factor_;
};

TEST_F(StyleEngineTest, ViewportDescriptionForZoomDSF) {
  StyleEngineClient client;
  client.set_device_scale_factor(1.f);

  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.Initialize(nullptr, &client, nullptr, nullptr);
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();

  float min_width =
      document->GetViewportDescription().min_width.GetFloatValue();
  float max_width =
      document->GetViewportDescription().max_width.GetFloatValue();
  float min_height =
      document->GetViewportDescription().min_height.GetFloatValue();
  float max_height =
      document->GetViewportDescription().max_height.GetFloatValue();

  const float device_scale = 3.5f;
  client.set_device_scale_factor(device_scale);
  web_view_impl->UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(device_scale * min_width,
                  document->GetViewportDescription().min_width.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * max_width,
                  document->GetViewportDescription().max_width.GetFloatValue());
  EXPECT_FLOAT_EQ(
      device_scale * min_height,
      document->GetViewportDescription().min_height.GetFloatValue());
  EXPECT_FLOAT_EQ(
      device_scale * max_height,
      document->GetViewportDescription().max_height.GetFloatValue());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementNoMedia) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNoValue) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaEmpty) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media=''>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

// TODO(futhark@chromium.org): The test cases below where all queries are either
// "all" or "not all", we could have detected those and not trigger an active
// stylesheet update for those cases.

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNoValid) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media=',,'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementMediaAll) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media='all'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNotAll) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media='not all'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementMediaType) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media='print'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

}  // namespace blink

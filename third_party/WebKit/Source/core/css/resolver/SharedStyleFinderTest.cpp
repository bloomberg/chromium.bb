// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/SharedStyleFinder.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/Document.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/ShadowRootInit.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SharedStyleFinderTest : public ::testing::Test {
 protected:
  SharedStyleFinderTest() = default;
  ~SharedStyleFinderTest() override = default;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  void SetBodyContent(const String& html) {
    GetDocument().body()->setInnerHTML(html);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  ShadowRoot& AttachShadow(Element& host) {
    ShadowRootInit init;
    init.setMode("open");
    ShadowRoot* shadow_root =
        host.attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                          init, ASSERT_NO_EXCEPTION);
    EXPECT_TRUE(shadow_root);
    return *shadow_root;
  }

  void AddSelector(const String& selector) {
    StyleRuleBase* new_rule =
        CSSParser::ParseRule(CSSParserContext::Create(kHTMLStandardMode),
                             nullptr, selector + "{color:pink}");
    rule_set_->AddStyleRule(static_cast<StyleRule*>(new_rule),
                            kRuleHasNoSpecialState);
  }

  void FinishAddingSelectors() {
    sibling_rule_set_ = MakeRuleSet(rule_set_->Features().SiblingRules());
    uncommon_attribute_rule_set_ =
        MakeRuleSet(rule_set_->Features().UncommonAttributeRules());
  }

  bool MatchesUncommonAttributeRuleSet(Element& element) {
    return MatchesRuleSet(element, uncommon_attribute_rule_set_);
  }

 private:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    rule_set_ = RuleSet::Create();
  }

  static RuleSet* MakeRuleSet(const HeapVector<RuleFeature>& rule_features) {
    if (rule_features.IsEmpty())
      return nullptr;
    RuleSet* rule_set = RuleSet::Create();
    for (auto rule_feature : rule_features)
      rule_set->AddRule(rule_feature.rule, rule_feature.selector_index,
                        kRuleHasNoSpecialState);
    return rule_set;
  }

  bool MatchesRuleSet(Element& element, RuleSet* rule_set) {
    if (!rule_set)
      return false;

    ElementResolveContext context(element);
    SharedStyleFinder finder(context, rule_set_->Features(), sibling_rule_set_,
                             uncommon_attribute_rule_set_,
                             GetDocument().EnsureStyleResolver());

    return finder.MatchesRuleSet(rule_set);
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<RuleSet> rule_set_;
  Persistent<RuleSet> sibling_rule_set_;
  Persistent<RuleSet> uncommon_attribute_rule_set_;
};

// Selectors which only fail matching :hover/:focus/:active/:-webkit-drag are
// considered as matching for matchesRuleSet because affectedBy bits need to be
// set correctly on ComputedStyle objects, hence ComputedStyle may not be shared
// with elements which would not have reached the pseudo classes during
// matching.

TEST_F(SharedStyleFinderTest, AttributeAffectedByHover) {
  SetBodyContent("<div id=a attr></div><div id=b></div>");

  AddSelector("[attr]:hover");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->IsHovered());
  EXPECT_FALSE(b->IsHovered());

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByHoverNegated) {
  SetBodyContent("<div id=a attr></div><div id=b></div>");

  AddSelector("[attr]:not(:hover)");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->IsHovered());
  EXPECT_FALSE(b->IsHovered());

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByFocus) {
  SetBodyContent("<div id=a attr></div><div id=b></div>");

  AddSelector("[attr]:focus");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->IsFocused());
  EXPECT_FALSE(b->IsFocused());

  // :focus rules do not end up in uncommon attribute rule sets. Style sharing
  // is skipped for focused elements in Element::SupportsStyleSharing().
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByFocusWithin) {
  SetBodyContent("<div id=a attr></div><div id=b></div>");

  AddSelector("[attr]:focus-within");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->HasFocusWithin());
  EXPECT_FALSE(b->HasFocusWithin());

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByActive) {
  SetBodyContent("<div id=a attr></div><div id=b></div>");

  AddSelector("[attr]:active");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->IsActive());
  EXPECT_FALSE(b->IsActive());

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByDrag) {
  SetBodyContent("<div id=a attr></div><div id=b></div>");

  AddSelector("[attr]:-webkit-drag");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->IsDragged());
  EXPECT_FALSE(b->IsDragged());

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, SlottedPseudoWithAttribute) {
  SetBodyContent("<div id=host><div id=a></div><div id=b attr></div></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<slot></slot>");
  GetDocument().UpdateDistribution();

  AddSelector("::slotted([attr])");
  FinishAddingSelectors();

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");

  EXPECT_TRUE(a->AssignedSlot());
  EXPECT_TRUE(b->AssignedSlot());

  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*a));
  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, HostWithAttribute) {
  SetBodyContent("<div id=host attr></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<slot></slot>");
  GetDocument().UpdateDistribution();

  AddSelector(":host([attr])");
  FinishAddingSelectors();

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*host));
}

TEST_F(SharedStyleFinderTest, HostAncestorWithAttribute) {
  SetBodyContent("<div id=host attr></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<div id=inner></div>");
  Element* inner = root.getElementById("inner");
  GetDocument().UpdateDistribution();

  AddSelector(":host([attr]) div");
  FinishAddingSelectors();

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*inner));
}

TEST_F(SharedStyleFinderTest, HostContextAncestorWithAttribute) {
  SetBodyContent("<div id=host attr></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<div id=inner></div>");
  Element* inner = root.getElementById("inner");
  GetDocument().UpdateDistribution();

  AddSelector(":host-context([attr]) div");
  FinishAddingSelectors();

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*inner));
}

TEST_F(SharedStyleFinderTest, HostParentWithAttribute) {
  SetBodyContent("<div id=host attr></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<div id=inner></div>");
  Element* inner = root.getElementById("inner");
  GetDocument().UpdateDistribution();

  AddSelector(":host([attr]) > div");
  FinishAddingSelectors();

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*inner));
}

TEST_F(SharedStyleFinderTest, HostContextParentWithAttribute) {
  SetBodyContent("<div id=host attr></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<div id=inner></div>");
  Element* inner = root.getElementById("inner");
  GetDocument().UpdateDistribution();

  AddSelector(":host-context([attr]) > div");
  FinishAddingSelectors();

  EXPECT_TRUE(MatchesUncommonAttributeRuleSet(*inner));
}

TEST_F(SharedStyleFinderTest, AncestorWithAttributeInParentScope) {
  SetBodyContent("<div id=host attr></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = AttachShadow(*host);
  root.setInnerHTML("<div id=inner></div>");
  Element* inner = root.getElementById("inner");
  GetDocument().UpdateDistribution();

  AddSelector("div[attr] div");
  FinishAddingSelectors();

  EXPECT_FALSE(MatchesUncommonAttributeRuleSet(*inner));
}

}  // namespace blink

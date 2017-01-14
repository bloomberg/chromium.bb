// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/SharedStyleFinder.h"

#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class SharedStyleFinderTest : public ::testing::Test {
 protected:
  SharedStyleFinderTest() = default;
  ~SharedStyleFinderTest() override = default;

  Document& document() { return m_dummyPageHolder->document(); }

  void setBodyContent(const String& html) {
    document().body()->setInnerHTML(html);
    document().view()->updateAllLifecyclePhases();
  }

  ShadowRoot& attachShadow(Element& host) {
    ShadowRootInit init;
    init.setMode("open");
    ShadowRoot* shadowRoot =
        host.attachShadow(ScriptState::forMainWorld(document().frame()), init,
                          ASSERT_NO_EXCEPTION);
    EXPECT_TRUE(shadowRoot);
    return *shadowRoot;
  }

  void addSelector(const String& selector) {
    StyleRuleBase* newRule =
        CSSParser::parseRule(CSSParserContext::create(HTMLStandardMode),
                             nullptr, selector + "{color:pink}");
    m_ruleSet->addStyleRule(static_cast<StyleRule*>(newRule),
                            RuleHasNoSpecialState);
  }

  void finishAddingSelectors() {
    m_siblingRuleSet = makeRuleSet(m_ruleSet->features().siblingRules());
    m_uncommonAttributeRuleSet =
        makeRuleSet(m_ruleSet->features().uncommonAttributeRules());
  }

  bool matchesUncommonAttributeRuleSet(Element& element) {
    return matchesRuleSet(element, m_uncommonAttributeRuleSet);
  }

 private:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_ruleSet = RuleSet::create();
  }

  static RuleSet* makeRuleSet(const HeapVector<RuleFeature>& ruleFeatures) {
    if (ruleFeatures.isEmpty())
      return nullptr;
    RuleSet* ruleSet = RuleSet::create();
    for (auto ruleFeature : ruleFeatures)
      ruleSet->addRule(ruleFeature.rule, ruleFeature.selectorIndex,
                       RuleHasNoSpecialState);
    return ruleSet;
  }

  bool matchesRuleSet(Element& element, RuleSet* ruleSet) {
    if (!ruleSet)
      return false;

    ElementResolveContext context(element);
    SharedStyleFinder finder(context, m_ruleSet->features(), m_siblingRuleSet,
                             m_uncommonAttributeRuleSet,
                             document().ensureStyleResolver());

    return finder.matchesRuleSet(ruleSet);
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<RuleSet> m_ruleSet;
  Persistent<RuleSet> m_siblingRuleSet;
  Persistent<RuleSet> m_uncommonAttributeRuleSet;
};

// Selectors which only fail matching :hover/:focus/:active/:-webkit-drag are
// considered as matching for matchesRuleSet because affectedBy bits need to be
// set correctly on ComputedStyle objects, hence ComputedStyle may not be shared
// with elements which would not have reached the pseudo classes during
// matching.

TEST_F(SharedStyleFinderTest, AttributeAffectedByHover) {
  setBodyContent("<div id=a attr></div><div id=b></div>");

  addSelector("[attr]:hover");
  finishAddingSelectors();

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->isHovered());
  EXPECT_FALSE(b->isHovered());

  EXPECT_TRUE(matchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(matchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByHoverNegated) {
  setBodyContent("<div id=a attr></div><div id=b></div>");

  addSelector("[attr]:not(:hover)");
  finishAddingSelectors();

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->isHovered());
  EXPECT_FALSE(b->isHovered());

  EXPECT_TRUE(matchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(matchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByFocus) {
  setBodyContent("<div id=a attr></div><div id=b></div>");

  addSelector("[attr]:focus");
  finishAddingSelectors();

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->isFocused());
  EXPECT_FALSE(b->isFocused());

  EXPECT_TRUE(matchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(matchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByActive) {
  setBodyContent("<div id=a attr></div><div id=b></div>");

  addSelector("[attr]:active");
  finishAddingSelectors();

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->isActive());
  EXPECT_FALSE(b->isActive());

  EXPECT_TRUE(matchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(matchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, AttributeAffectedByDrag) {
  setBodyContent("<div id=a attr></div><div id=b></div>");

  addSelector("[attr]:-webkit-drag");
  finishAddingSelectors();

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_FALSE(a->isDragged());
  EXPECT_FALSE(b->isDragged());

  EXPECT_TRUE(matchesUncommonAttributeRuleSet(*a));
  EXPECT_FALSE(matchesUncommonAttributeRuleSet(*b));
}

TEST_F(SharedStyleFinderTest, SlottedPseudoWithAttribute) {
  setBodyContent("<div id=host><div id=a></div><div id=b attr></div></div>");
  Element* host = document().getElementById("host");
  ShadowRoot& root = attachShadow(*host);
  root.setInnerHTML("<slot></slot>");
  document().updateDistribution();

  addSelector("::slotted([attr])");
  finishAddingSelectors();

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");

  EXPECT_TRUE(a->assignedSlot());
  EXPECT_TRUE(b->assignedSlot());

  EXPECT_FALSE(matchesUncommonAttributeRuleSet(*a));
  EXPECT_TRUE(matchesUncommonAttributeRuleSet(*b));
}

}  // namespace blink

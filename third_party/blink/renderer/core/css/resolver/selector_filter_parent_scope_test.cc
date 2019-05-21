// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class SelectorFilterParentScopeTest : public testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  static constexpr size_t max_identifier_hashes = 4;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(SelectorFilterParentScopeTest, ParentScope) {
  GetDocument().body()->setAttribute(html_names::kClassAttr, "match");
  GetDocument().documentElement()->SetIdAttribute("myId");
  SelectorFilter& filter =
      GetDocument().EnsureStyleResolver().GetSelectorFilter();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  SelectorFilterParentScope html_scope(*GetDocument().documentElement());
  {
    SelectorFilterParentScope body_scope(*GetDocument().body());
    SelectorFilterParentScope::EnsureParentStackIsPushed();

    CSSSelectorList selectors = CSSParser::ParseSelector(
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext),
        nullptr, "html, body, .match, #myId");

    for (const CSSSelector* selector = selectors.First(); selector;
         selector = CSSSelectorList::Next(*selector)) {
      unsigned selector_hashes[max_identifier_hashes];
      filter.CollectIdentifierHashes(*selector, selector_hashes,
                                     max_identifier_hashes);
      EXPECT_FALSE(
          filter.FastRejectSelector<max_identifier_hashes>(selector_hashes));
    }
  }
}

TEST_F(SelectorFilterParentScopeTest, AncestorScope) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div class=x>
      <span id=y></span>
    </div>
  )HTML");
  SelectorFilter& filter =
      GetDocument().EnsureStyleResolver().GetSelectorFilter();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  SelectorFilterAncestorScope span_scope(*GetDocument().getElementById("y"));
  SelectorFilterParentScope::EnsureParentStackIsPushed();

  CSSSelectorList selectors = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext),
      nullptr, "html, body, div, span, .x, #y");

  for (const CSSSelector* selector = selectors.First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    unsigned selector_hashes[max_identifier_hashes];
    filter.CollectIdentifierHashes(*selector, selector_hashes,
                                   max_identifier_hashes);
    EXPECT_FALSE(
        filter.FastRejectSelector<max_identifier_hashes>(selector_hashes));
  }
}

}  // namespace blink

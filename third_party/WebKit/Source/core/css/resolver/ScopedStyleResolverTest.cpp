// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/ScopedStyleResolver.h"

#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ScopedStyleResolverTest : public ::testing::Test {
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

ShadowRoot& ScopedStyleResolverTest::attachShadow(Element& host) {
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadowRoot = host.attachShadow(
      ScriptState::forMainWorld(document().frame()), init, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(shadowRoot);
  return *shadowRoot;
}

TEST_F(ScopedStyleResolverTest, HasSameStylesNullNull) {
  EXPECT_TRUE(ScopedStyleResolver::haveSameStyles(nullptr, nullptr));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesNullEmpty) {
  ScopedStyleResolver& resolver = document().ensureScopedStyleResolver();
  EXPECT_TRUE(ScopedStyleResolver::haveSameStyles(nullptr, &resolver));
  EXPECT_TRUE(ScopedStyleResolver::haveSameStyles(&resolver, nullptr));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesEmptyEmpty) {
  ScopedStyleResolver& resolver = document().ensureScopedStyleResolver();
  EXPECT_TRUE(ScopedStyleResolver::haveSameStyles(&resolver, &resolver));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesNonEmpty) {
  document().body()->setInnerHTML("<div id=host1></div><div id=host2></div>");
  Element* host1 = document().getElementById("host1");
  Element* host2 = document().getElementById("host2");
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  ShadowRoot& root1 = attachShadow(*host1);
  ShadowRoot& root2 = attachShadow(*host2);
  root1.setInnerHTML("<style>::slotted(#dummy){color:pink}</style>");
  root2.setInnerHTML("<style>::slotted(#dummy){color:pink}</style>");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(ScopedStyleResolver::haveSameStyles(
      &root1.ensureScopedStyleResolver(), &root2.ensureScopedStyleResolver()));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesDifferentSheetCount) {
  document().body()->setInnerHTML("<div id=host1></div><div id=host2></div>");
  Element* host1 = document().getElementById("host1");
  Element* host2 = document().getElementById("host2");
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  ShadowRoot& root1 = attachShadow(*host1);
  ShadowRoot& root2 = attachShadow(*host2);
  root1.setInnerHTML(
      "<style>::slotted(#dummy){color:pink}</style><style>div{}</style>");
  root2.setInnerHTML("<style>::slotted(#dummy){color:pink}</style>");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(ScopedStyleResolver::haveSameStyles(
      &root1.ensureScopedStyleResolver(), &root2.ensureScopedStyleResolver()));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesCacheMiss) {
  document().body()->setInnerHTML("<div id=host1></div><div id=host2></div>");
  Element* host1 = document().getElementById("host1");
  Element* host2 = document().getElementById("host2");
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  ShadowRoot& root1 = attachShadow(*host1);
  ShadowRoot& root2 = attachShadow(*host2);
  // Style equality is detected when StyleSheetContents is shared. That is only
  // the case when the source text is the same. The comparison will fail when
  // adding an extra space to one of the sheets.
  root1.setInnerHTML("<style>::slotted(#dummy){color:pink}</style>");
  root2.setInnerHTML("<style>::slotted(#dummy){ color:pink}</style>");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(ScopedStyleResolver::haveSameStyles(
      &root1.ensureScopedStyleResolver(), &root2.ensureScopedStyleResolver()));
}

}  // namespace blink

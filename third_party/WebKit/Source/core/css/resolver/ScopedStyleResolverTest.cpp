// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/ScopedStyleResolver.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/ShadowRootInit.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ScopedStyleResolverTest : public PageTestBase {
 protected:
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }
  ShadowRoot& AttachShadow(Element& host);
};

ShadowRoot& ScopedStyleResolverTest::AttachShadow(Element& host) {
  ShadowRootInit init;
  init.setMode("open");
  ShadowRoot* shadow_root =
      host.attachShadow(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                        init, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(shadow_root);
  return *shadow_root;
}

TEST_F(ScopedStyleResolverTest, HasSameStylesNullNull) {
  EXPECT_TRUE(ScopedStyleResolver::HaveSameStyles(nullptr, nullptr));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesNullEmpty) {
  ScopedStyleResolver& resolver = GetDocument().EnsureScopedStyleResolver();
  EXPECT_TRUE(ScopedStyleResolver::HaveSameStyles(nullptr, &resolver));
  EXPECT_TRUE(ScopedStyleResolver::HaveSameStyles(&resolver, nullptr));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesEmptyEmpty) {
  ScopedStyleResolver& resolver = GetDocument().EnsureScopedStyleResolver();
  EXPECT_TRUE(ScopedStyleResolver::HaveSameStyles(&resolver, &resolver));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesNonEmpty) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=host1></div><div id=host2></div>");
  Element* host1 = GetDocument().getElementById("host1");
  Element* host2 = GetDocument().getElementById("host2");
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  ShadowRoot& root1 = AttachShadow(*host1);
  ShadowRoot& root2 = AttachShadow(*host2);
  root1.SetInnerHTMLFromString("<style>::slotted(#dummy){color:pink}</style>");
  root2.SetInnerHTMLFromString("<style>::slotted(#dummy){color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(ScopedStyleResolver::HaveSameStyles(
      &root1.EnsureScopedStyleResolver(), &root2.EnsureScopedStyleResolver()));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesDifferentSheetCount) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=host1></div><div id=host2></div>");
  Element* host1 = GetDocument().getElementById("host1");
  Element* host2 = GetDocument().getElementById("host2");
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  ShadowRoot& root1 = AttachShadow(*host1);
  ShadowRoot& root2 = AttachShadow(*host2);
  root1.SetInnerHTMLFromString(
      "<style>::slotted(#dummy){color:pink}</style><style>div{}</style>");
  root2.SetInnerHTMLFromString("<style>::slotted(#dummy){color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ScopedStyleResolver::HaveSameStyles(
      &root1.EnsureScopedStyleResolver(), &root2.EnsureScopedStyleResolver()));
}

TEST_F(ScopedStyleResolverTest, HasSameStylesCacheMiss) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=host1></div><div id=host2></div>");
  Element* host1 = GetDocument().getElementById("host1");
  Element* host2 = GetDocument().getElementById("host2");
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  ShadowRoot& root1 = AttachShadow(*host1);
  ShadowRoot& root2 = AttachShadow(*host2);
  // Style equality is detected when StyleSheetContents is shared. That is only
  // the case when the source text is the same. The comparison will fail when
  // adding an extra space to one of the sheets.
  root1.SetInnerHTMLFromString("<style>::slotted(#dummy){color:pink}</style>");
  root2.SetInnerHTMLFromString("<style>::slotted(#dummy){ color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ScopedStyleResolver::HaveSameStyles(
      &root1.EnsureScopedStyleResolver(), &root2.EnsureScopedStyleResolver()));
}

}  // namespace blink

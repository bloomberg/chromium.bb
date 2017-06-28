// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementShadow.h"
#include "core/dom/ElementShadowV0.h"
#include "core/html/HTMLBodyElement.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"

namespace blink {

namespace {

bool HasSelectorForIdInShadow(Element* host, const AtomicString& id) {
  DCHECK(host);
  return host->Shadow()->V0().EnsureSelectFeatureSet().HasSelectorForId(id);
}

bool HasSelectorForClassInShadow(Element* host,
                                 const AtomicString& class_name) {
  DCHECK(host);
  return host->Shadow()->V0().EnsureSelectFeatureSet().HasSelectorForClass(
      class_name);
}

bool HasSelectorForAttributeInShadow(Element* host,
                                     const AtomicString& attribute_name) {
  DCHECK(host);
  return host->Shadow()->V0().EnsureSelectFeatureSet().HasSelectorForAttribute(
      attribute_name);
}

class ShadowDOMVTest : public SimTest {};

TEST_F(ShadowDOMVTest, FeatureSetId) {
  LoadURL("about:blank");
  auto* host = GetDocument().createElement("div");
  auto* content = GetDocument().createElement("content");
  content->setAttribute("select", "#foo");
  host->createShadowRoot()->AppendChild(content);
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "host"));
  content->setAttribute("select", "#bar");
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "foo"));
}

TEST_F(ShadowDOMVTest, FeatureSetClassName) {
  LoadURL("about:blank");
  auto* host = GetDocument().createElement("div");
  auto* content = GetDocument().createElement("content");
  content->setAttribute("select", ".foo");
  host->createShadowRoot()->AppendChild(content);
  EXPECT_TRUE(HasSelectorForClassInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "host"));
  content->setAttribute("select", ".bar");
  EXPECT_TRUE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
}

TEST_F(ShadowDOMVTest, FeatureSetAttributeName) {
  LoadURL("about:blank");
  auto* host = GetDocument().createElement("div");
  auto* content = GetDocument().createElement("content");
  content->setAttribute("select", "div[foo]");
  host->createShadowRoot()->AppendChild(content);
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "host"));
  content->setAttribute("select", "div[bar]");
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "foo"));
}

TEST_F(ShadowDOMVTest, FeatureSetMultipleSelectors) {
  LoadURL("about:blank");
  auto* host = GetDocument().createElement("div");
  auto* content = GetDocument().createElement("content");
  content->setAttribute("select", "#foo,.bar,div[baz]");
  host->createShadowRoot()->AppendChild(content);
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "baz"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
  EXPECT_TRUE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "baz"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "baz"));
}

TEST_F(ShadowDOMVTest, FeatureSetSubtree) {
  LoadURL("about:blank");
  auto* host = GetDocument().createElement("div");
  host->createShadowRoot()->setInnerHTML(
      "<div>"
      "  <div></div>"
      "  <content select='*'></content>"
      "  <div>"
      "    <content select='div[foo=piyo]'></content>"
      "  </div>"
      "</div>");
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "piyo"));
}

TEST_F(ShadowDOMVTest, FeatureSetMultipleShadowRoots) {
  LoadURL("about:blank");
  auto* host = GetDocument().createElement("div");
  auto* host_shadow = host->createShadowRoot();
  host_shadow->setInnerHTML("<content select='#foo'></content>");
  auto* child = GetDocument().createElement("div");
  auto* child_root = child->createShadowRoot();
  auto* child_content = GetDocument().createElement("content");
  child_content->setAttribute("select", "#bar");
  child_root->AppendChild(child_content);
  host_shadow->AppendChild(child);
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "baz"));
  child_content->setAttribute("select", "#baz");
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "baz"));
}

}  // namespace

}  // namespace blink

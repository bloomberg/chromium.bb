// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ElementShadowV0.h"
#include "core/html/HTMLBodyElement.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

namespace {

bool hasSelectorForIdInShadow(Element* host, const AtomicString& id) {
  DCHECK(host);
  return host->shadow()->v0().ensureSelectFeatureSet().hasSelectorForId(id);
}

bool hasSelectorForClassInShadow(Element* host, const AtomicString& className) {
  DCHECK(host);
  return host->shadow()->v0().ensureSelectFeatureSet().hasSelectorForClass(
      className);
}

bool hasSelectorForAttributeInShadow(Element* host,
                                     const AtomicString& attributeName) {
  DCHECK(host);
  return host->shadow()->v0().ensureSelectFeatureSet().hasSelectorForAttribute(
      attributeName);
}

class ShadowDOMVTest : public SimTest {};

TEST_F(ShadowDOMVTest, FeatureSetId) {
  loadURL("about:blank");
  auto* host = document().createElement("div");
  auto* content = document().createElement("content");
  content->setAttribute("select", "#foo");
  host->createShadowRoot()->appendChild(content);
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "host"));
  content->setAttribute("select", "#bar");
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "foo"));
}

TEST_F(ShadowDOMVTest, FeatureSetClassName) {
  loadURL("about:blank");
  auto* host = document().createElement("div");
  auto* content = document().createElement("content");
  content->setAttribute("select", ".foo");
  host->createShadowRoot()->appendChild(content);
  EXPECT_TRUE(hasSelectorForClassInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "host"));
  content->setAttribute("select", ".bar");
  EXPECT_TRUE(hasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "foo"));
}

TEST_F(ShadowDOMVTest, FeatureSetAttributeName) {
  loadURL("about:blank");
  auto* host = document().createElement("div");
  auto* content = document().createElement("content");
  content->setAttribute("select", "div[foo]");
  host->createShadowRoot()->appendChild(content);
  EXPECT_TRUE(hasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "host"));
  content->setAttribute("select", "div[bar]");
  EXPECT_TRUE(hasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "foo"));
}

TEST_F(ShadowDOMVTest, FeatureSetMultipleSelectors) {
  loadURL("about:blank");
  auto* host = document().createElement("div");
  auto* content = document().createElement("content");
  content->setAttribute("select", "#foo,.bar,div[baz]");
  host->createShadowRoot()->appendChild(content);
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "baz"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "foo"));
  EXPECT_TRUE(hasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "baz"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_TRUE(hasSelectorForAttributeInShadow(host, "baz"));
}

TEST_F(ShadowDOMVTest, FeatureSetSubtree) {
  loadURL("about:blank");
  auto* host = document().createElement("div");
  host->createShadowRoot()->setInnerHTML(
      "<div>"
      "  <div></div>"
      "  <content select='*'></content>"
      "  <div>"
      "    <content select='div[foo=piyo]'></content>"
      "  </div>"
      "</div>");
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForClassInShadow(host, "foo"));
  EXPECT_TRUE(hasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForAttributeInShadow(host, "piyo"));
}

TEST_F(ShadowDOMVTest, FeatureSetMultipleShadowRoots) {
  loadURL("about:blank");
  auto* host = document().createElement("div");
  auto* hostShadow = host->createShadowRoot();
  hostShadow->setInnerHTML("<content select='#foo'></content>");
  auto* child = document().createElement("div");
  auto* childRoot = child->createShadowRoot();
  auto* childContent = document().createElement("content");
  childContent->setAttribute("select", "#bar");
  childRoot->appendChild(childContent);
  hostShadow->appendChild(child);
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "foo"));
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "baz"));
  childContent->setAttribute("select", "#baz");
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(hasSelectorForIdInShadow(host, "bar"));
  EXPECT_TRUE(hasSelectorForIdInShadow(host, "baz"));
}

}  // namespace

}  // namespace blink

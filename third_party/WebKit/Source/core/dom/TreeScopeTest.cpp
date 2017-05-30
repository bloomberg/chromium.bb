// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TreeScope.h"

#include "bindings/core/v8/StringOrDictionary.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TreeScopeTest, CommonAncestorOfSameTrees) {
  Document* document = Document::Create();
  EXPECT_EQ(document, document->CommonAncestorTreeScope(*document));

  Element* html = document->createElement("html", StringOrDictionary());
  document->AppendChild(html);
  ShadowRoot* shadow_root =
      html->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(shadow_root, shadow_root->CommonAncestorTreeScope(*shadow_root));
}

TEST(TreeScopeTest, CommonAncestorOfInclusiveTrees) {
  //  document
  //     |      : Common ancestor is document.
  // shadowRoot

  Document* document = Document::Create();
  Element* html = document->createElement("html", StringOrDictionary());
  document->AppendChild(html);
  ShadowRoot* shadow_root =
      html->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(document, document->CommonAncestorTreeScope(*shadow_root));
  EXPECT_EQ(document, shadow_root->CommonAncestorTreeScope(*document));
}

TEST(TreeScopeTest, CommonAncestorOfSiblingTrees) {
  //  document
  //   /    \  : Common ancestor is document.
  //  A      B

  Document* document = Document::Create();
  Element* html = document->createElement("html", StringOrDictionary());
  document->AppendChild(html);
  Element* head = document->createElement("head", StringOrDictionary());
  html->AppendChild(head);
  Element* body = document->createElement("body", StringOrDictionary());
  html->AppendChild(body);

  ShadowRoot* shadow_root_a =
      head->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
  ShadowRoot* shadow_root_b =
      body->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(document, shadow_root_a->CommonAncestorTreeScope(*shadow_root_b));
  EXPECT_EQ(document, shadow_root_b->CommonAncestorTreeScope(*shadow_root_a));
}

TEST(TreeScopeTest, CommonAncestorOfTreesAtDifferentDepths) {
  //  document
  //    / \    : Common ancestor is document.
  //   Y   B
  //  /
  // A

  Document* document = Document::Create();
  Element* html = document->createElement("html", StringOrDictionary());
  document->AppendChild(html);
  Element* head = document->createElement("head", StringOrDictionary());
  html->AppendChild(head);
  Element* body = document->createElement("body", StringOrDictionary());
  html->AppendChild(body);

  ShadowRoot* shadow_root_y =
      head->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
  ShadowRoot* shadow_root_b =
      body->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

  Element* div_in_y = document->createElement("div", StringOrDictionary());
  shadow_root_y->AppendChild(div_in_y);
  ShadowRoot* shadow_root_a = div_in_y->CreateShadowRootInternal(
      ShadowRootType::V0, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(document, shadow_root_a->CommonAncestorTreeScope(*shadow_root_b));
  EXPECT_EQ(document, shadow_root_b->CommonAncestorTreeScope(*shadow_root_a));
}

TEST(TreeScopeTest, CommonAncestorOfTreesInDifferentDocuments) {
  Document* document1 = Document::Create();
  Document* document2 = Document::Create();
  EXPECT_EQ(0, document1->CommonAncestorTreeScope(*document2));
  EXPECT_EQ(0, document2->CommonAncestorTreeScope(*document1));
}

}  // namespace blink

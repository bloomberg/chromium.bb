// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TreeScopeAdopter.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ShadowRoot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TreeScopeAdopterTest, SimpleMove) {
  Document* doc1 = Document::CreateForTest();
  Document* doc2 = Document::CreateForTest();

  Element* html1 = doc1->createElement("html");
  doc1->AppendChild(html1);
  Element* div1 = doc1->createElement("div");
  html1->AppendChild(div1);

  Element* html2 = doc2->createElement("html");
  doc2->AppendChild(html2);
  Element* div2 = doc1->createElement("div");
  html2->AppendChild(div2);

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter1(*div1, *doc1);
  EXPECT_FALSE(adopter1.NeedsScopeChange());

  TreeScopeAdopter adopter2(*div2, *doc1);
  ASSERT_TRUE(adopter2.NeedsScopeChange());

  adopter2.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
}

TEST(TreeScopeAdopterTest, AdoptV1ShadowRootToV0Document) {
  Document* doc1 = Document::CreateForTest();
  Document* doc2 = Document::CreateForTest();

  Element* html1 = doc1->createElement("html");
  doc1->AppendChild(html1);
  Element* div1 = doc1->createElement("div");
  html1->AppendChild(div1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeNone);
  div1->CreateShadowRootInternal();
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV0);
  EXPECT_TRUE(doc1->MayContainV0Shadow());

  Element* html2 = doc2->createElement("html");
  doc2->AppendChild(html2);
  Element* div2 = doc1->createElement("div");
  html2->AppendChild(div2);
  div2->AttachShadowRootInternal(ShadowRootType::kOpen);

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter(*div2, *doc1);
  ASSERT_TRUE(adopter.NeedsScopeChange());

  adopter.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV1);
  EXPECT_TRUE(doc1->MayContainV0Shadow());
  EXPECT_FALSE(doc2->MayContainV0Shadow());
}

TEST(TreeScopeAdopterTest, AdoptV0ShadowRootToV1Document) {
  Document* doc1 = Document::CreateForTest();
  Document* doc2 = Document::CreateForTest();

  Element* html1 = doc1->createElement("html");
  doc1->AppendChild(html1);
  Element* div1 = doc1->createElement("div");
  html1->AppendChild(div1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeNone);
  div1->AttachShadowRootInternal(ShadowRootType::kOpen);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV1);
  EXPECT_FALSE(doc1->MayContainV0Shadow());

  Element* html2 = doc2->createElement("html");
  doc2->AppendChild(html2);
  Element* div2 = doc1->createElement("div");
  html2->AppendChild(div2);
  div2->CreateShadowRootInternal();

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter(*div2, *doc1);
  ASSERT_TRUE(adopter.NeedsScopeChange());

  adopter.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV1);
  EXPECT_TRUE(doc1->MayContainV0Shadow());
  EXPECT_TRUE(doc2->MayContainV0Shadow());
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TreeScope.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TreeScopeTest, CommonAncestorOfSameTrees)
{
    RawPtr<Document> document = Document::create();
    EXPECT_EQ(document.get(), document->commonAncestorTreeScope(*document));

    RawPtr<Element> html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    RawPtr<ShadowRoot> shadowRoot = html->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(shadowRoot.get(), shadowRoot->commonAncestorTreeScope(*shadowRoot));
}

TEST(TreeScopeTest, CommonAncestorOfInclusiveTrees)
{
    //  document
    //     |      : Common ancestor is document.
    // shadowRoot

    RawPtr<Document> document = Document::create();
    RawPtr<Element> html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    RawPtr<ShadowRoot> shadowRoot = html->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    EXPECT_EQ(document.get(), document->commonAncestorTreeScope(*shadowRoot));
    EXPECT_EQ(document.get(), shadowRoot->commonAncestorTreeScope(*document));
}

TEST(TreeScopeTest, CommonAncestorOfSiblingTrees)
{
    //  document
    //   /    \  : Common ancestor is document.
    //  A      B

    RawPtr<Document> document = Document::create();
    RawPtr<Element> html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    RawPtr<Element> head = document->createElement("head", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(head);
    RawPtr<Element> body = document->createElement("body", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(body);

    RawPtr<ShadowRoot> shadowRootA = head->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    RawPtr<ShadowRoot> shadowRootB = body->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    EXPECT_EQ(document.get(), shadowRootA->commonAncestorTreeScope(*shadowRootB));
    EXPECT_EQ(document.get(), shadowRootB->commonAncestorTreeScope(*shadowRootA));
}

TEST(TreeScopeTest, CommonAncestorOfTreesAtDifferentDepths)
{
    //  document
    //    / \    : Common ancestor is document.
    //   Y   B
    //  /
    // A

    RawPtr<Document> document = Document::create();
    RawPtr<Element> html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    RawPtr<Element> head = document->createElement("head", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(head);
    RawPtr<Element> body = document->createElement("body", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(body);

    RawPtr<ShadowRoot> shadowRootY = head->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    RawPtr<ShadowRoot> shadowRootB = body->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    RawPtr<Element> divInY = document->createElement("div", nullAtom, ASSERT_NO_EXCEPTION);
    shadowRootY->appendChild(divInY);
    RawPtr<ShadowRoot> shadowRootA = divInY->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    EXPECT_EQ(document.get(), shadowRootA->commonAncestorTreeScope(*shadowRootB));
    EXPECT_EQ(document.get(), shadowRootB->commonAncestorTreeScope(*shadowRootA));
}

TEST(TreeScopeTest, CommonAncestorOfTreesInDifferentDocuments)
{
    RawPtr<Document> document1 = Document::create();
    RawPtr<Document> document2 = Document::create();
    EXPECT_EQ(0, document1->commonAncestorTreeScope(*document2));
    EXPECT_EQ(0, document2->commonAncestorTreeScope(*document1));
}

} // namespace blink

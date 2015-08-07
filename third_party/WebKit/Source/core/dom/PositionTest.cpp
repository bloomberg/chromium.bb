// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/Position.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class PositionTest : public EditingTestBase {
};

TEST_F(PositionTest, downstreamAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");

    EXPECT_EQ(Position::lastPositionInNode(host.get()), Position::afterNode(host.get()).downstream());
    EXPECT_EQ(PositionInComposedTree::lastPositionInNode(host.get()), PositionInComposedTree::afterNode(host.get()).downstream());
}

TEST_F(PositionTest, NextNodeIndex)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
    Node* host = document().getElementById("host");
    Node* two = document().getElementById("two");

    EXPECT_EQ(Position(host, 3), Position(two, 2).next());
    EXPECT_EQ(PositionInComposedTree(host, 1), PositionInComposedTree(two, 2).next());
}

TEST_F(PositionTest, NodeAsRangeLastNodeNull)
{
    EXPECT_EQ(nullptr, Position().nodeAsRangeLastNode());
    EXPECT_EQ(nullptr, PositionInComposedTree().nodeAsRangeLastNode());
}

TEST_F(PositionTest, NodeAsRangeLastNode)
{
    const char* bodyContent = "<p id='p1'>11</p><p id='p2'></p><p id='p3'>33</p>";
    setBodyContent(bodyContent);
    Node* p1 = document().getElementById("p1");
    Node* p2 = document().getElementById("p2");
    Node* p3 = document().getElementById("p3");
    Node* body = EditingStrategy::parent(*p1);
    Node* t1 = EditingStrategy::firstChild(*p1);
    Node* t3 = EditingStrategy::firstChild(*p3);

    EXPECT_EQ(body, Position::inParentBeforeNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentBeforeNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(p2, Position::inParentBeforeNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentAfterNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(p2, Position::inParentAfterNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(t3, Position::inParentAfterNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t3, Position::afterNode(p3).nodeAsRangeLastNode());

    EXPECT_EQ(body, PositionInComposedTree::inParentBeforeNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::inParentBeforeNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(p2, PositionInComposedTree::inParentBeforeNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::inParentAfterNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(p2, PositionInComposedTree::inParentAfterNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(t3, PositionInComposedTree::inParentAfterNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t3, PositionInComposedTree::afterNode(p3).nodeAsRangeLastNode());
}

TEST_F(PositionTest, NodeAsRangeLastNodeShadow)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a id='a'><content select=#two></content><content select=#one></content></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    Node* host = document().getElementById("host");
    Node* n1 = document().getElementById("one");
    Node* n2 = document().getElementById("two");
    Node* t0 = EditingStrategy::firstChild(*host);
    Node* t1 = EditingStrategy::firstChild(*n1);
    Node* t2 = EditingStrategy::firstChild(*n2);
    Node* t3 = EditingStrategy::lastChild(*host);
    Node* a = shadowRoot->getElementById("a");

    EXPECT_EQ(t0, Position::inParentBeforeNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentBeforeNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentAfterNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(t2, Position::inParentAfterNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t3, Position::afterNode(host).nodeAsRangeLastNode());

    EXPECT_EQ(t2, PositionInComposedTree::inParentBeforeNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(a, PositionInComposedTree::inParentBeforeNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::inParentAfterNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(t2, PositionInComposedTree::inParentAfterNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::afterNode(host).nodeAsRangeLastNode());
}

TEST_F(PositionTest, rendersInDifferentPositionAfterAnchor)
{
    const char* bodyContent = "<p id='sample'>00</p>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();
    RefPtrWillBeRawPtr<Element> sample = document().getElementById("sample");

    EXPECT_FALSE(Position::afterNode(sample.get()).rendersInDifferentPosition(Position(sample.get(), 1)));
    EXPECT_FALSE(Position::lastPositionInNode(sample.get()).rendersInDifferentPosition(Position(sample.get(), 1)));
}

TEST_F(PositionTest, ToPositionInComposedTreeWithActiveInsertionPoint)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
    const char* shadowContent = "<a id='a'><content select=#one id='content'></content><content></content></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    RefPtrWillBeRawPtr<Element> anchor = shadowRoot->getElementById("a");

    EXPECT_EQ(PositionInComposedTree(anchor.get(), 0), toPositionInComposedTree(Position(anchor.get(), 0)));
    EXPECT_EQ(PositionInComposedTree(anchor.get(), 1), toPositionInComposedTree(Position(anchor.get(), 1)));
    EXPECT_EQ(PositionInComposedTree(anchor, PositionAnchorType::AfterChildren), toPositionInComposedTree(Position(anchor.get(), 2)));
}

TEST_F(PositionTest, ToPositionInComposedTreeWithInactiveInsertionPoint)
{
    const char* bodyContent = "<p id='p'><content></content></p>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<Element> anchor = document().getElementById("p");

    EXPECT_EQ(PositionInComposedTree(anchor.get(), 0), toPositionInComposedTree(Position(anchor.get(), 0)));
    EXPECT_EQ(PositionInComposedTree(anchor, PositionAnchorType::AfterChildren), toPositionInComposedTree(Position(anchor.get(), 1)));
}

TEST_F(PositionTest, ToPositionInComposedTreeWithShadowRoot)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
    const char* shadowContent = "<a><content select=#one></content></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");

    EXPECT_EQ(PositionInComposedTree(host.get(), 0), toPositionInComposedTree(Position(shadowRoot.get(), 0)));
    EXPECT_EQ(PositionInComposedTree(host.get(), PositionAnchorType::AfterChildren), toPositionInComposedTree(Position(shadowRoot.get(), 1)));
}

TEST_F(PositionTest, ToPositionInComposedTreeWithShadowRootContainingSingleContent)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
    const char* shadowContent = "<content select=#one></content>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");

    EXPECT_EQ(PositionInComposedTree(host.get(), 0), toPositionInComposedTree(Position(shadowRoot.get(), 0)));
    EXPECT_EQ(PositionInComposedTree(host.get(), PositionAnchorType::AfterChildren), toPositionInComposedTree(Position(shadowRoot.get(), 1)));
}

TEST_F(PositionTest, ToPositionInComposedTreeWithEmptyShadowRoot)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
    const char* shadowContent = "";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");

    EXPECT_EQ(PositionInComposedTree(host, PositionAnchorType::AfterChildren), toPositionInComposedTree(Position(shadowRoot.get(), 0)));
}

TEST_F(PositionTest, upstreamAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");
    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one->firstChild(), 1), Position::afterNode(host.get()).upstream());
    EXPECT_EQ(PositionInComposedTree(three->firstChild(), 3), PositionInComposedTree::afterNode(host.get()).upstream());
}

} // namespace blink

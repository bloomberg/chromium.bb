// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingUtilities.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class EditingUtilitiesTest : public EditingTestBase {
};

TEST_F(EditingUtilitiesTest, directionOfEnclosingBlock)
{
    const char* bodyContent = "<p id='host'><b id='one'></b><b id='two'>22</b></p>";
    const char* shadowContent = "<content select=#two></content><p dir=rtl><content select=#one></content><p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Node* one = document().getElementById("one");

    EXPECT_EQ(LTR, directionOfEnclosingBlock(Position(one, 0)));
    EXPECT_EQ(RTL, directionOfEnclosingBlock(PositionInComposedTree(one, 0)));
}

TEST_F(EditingUtilitiesTest, firstEditablePositionAfterPositionInRoot)
{
    const char* bodyContent = "<p id='host' contenteditable><b id='one'>1</b><b id='two'>22</b></p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Element* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one, 0), firstEditablePositionAfterPositionInRoot(Position(one, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), firstEditableVisiblePositionAfterPositionInRoot(Position(one, 0), *host).deepEquivalent());

    EXPECT_EQ(PositionInComposedTree(one, 0), firstEditablePositionAfterPositionInRoot(PositionInComposedTree(one, 0), *host));
    EXPECT_EQ(PositionInComposedTree(two->firstChild(), 2), firstEditableVisiblePositionAfterPositionInRoot(PositionInComposedTree(one, 0), *host).deepEquivalent());

    EXPECT_EQ(Position::firstPositionInNode(host), firstEditablePositionAfterPositionInRoot(Position(three, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), firstEditableVisiblePositionAfterPositionInRoot(Position(three, 0), *host).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree::afterNode(host), firstEditablePositionAfterPositionInRoot(PositionInComposedTree(three, 0), *host));
    EXPECT_EQ(PositionInComposedTree::lastPositionInNode(host), firstEditableVisiblePositionAfterPositionInRoot(PositionInComposedTree(three, 0), *host).deepEquivalent());
}

TEST_F(EditingUtilitiesTest, enclosingBlock)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b></p>";
    const char* shadowContent = "<content select=#two></content><div id='three'><content select=#one></content></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Node* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(host, enclosingBlock(Position(one, 0), CannotCrossEditingBoundary));
    EXPECT_EQ(three, enclosingBlock(PositionInComposedTree(one, 0), CannotCrossEditingBoundary));
}

TEST_F(EditingUtilitiesTest, enclosingNodeOfType)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b></p>";
    const char* shadowContent = "<content select=#two></content><div id='three'><content select=#one></div></content>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Node* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(host, enclosingNodeOfType(Position(one, 0), isEnclosingBlock));
    EXPECT_EQ(three, enclosingNodeOfType(PositionInComposedTree(one, 0), isEnclosingBlock));
}

TEST_F(EditingUtilitiesTest, isFirstPositionAfterTable)
{
    const char* bodyContent = "<div contenteditable id=host><table id=table><tr><td>1</td></tr></table><b id=two>22</b></div>";
    const char* shadowContent = "<content select=#two></content><content select=#table></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Node* host = document().getElementById("host");
    Node* table = document().getElementById("table");

    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(Position::afterNode(table))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInComposedTree::afterNode(table))));

    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(Position::lastPositionInNode(table))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInComposedTree::lastPositionInNode(table))));

    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(Position(host, 2))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInComposedTree(host, 2))));

    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(Position::afterNode(host))));
    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(PositionInComposedTree::afterNode(host))));

    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(Position::lastPositionInNode(host))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInComposedTree::lastPositionInNode(host))));
}

TEST_F(EditingUtilitiesTest, lastEditablePositionBeforePositionInRoot)
{
    const char* bodyContent = "<p id='host' contenteditable><b id='one'>1</b><b id='two'>22</b></p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Element* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one, 0), lastEditablePositionBeforePositionInRoot(Position(one, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(Position(one, 0), *host).deepEquivalent());

    EXPECT_EQ(PositionInComposedTree(one, 0), lastEditablePositionBeforePositionInRoot(PositionInComposedTree(one, 0), *host));
    EXPECT_EQ(PositionInComposedTree(two->firstChild(), 2), lastEditableVisiblePositionBeforePositionInRoot(PositionInComposedTree(one, 0), *host).deepEquivalent());

    EXPECT_EQ(Position::firstPositionInNode(host), lastEditablePositionBeforePositionInRoot(Position(three, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(Position(three, 0), *host).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree::firstPositionInNode(host), lastEditablePositionBeforePositionInRoot(PositionInComposedTree(three, 0), *host));
    EXPECT_EQ(PositionInComposedTree(two->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(PositionInComposedTree(three, 0), *host).deepEquivalent());
}

TEST_F(EditingUtilitiesTest, NextNodeIndex)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    Node* host = document().getElementById("host");
    Node* two = document().getElementById("two");

    EXPECT_EQ(Position(host, 3), nextPositionOf(Position(two, 2), PositionMoveType::CodePoint));
    EXPECT_EQ(PositionInComposedTree(host, 1), nextPositionOf(PositionInComposedTree(two, 2), PositionMoveType::CodePoint));
}

TEST_F(EditingUtilitiesTest, NextVisuallyDistinctCandidate)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b><b id='three'>33</b></p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content><content select=#three></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = document().getElementById("three");

    EXPECT_EQ(Position(two->firstChild(), 1), nextVisuallyDistinctCandidate(Position(one, 1)));
    EXPECT_EQ(PositionInComposedTree(three->firstChild(), 1), nextVisuallyDistinctCandidate(PositionInComposedTree(one, 1)));
}

} // namespace blink

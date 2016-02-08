// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingUtilities.h"

#include "core/dom/StaticNodeList.h"
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
    EXPECT_EQ(RTL, directionOfEnclosingBlock(PositionInFlatTree(one, 0)));
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

    EXPECT_EQ(PositionInFlatTree(one, 0), firstEditablePositionAfterPositionInRoot(PositionInFlatTree(one, 0), *host));
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2), firstEditableVisiblePositionAfterPositionInRoot(PositionInFlatTree(one, 0), *host).deepEquivalent());

    EXPECT_EQ(Position::firstPositionInNode(host), firstEditablePositionAfterPositionInRoot(Position(three, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), firstEditableVisiblePositionAfterPositionInRoot(Position(three, 0), *host).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree::afterNode(host), firstEditablePositionAfterPositionInRoot(PositionInFlatTree(three, 0), *host));
    EXPECT_EQ(PositionInFlatTree::lastPositionInNode(host), firstEditableVisiblePositionAfterPositionInRoot(PositionInFlatTree(three, 0), *host).deepEquivalent());
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
    EXPECT_EQ(three, enclosingBlock(PositionInFlatTree(one, 0), CannotCrossEditingBoundary));
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
    EXPECT_EQ(three, enclosingNodeOfType(PositionInFlatTree(one, 0), isEnclosingBlock));
}

TEST_F(EditingUtilitiesTest, isEditablePositionWithTable)
{
    // We would like to have below DOM tree without HTML, HEAD and BODY element.
    //   <table id=table><caption>foo</caption></table>
    // However, |setBodyContent()| automatically creates HTML, HEAD and BODY
    // element. So, we build DOM tree manually.
    // Note: This is unusual HTML taken from http://crbug.com/574230
    RefPtrWillBeRawPtr<Element> table = document().createElement("table", ASSERT_NO_EXCEPTION);
    table->setInnerHTML("<caption>foo</caption>", ASSERT_NO_EXCEPTION);
    while (document().firstChild())
        document().firstChild()->remove();
    document().appendChild(table);
    document().setDesignMode("on");
    updateLayoutAndStyleForPainting();

    EXPECT_FALSE(isEditablePosition(Position(table.get(), 0)));
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
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInFlatTree::afterNode(table))));

    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(Position::lastPositionInNode(table))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInFlatTree::lastPositionInNode(table))));

    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(Position(host, 2))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInFlatTree(host, 2))));

    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(Position::afterNode(host))));
    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(PositionInFlatTree::afterNode(host))));

    EXPECT_EQ(nullptr, isFirstPositionAfterTable(createVisiblePosition(Position::lastPositionInNode(host))));
    EXPECT_EQ(table, isFirstPositionAfterTable(createVisiblePosition(PositionInFlatTree::lastPositionInNode(host))));
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

    EXPECT_EQ(PositionInFlatTree(one, 0), lastEditablePositionBeforePositionInRoot(PositionInFlatTree(one, 0), *host));
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2), lastEditableVisiblePositionBeforePositionInRoot(PositionInFlatTree(one, 0), *host).deepEquivalent());

    EXPECT_EQ(Position::firstPositionInNode(host), lastEditablePositionBeforePositionInRoot(Position(three, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(Position(three, 0), *host).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree::firstPositionInNode(host), lastEditablePositionBeforePositionInRoot(PositionInFlatTree(three, 0), *host));
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(PositionInFlatTree(three, 0), *host).deepEquivalent());
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
    EXPECT_EQ(PositionInFlatTree(host, 1), nextPositionOf(PositionInFlatTree(two, 2), PositionMoveType::CodePoint));
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
    EXPECT_EQ(PositionInFlatTree(three->firstChild(), 1), nextVisuallyDistinctCandidate(PositionInFlatTree(one, 1)));
}

TEST_F(EditingUtilitiesTest, AreaIdenticalElements)
{
    setBodyContent("<style>li:nth-child(even) { -webkit-user-modify: read-write; }</style><ul><li>first item</li><li>second item</li><li class=foo>third</li><li>fourth</li></ul>");
    updateLayoutAndStyleForPainting();
    RefPtrWillBeRawPtr<StaticElementList> items = document().querySelectorAll("li", ASSERT_NO_EXCEPTION);
    ASSERT(items->length() == 4);

    EXPECT_FALSE(areIdenticalElements(*items->item(0)->firstChild(), *items->item(1)->firstChild()))
        << "Can't merge non-elements.  e.g. Text nodes";

    // Compare a LI and a UL.
    EXPECT_FALSE(areIdenticalElements(*items->item(0), *items->item(0)->parentNode()))
        << "Can't merge different tag names.";

    EXPECT_FALSE(areIdenticalElements(*items->item(0), *items->item(2)))
        << "Can't merge a element with no attributes and another element with an attribute.";

    // We can't use contenteditable attribute to make editability difference
    // because the hasEquivalentAttributes check is done earier.
    EXPECT_FALSE(areIdenticalElements(*items->item(0), *items->item(1)))
        << "Can't merge non-editable nodes.";

    EXPECT_TRUE(areIdenticalElements(*items->item(1), *items->item(3)));
}

} // namespace blink

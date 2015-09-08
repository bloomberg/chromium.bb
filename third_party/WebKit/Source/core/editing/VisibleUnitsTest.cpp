// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisibleUnits.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"

namespace blink {

namespace {

PositionWithAffinity positionWithAffinityInDOMTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionWithAffinity(canonicalPositionOf(Position(&anchor, offset)), affinity);
}

VisiblePosition createVisiblePositionInDOMTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return createVisiblePosition(Position(&anchor, offset), affinity);
}

PositionInComposedTreeWithAffinity positionWithAffinityInComposedTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionInComposedTreeWithAffinity(canonicalPositionOf(PositionInComposedTree(&anchor, offset)), affinity);
}

VisiblePositionInComposedTree createVisiblePositionInComposedTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return createVisiblePosition(PositionInComposedTree(&anchor, offset), affinity);
}

} // namespace

class VisibleUnitsTest : public EditingTestBase {
};

TEST_F(VisibleUnitsTest, absoluteCaretBoundsOf)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b><b id='two'>22</b></p>";
    const char* shadowContent = "<div><content select=#two></content><content select=#one></content></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    ASSERT_UNUSED(shadowRoot, shadowRoot);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);

    IntRect boundsInDOMTree = absoluteCaretBoundsOf(createVisiblePosition(Position(one.get(), 0)));
    IntRect boundsInComposedTree = absoluteCaretBoundsOf(createVisiblePosition(PositionInComposedTree(one.get(), 0)));

    EXPECT_FALSE(boundsInDOMTree.isEmpty());
    EXPECT_EQ(boundsInDOMTree, boundsInComposedTree);
}

TEST_F(VisibleUnitsTest, characterAfter)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b><b id='two'>22</b></p><b id='three'>333</b>";
    const char* shadowContent = "<b id='four'>4444</b><content select=#two></content><content select=#one></content><b id='five'>5555</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> two = document().getElementById("two");

    EXPECT_EQ('2', characterAfter(createVisiblePositionInDOMTree(*one->firstChild(), 1)));
    EXPECT_EQ('5', characterAfter(createVisiblePositionInComposedTree(*one->firstChild(), 1)));

    EXPECT_EQ(0, characterAfter(createVisiblePositionInDOMTree(*two->firstChild(), 2)));
    EXPECT_EQ('1', characterAfter(createVisiblePositionInComposedTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, inSameLine)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<div><span id='s4'>44</span><content select=#two></content><br><span id='s5'>55</span><br><content select=#one></content><span id='s6'>66</span></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one, 0), positionWithAffinityInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*two->firstChild(), 0), positionWithAffinityInDOMTree(*four->firstChild(), 0)));

    EXPECT_TRUE(inSameLine(createVisiblePositionInDOMTree(*one, 0), createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(createVisiblePositionInDOMTree(*one->firstChild(), 0), createVisiblePositionInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInDOMTree(*one->firstChild(), 0), createVisiblePositionInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInDOMTree(*two->firstChild(), 0), createVisiblePositionInDOMTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one, 0), positionWithAffinityInComposedTree(*two, 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInComposedTree(*two->firstChild(), 0), positionWithAffinityInComposedTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(createVisiblePositionInComposedTree(*one, 0), createVisiblePositionInComposedTree(*two, 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInComposedTree(*one->firstChild(), 0), createVisiblePositionInComposedTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInComposedTree(*one->firstChild(), 0), createVisiblePositionInComposedTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(createVisiblePositionInComposedTree(*two->firstChild(), 0), createVisiblePositionInComposedTree(*four->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, localCaretRectOfPosition)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");

    LayoutObject* layoutObjectFromDOMTree;
    LayoutRect layoutRectFromDOMTree = localCaretRectOfPosition(Position(one->firstChild(), 0), layoutObjectFromDOMTree);

    LayoutObject* layoutObjectFromComposedTree;
    LayoutRect layoutRectFromComposedTree = localCaretRectOfPosition(PositionInComposedTree(one->firstChild(), 0), layoutObjectFromComposedTree);

    EXPECT_TRUE(layoutObjectFromDOMTree);
    EXPECT_FALSE(layoutRectFromDOMTree.isEmpty());
    EXPECT_EQ(layoutObjectFromDOMTree, layoutObjectFromComposedTree);
    EXPECT_EQ(layoutRectFromDOMTree, layoutRectFromComposedTree);
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");

    EXPECT_EQ(Position::lastPositionInNode(host.get()), mostForwardCaretPosition(Position::afterNode(host.get())));
    EXPECT_EQ(PositionInComposedTree::lastPositionInNode(host.get()), mostForwardCaretPosition(PositionInComposedTree::afterNode(host.get())));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");
    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one->firstChild(), 1), mostBackwardCaretPosition(Position::afterNode(host.get())));
    EXPECT_EQ(PositionInComposedTree(three->firstChild(), 3), mostBackwardCaretPosition(PositionInComposedTree::afterNode(host.get())));
}

TEST_F(VisibleUnitsTest, nextPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* zero = document().getElementById("zero");
    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");
    Element* four = shadowRoot->getElementById("four");
    Element* five = shadowRoot->getElementById("five");

    EXPECT_EQ(Position(one->firstChild(), 0), nextPositionOf(createVisiblePosition(Position(zero, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four->firstChild(), 0), nextPositionOf(createVisiblePosition(PositionInComposedTree(zero, 1))).deepEquivalent());

    EXPECT_EQ(Position(one->firstChild(), 1), nextPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInComposedTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 1), nextPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInComposedTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(three->firstChild(), 0), nextPositionOf(createVisiblePosition(Position(two, 2))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInComposedTree(two, 2))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchor)
{
    const char* bodyContent = "<p id='sample'>00</p>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();
    RefPtrWillBeRawPtr<Element> sample = document().getElementById("sample");

    EXPECT_FALSE(rendersInDifferentPosition(Position::afterNode(sample.get()), Position(sample.get(), 1)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(sample.get()), Position(sample.get(), 1)));
}

TEST_F(VisibleUnitsTest, renderedOffset)
{
    const char* bodyContent = "<div contenteditable><span id='sample1'>1</span><span id='sample2'>22</span></div>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();
    RefPtrWillBeRawPtr<Element> sample1 = document().getElementById("sample1");
    RefPtrWillBeRawPtr<Element> sample2 = document().getElementById("sample2");

    EXPECT_FALSE(rendersInDifferentPosition(Position::afterNode(sample1->firstChild()), Position(sample2->firstChild(), 0)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(sample1->firstChild()), Position(sample2->firstChild(), 0)));
}

} // namespace blink

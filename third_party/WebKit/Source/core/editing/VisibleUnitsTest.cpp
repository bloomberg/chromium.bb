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

PositionInComposedTreeWithAffinity positionWithAffinityInComposedTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionInComposedTreeWithAffinity(canonicalPositionOf(PositionInComposedTree(&anchor, offset)), affinity);
}

} // namespace

class VisibleUnitsTest : public EditingTestBase {
};

TEST_F(VisibleUnitsTest, inSameLine)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<div><span id='s4'>44</span><content select=#two></content><br><span id='s5'>55</span><br><content select=#one></content><span id='s6'>66</span></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
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

    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one, 0), positionWithAffinityInComposedTree(*two, 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInComposedTree(*two->firstChild(), 0), positionWithAffinityInComposedTree(*four->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
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
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");
    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one->firstChild(), 1), mostBackwardCaretPosition(Position::afterNode(host.get())));
    EXPECT_EQ(PositionInComposedTree(three->firstChild(), 3), mostBackwardCaretPosition(PositionInComposedTree::afterNode(host.get())));
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

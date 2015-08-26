// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/EditingUtilities.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class EditingUtilitiesTest : public EditingTestBase {
};

TEST_F(EditingUtilitiesTest, enclosingNodeOfType)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b></p>";
    const char* shadowContent = "<content select=#two></content><div id='three'><content select=#one></div></content>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);
    updateLayoutAndStyleForPainting();
    Node* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(host, enclosingNodeOfType(Position(one, 0), isBlock));
    EXPECT_EQ(three, enclosingNodeOfType(PositionInComposedTree(one, 0), isBlock));
}

TEST_F(EditingUtilitiesTest, NextNodeIndex)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent);
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
    setShadowContent(shadowContent);
    updateLayoutAndStyleForPainting();
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = document().getElementById("three");

    EXPECT_EQ(Position(two->firstChild(), 1), nextVisuallyDistinctCandidate(Position(one, 1)));
    EXPECT_EQ(PositionInComposedTree(three->firstChild(), 1), nextVisuallyDistinctCandidate(PositionInComposedTree(one, 1)));
}

} // namespace blink

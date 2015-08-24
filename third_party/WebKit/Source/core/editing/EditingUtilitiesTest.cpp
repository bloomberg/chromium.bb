// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/EditingUtilities.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class EditingUtilitiesTest : public EditingTestBase {
};

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

} // namespace blink

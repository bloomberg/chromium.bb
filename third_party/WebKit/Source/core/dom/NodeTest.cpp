// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Node.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class NodeTest : public EditingTestBase {
};

TEST_F(NodeTest, canStartSelection)
{
    const char* bodyContent = "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");

    EXPECT_FALSE(one->canStartSelection());
    EXPECT_FALSE(one->firstChild()->canStartSelection());
    EXPECT_TRUE(two->canStartSelection());
    EXPECT_TRUE(two->firstChild()->canStartSelection());
}

TEST_F(NodeTest, canStartSelectionWithShadowDOM)
{
    const char* bodyContent = "<div id=host><span id=one>one</span></div>";
    const char* shadowContent = "<a href='http://www.msn.com'><content></content></a>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();
    Node* one = document().getElementById("one");

    EXPECT_FALSE(one->canStartSelection());
    EXPECT_FALSE(one->firstChild()->canStartSelection());
}

} // namespace blink

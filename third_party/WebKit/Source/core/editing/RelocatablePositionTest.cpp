// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/RelocatablePosition.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"

namespace blink {

class RelocatablePositionTest : public EditingTestBase {
};

TEST_F(RelocatablePositionTest, position)
{
    setBodyContent("<b>foo</b><textarea>bar</textarea>");
    Node* boldface = document().querySelector("b", ASSERT_NO_EXCEPTION);
    Node* textarea = document().querySelector("textarea", ASSERT_NO_EXCEPTION);

    RelocatablePosition relocatablePosition(Position(textarea, PositionAnchorType::BeforeAnchor));
    textarea->remove();

    // RelocatablePosition should track the given Position even if its original
    // anchor node is moved away from the document.
    Position expectedPosition(boldface, PositionAnchorType::AfterAnchor);
    Position trackedPosition = relocatablePosition.position();
    EXPECT_TRUE(trackedPosition.anchorNode()->inShadowIncludingDocument());
    EXPECT_EQ(createVisiblePosition(expectedPosition).deepEquivalent(), createVisiblePosition(trackedPosition).deepEquivalent());
}

} // namespace blink

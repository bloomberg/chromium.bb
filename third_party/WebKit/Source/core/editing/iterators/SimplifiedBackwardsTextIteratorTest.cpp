// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/EphemeralRange.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SimplifiedBackwardsTextIteratorTest : public EditingTestBase {
};

template <typename Strategy>
static String extractString(const Element& element)
{
    const EphemeralRangeTemplate<Strategy> range = EphemeralRangeTemplate<Strategy>::rangeOfContents(element);
    Vector<UChar> buffer;
    for (SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(range.startPosition(), range.endPosition()); !it.atEnd(); it.advance()) {
        it.prependTextTo(buffer);
    }
    return String(buffer);
}

TEST_F(SimplifiedBackwardsTextIteratorTest, SubrangeWithReplacedElements)
{
    static const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* host = document().getElementById("host");

    // We should not apply DOM tree version to containing shadow tree in
    // general. To record current behavior, we have this test. even if it
    // isn't intuitive.
    EXPECT_EQ("onetwo", extractString<EditingStrategy>(*host));
    EXPECT_EQ("three two one zero", extractString<EditingInComposedTreeStrategy>(*host));
}

} // namespace blink

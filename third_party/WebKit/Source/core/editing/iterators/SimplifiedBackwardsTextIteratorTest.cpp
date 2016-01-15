// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

TEST_F(SimplifiedBackwardsTextIteratorTest, characterAt)
{
    const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* host = document().getElementById("host");

    EphemeralRangeTemplate<EditingStrategy> range1(EphemeralRangeTemplate<EditingStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> backIter1(range1.startPosition(), range1.endPosition());
    const char* message1 = "|backIter1| should emit 'one' and 'two' in reverse order.";
    EXPECT_EQ('o', backIter1.characterAt(0)) << message1;
    EXPECT_EQ('w', backIter1.characterAt(1)) << message1;
    EXPECT_EQ('t', backIter1.characterAt(2)) << message1;
    backIter1.advance();
    EXPECT_EQ('e', backIter1.characterAt(0)) << message1;
    EXPECT_EQ('n', backIter1.characterAt(1)) << message1;
    EXPECT_EQ('o', backIter1.characterAt(2)) << message1;

    EphemeralRangeTemplate<EditingInComposedTreeStrategy> range2(EphemeralRangeTemplate<EditingInComposedTreeStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingInComposedTreeStrategy> backIter2(range2.startPosition(), range2.endPosition());
    const char* message2 = "|backIter2| should emit 'three ', 'two', ' ', 'one' and ' zero' in reverse order.";
    EXPECT_EQ('o', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('r', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('e', backIter2.characterAt(2)) << message2;
    EXPECT_EQ('z', backIter2.characterAt(3)) << message2;
    EXPECT_EQ(' ', backIter2.characterAt(4)) << message2;
    backIter2.advance();
    EXPECT_EQ('e', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('n', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('o', backIter2.characterAt(2)) << message2;
    backIter2.advance();
    EXPECT_EQ(' ', backIter2.characterAt(0)) << message2;
    backIter2.advance();
    EXPECT_EQ('o', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('w', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('t', backIter2.characterAt(2)) << message2;
    backIter2.advance();
    EXPECT_EQ(' ', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('e', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('e', backIter2.characterAt(2)) << message2;
    EXPECT_EQ('r', backIter2.characterAt(3)) << message2;
    EXPECT_EQ('h', backIter2.characterAt(4)) << message2;
    EXPECT_EQ('t', backIter2.characterAt(5)) << message2;
}

} // namespace blink

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
        it.copyTextTo(buffer);
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

TEST_F(SimplifiedBackwardsTextIteratorTest, copyTextTo)
{
    const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* host = document().getElementById("host");
    const char* message = "|backIter%d| should have emitted '%s' in reverse order.";

    EphemeralRangeTemplate<EditingStrategy> range1(EphemeralRangeTemplate<EditingStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> backIter1(range1.startPosition(), range1.endPosition());
    Vector<UChar> output1;
    backIter1.copyTextTo(output1, 0, 2);
    EXPECT_EQ("wo", String(output1)) << String::format(message, 1, "wo").utf8().data();
    backIter1.copyTextTo(output1, 2, 1);
    EXPECT_EQ("two", String(output1)) << String::format(message, 1, "two").utf8().data();
    backIter1.advance();
    backIter1.copyTextTo(output1, 0, 1);
    EXPECT_EQ("etwo", String(output1)) << String::format(message, 1, "etwo").utf8().data();
    backIter1.copyTextTo(output1, 1, 2);
    EXPECT_EQ("onetwo", String(output1)) << String::format(message, 1, "onetwo").utf8().data();

    EphemeralRangeTemplate<EditingInComposedTreeStrategy> range2(EphemeralRangeTemplate<EditingInComposedTreeStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingInComposedTreeStrategy> backIter2(range2.startPosition(), range2.endPosition());
    Vector<UChar> output2;
    backIter2.copyTextTo(output2, 0, 2);
    EXPECT_EQ("ro", String(output2)) << String::format(message, 2, "ro").utf8().data();
    backIter2.copyTextTo(output2, 2, 3);
    EXPECT_EQ(" zero", String(output2)) << String::format(message, 2, " zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(output2, 0, 1);
    EXPECT_EQ("e zero", String(output2)) << String::format(message, 2, "e zero").utf8().data();
    backIter2.copyTextTo(output2, 1, 2);
    EXPECT_EQ("one zero", String(output2)) << String::format(message, 2, "one zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(output2, 0, 1);
    EXPECT_EQ(" one zero", String(output2)) << String::format(message, 2, " one zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(output2, 0, 2);
    EXPECT_EQ("wo one zero", String(output2)) << String::format(message, 2, "wo one zero").utf8().data();
    backIter2.copyTextTo(output2, 2, 1);
    EXPECT_EQ("two one zero", String(output2)) << String::format(message, 2, "two one zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(output2, 0, 3);
    EXPECT_EQ("ee two one zero", String(output2)) << String::format(message, 2, "ee two one zero").utf8().data();
    backIter2.copyTextTo(output2, 3, 3);
    EXPECT_EQ("three two one zero", String(output2)) << String::format(message, 2, "three two one zero").utf8().data();
}

} // namespace blink

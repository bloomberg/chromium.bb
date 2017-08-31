// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/EphemeralRange.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class SimplifiedBackwardsTextIteratorTest : public EditingTestBase {};

template <typename Strategy>
static String ExtractString(const Element& element) {
  const EphemeralRangeTemplate<Strategy> range =
      EphemeralRangeTemplate<Strategy>::RangeOfContents(element);
  BackwardsTextBuffer buffer;
  for (SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(range);
       !it.AtEnd(); it.Advance()) {
    it.CopyTextTo(&buffer);
  }
  return String(buffer.Data(), buffer.Size());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, Basic) {
  SetBodyContent("<p> [(3)]678</p>");
  const Element* const sample = GetDocument().QuerySelector("p");
  SimplifiedBackwardsTextIterator iterator(EphemeralRange(
      Position(sample->firstChild(), 0), Position(sample->firstChild(), 9)));
  // TODO(editing-dev): |SimplifiedBackwardsTextIterator| should not account
  // collapsed whitespace (http://crbug.com/760428)
  EXPECT_EQ(9, iterator.length())
      << "We should have 8 as ignoring collapsed whitespace.";
  EXPECT_EQ(Position(sample->firstChild(), 0), iterator.StartPosition());
  EXPECT_EQ(Position(sample->firstChild(), 9), iterator.EndPosition());
  EXPECT_EQ(sample->firstChild(), iterator.StartContainer());
  EXPECT_EQ(9, iterator.EndOffset());
  EXPECT_EQ(sample->firstChild(), iterator.GetNode());
  EXPECT_EQ('8', iterator.CharacterAt(0));
  EXPECT_EQ('7', iterator.CharacterAt(1));
  EXPECT_EQ('6', iterator.CharacterAt(2));
  EXPECT_EQ(']', iterator.CharacterAt(3));
  EXPECT_EQ(')', iterator.CharacterAt(4));
  EXPECT_EQ('3', iterator.CharacterAt(5));
  EXPECT_EQ('(', iterator.CharacterAt(6));
  EXPECT_EQ('[', iterator.CharacterAt(7));
  EXPECT_EQ(' ', iterator.CharacterAt(8));

  EXPECT_FALSE(iterator.AtEnd());
  iterator.Advance();
  EXPECT_TRUE(iterator.AtEnd());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, FirstLetter) {
  SetBodyContent(
      "<style>p::first-letter {font-size: 200%}</style>"
      "<p> [(3)]678</p>");
  const Element* const sample = GetDocument().QuerySelector("p");
  SimplifiedBackwardsTextIterator iterator(EphemeralRange(
      Position(sample->firstChild(), 0), Position(sample->firstChild(), 9)));
  EXPECT_EQ(3, iterator.length());
  EXPECT_EQ(Position(sample->firstChild(), 6), iterator.StartPosition());
  EXPECT_EQ(Position(sample->firstChild(), 9), iterator.EndPosition());
  EXPECT_EQ(sample->firstChild(), iterator.StartContainer());
  EXPECT_EQ(9, iterator.EndOffset());
  EXPECT_EQ(sample->firstChild(), iterator.GetNode());
  EXPECT_EQ('8', iterator.CharacterAt(0));
  EXPECT_EQ('7', iterator.CharacterAt(1));
  EXPECT_EQ('6', iterator.CharacterAt(2));

  iterator.Advance();
  // TODO(editing-dev): |SimplifiedBackwardsTextIterator| should not account
  // collapsed whitespace (http://crbug.com/760428)
  EXPECT_EQ(6, iterator.length())
      << "We should have 5 as ignoring collapsed whitespace.";
  EXPECT_EQ(Position(sample->firstChild(), 0), iterator.StartPosition());
  EXPECT_EQ(Position(sample->firstChild(), 6), iterator.EndPosition());
  EXPECT_EQ(sample->firstChild(), iterator.StartContainer());
  EXPECT_EQ(6, iterator.EndOffset());
  EXPECT_EQ(sample->firstChild(), iterator.GetNode());
  EXPECT_EQ(']', iterator.CharacterAt(0));
  EXPECT_EQ(')', iterator.CharacterAt(1));
  EXPECT_EQ('3', iterator.CharacterAt(2));
  EXPECT_EQ('(', iterator.CharacterAt(3));
  EXPECT_EQ('[', iterator.CharacterAt(4));
  EXPECT_EQ(' ', iterator.CharacterAt(5));

  EXPECT_FALSE(iterator.AtEnd());
  iterator.Advance();
  EXPECT_TRUE(iterator.AtEnd());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, SubrangeWithReplacedElements) {
  static const char* body_content =
      "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
  const char* shadow_content =
      "three <content select=#two></content> <content select=#one></content> "
      "zero";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById("host");

  // We should not apply DOM tree version to containing shadow tree in
  // general. To record current behavior, we have this test. even if it
  // isn't intuitive.
  EXPECT_EQ("onetwo", ExtractString<EditingStrategy>(*host));
  EXPECT_EQ("three two one zero",
            ExtractString<EditingInFlatTreeStrategy>(*host));
}

TEST_F(SimplifiedBackwardsTextIteratorTest, characterAt) {
  const char* body_content =
      "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
  const char* shadow_content =
      "three <content select=#two></content> <content select=#one></content> "
      "zero";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById("host");

  EphemeralRangeTemplate<EditingStrategy> range1(
      EphemeralRangeTemplate<EditingStrategy>::RangeOfContents(*host));
  SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> back_iter1(range1);
  const char* message1 =
      "|backIter1| should emit 'one' and 'two' in reverse order.";
  EXPECT_EQ('o', back_iter1.CharacterAt(0)) << message1;
  EXPECT_EQ('w', back_iter1.CharacterAt(1)) << message1;
  EXPECT_EQ('t', back_iter1.CharacterAt(2)) << message1;
  back_iter1.Advance();
  EXPECT_EQ('e', back_iter1.CharacterAt(0)) << message1;
  EXPECT_EQ('n', back_iter1.CharacterAt(1)) << message1;
  EXPECT_EQ('o', back_iter1.CharacterAt(2)) << message1;

  EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(
      EphemeralRangeTemplate<EditingInFlatTreeStrategy>::RangeOfContents(
          *host));
  SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy>
      back_iter2(range2);
  const char* message2 =
      "|backIter2| should emit 'three ', 'two', ' ', 'one' and ' zero' in "
      "reverse order.";
  EXPECT_EQ('o', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('r', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('e', back_iter2.CharacterAt(2)) << message2;
  EXPECT_EQ('z', back_iter2.CharacterAt(3)) << message2;
  EXPECT_EQ(' ', back_iter2.CharacterAt(4)) << message2;
  back_iter2.Advance();
  EXPECT_EQ('e', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('n', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('o', back_iter2.CharacterAt(2)) << message2;
  back_iter2.Advance();
  EXPECT_EQ(' ', back_iter2.CharacterAt(0)) << message2;
  back_iter2.Advance();
  EXPECT_EQ('o', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('w', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('t', back_iter2.CharacterAt(2)) << message2;
  back_iter2.Advance();
  EXPECT_EQ(' ', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('e', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('e', back_iter2.CharacterAt(2)) << message2;
  EXPECT_EQ('r', back_iter2.CharacterAt(3)) << message2;
  EXPECT_EQ('h', back_iter2.CharacterAt(4)) << message2;
  EXPECT_EQ('t', back_iter2.CharacterAt(5)) << message2;
}

TEST_F(SimplifiedBackwardsTextIteratorTest, copyTextTo) {
  const char* body_content =
      "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
  const char* shadow_content =
      "three <content select=#two></content> <content select=#one></content> "
      "zero";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById("host");
  const char* message =
      "|backIter%d| should have emitted '%s' in reverse order.";

  EphemeralRangeTemplate<EditingStrategy> range1(
      EphemeralRangeTemplate<EditingStrategy>::RangeOfContents(*host));
  SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> back_iter1(range1);
  BackwardsTextBuffer output1;
  back_iter1.CopyTextTo(&output1, 0, 2);
  EXPECT_EQ("wo", String(output1.Data(), output1.Size()))
      << String::Format(message, 1, "wo").Utf8().data();
  back_iter1.CopyTextTo(&output1, 2, 1);
  EXPECT_EQ("two", String(output1.Data(), output1.Size()))
      << String::Format(message, 1, "two").Utf8().data();
  back_iter1.Advance();
  back_iter1.CopyTextTo(&output1, 0, 1);
  EXPECT_EQ("etwo", String(output1.Data(), output1.Size()))
      << String::Format(message, 1, "etwo").Utf8().data();
  back_iter1.CopyTextTo(&output1, 1, 2);
  EXPECT_EQ("onetwo", String(output1.Data(), output1.Size()))
      << String::Format(message, 1, "onetwo").Utf8().data();

  EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(
      EphemeralRangeTemplate<EditingInFlatTreeStrategy>::RangeOfContents(
          *host));
  SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy>
      back_iter2(range2);
  BackwardsTextBuffer output2;
  back_iter2.CopyTextTo(&output2, 0, 2);
  EXPECT_EQ("ro", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "ro").Utf8().data();
  back_iter2.CopyTextTo(&output2, 2, 3);
  EXPECT_EQ(" zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, " zero").Utf8().data();
  back_iter2.Advance();
  back_iter2.CopyTextTo(&output2, 0, 1);
  EXPECT_EQ("e zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "e zero").Utf8().data();
  back_iter2.CopyTextTo(&output2, 1, 2);
  EXPECT_EQ("one zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "one zero").Utf8().data();
  back_iter2.Advance();
  back_iter2.CopyTextTo(&output2, 0, 1);
  EXPECT_EQ(" one zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, " one zero").Utf8().data();
  back_iter2.Advance();
  back_iter2.CopyTextTo(&output2, 0, 2);
  EXPECT_EQ("wo one zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "wo one zero").Utf8().data();
  back_iter2.CopyTextTo(&output2, 2, 1);
  EXPECT_EQ("two one zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "two one zero").Utf8().data();
  back_iter2.Advance();
  back_iter2.CopyTextTo(&output2, 0, 3);
  EXPECT_EQ("ee two one zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "ee two one zero").Utf8().data();
  back_iter2.CopyTextTo(&output2, 3, 3);
  EXPECT_EQ("three two one zero", String(output2.Data(), output2.Size()))
      << String::Format(message, 2, "three two one zero").Utf8().data();
}

TEST_F(SimplifiedBackwardsTextIteratorTest, CopyWholeCodePoints) {
  const char* body_content = "&#x13000;&#x13001;&#x13002; &#x13140;&#x13141;.";
  SetBodyContent(body_content);

  const UChar kExpected[] = {0xD80C, 0xDC00, 0xD80C, 0xDC01, 0xD80C, 0xDC02,
                             ' ',    0xD80C, 0xDD40, 0xD80C, 0xDD41, '.'};

  EphemeralRange range(EphemeralRange::RangeOfContents(GetDocument()));
  SimplifiedBackwardsTextIterator iter(range);
  BackwardsTextBuffer buffer;
  EXPECT_EQ(1, iter.CopyTextTo(&buffer, 0, 1))
      << "Should emit 1 UChar for '.'.";
  EXPECT_EQ(2, iter.CopyTextTo(&buffer, 1, 1))
      << "Should emit 2 UChars for 'U+13141'.";
  EXPECT_EQ(2, iter.CopyTextTo(&buffer, 3, 2))
      << "Should emit 2 UChars for 'U+13140'.";
  EXPECT_EQ(5, iter.CopyTextTo(&buffer, 5, 4))
      << "Should emit 5 UChars for 'U+13001U+13002 '.";
  EXPECT_EQ(2, iter.CopyTextTo(&buffer, 10, 2))
      << "Should emit 2 UChars for 'U+13000'.";
  for (int i = 0; i < 12; i++)
    EXPECT_EQ(kExpected[i], buffer[i]);
}

}  // namespace blink

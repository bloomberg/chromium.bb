// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/inline_autocompletion_util.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(InlineAutocompletionUtilTest, FindAtWordbreak) {
  // Should find the first wordbreak occurrence.
  EXPECT_EQ(FindAtWordbreak(base::UTF8ToUTF16("prefixmatch wordbreak_match"),
                            base::UTF8ToUTF16("match")),
            22u);

  // Should return npos when no occurrences exist.
  EXPECT_EQ(FindAtWordbreak(base::UTF8ToUTF16("prefixmatch"),
                            base::UTF8ToUTF16("match")),
            std::string::npos);

  // Should skip occurrences before |search_start|.
  EXPECT_EQ(FindAtWordbreak(base::UTF8ToUTF16("match match"),
                            base::UTF8ToUTF16("match"), 1),
            6u);
}

TEST(InlineAutocompletionUtilTest, FindWordsSequentiallyAtWordbreak) {
  using pair = std::pair<size_t, size_t>;

  // Occurrences must be sequential.
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a b c"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre(pair{0, 1}, pair{1, 2}, pair{4, 5}));
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("c b a"),
                                               base::UTF8ToUTF16("a b")),
              testing::ElementsAre());
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("b a b"),
                                               base::UTF8ToUTF16("a b")),
              testing::ElementsAre(pair{2, 3}, pair{3, 4}, pair{4, 5}));

  // Occurrences must be at word breaks.
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a b-c"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre(pair{0, 1}, pair{1, 2}, pair{4, 5}));
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a bc"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre());
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a bc c"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre(pair{0, 1}, pair{1, 2}, pair{5, 6}));

  // Whitespaces must also match
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a-c"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre());
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a c"),
                                               base::UTF8ToUTF16("a c ")),
              testing::ElementsAre());
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a -c"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre(pair{0, 1}, pair{1, 2}, pair{3, 4}));
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a- c"),
                                               base::UTF8ToUTF16("a c")),
              testing::ElementsAre(pair{0, 1}, pair{2, 3}, pair{3, 4}));
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a c c"),
                                               base::UTF8ToUTF16("a  c")),
              testing::ElementsAre());
  EXPECT_THAT(FindWordsSequentiallyAtWordbreak(base::UTF8ToUTF16("a c  c"),
                                               base::UTF8ToUTF16("a  c")),
              testing::ElementsAre(pair{0, 1}, pair{3, 5}, pair{5, 6}));
}

TEST(InlineAutocompletionUtilTest, InvertAndReverseRanges) {
  // Empty |ranges| in empty |length|.
  EXPECT_THAT(InvertAndReverseRanges(0, {}), testing::ElementsAre());
  // Empty |ranges| in non-empty |length|. 12345 -> [12345]
  EXPECT_THAT(InvertAndReverseRanges(5, {}),
              testing::ElementsAre(gfx::Range{5, 0}));
  // Single empty range in |ranges|.  12|345 -> [12345]
  EXPECT_THAT(InvertAndReverseRanges(5, {{2, 2}}),
              testing::ElementsAre(gfx::Range{5, 0}));
  // Single range in |ranges|. 12[3]45 -> [12]3[45]
  EXPECT_THAT(InvertAndReverseRanges(5, {{2, 3}}),
              testing::ElementsAre(gfx::Range{5, 3}, gfx::Range{2, 0}));
  // Single range in |ranges| spanning all of |length|. [12345] -> 12345
  EXPECT_THAT(InvertAndReverseRanges(5, {{0, 5}}), testing::ElementsAre());
  // Multiple ranges in |ranges|, including adjacent and empty ranges.
  // 1[23][45]6[78]9 -> [1]2345[6]78[9]
  EXPECT_THAT(InvertAndReverseRanges(9, {{1, 3}, {3, 5}, {6, 8}}),
              testing::ElementsAre(gfx::Range{9, 8}, gfx::Range{6, 5},
                                   gfx::Range{1, 0}));
  // |ranges| ending at |length| and starting 0.
  // [123][45]6[789] -> 12345[6]789
  EXPECT_THAT(InvertAndReverseRanges(9, {{0, 3}, {3, 5}, {6, 9}}),
              testing::ElementsAre(gfx::Range{6, 5}));
}

}  // namespace

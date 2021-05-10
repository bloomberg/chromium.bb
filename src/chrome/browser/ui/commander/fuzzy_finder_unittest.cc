// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commander {

namespace {
// Convenience function to avoid visual noise from constructing FuzzyFinder
// objects in-test.
double FuzzyFind(const base::string16& needle,
                 const base::string16& haystack,
                 std::vector<gfx::Range>* matched_ranges) {
  return FuzzyFinder(needle).Find(haystack, matched_ranges);
}
}  // namespace

TEST(CommanderFuzzyFinder, NonmatchIsZero) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16("orange"),
                         base::ASCIIToUTF16("orangutan"), &ranges));
  EXPECT_TRUE(ranges.empty());
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16("elephant"),
                         base::ASCIIToUTF16("orangutan"), &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, ExactMatchIsOne) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(1, FuzzyFind(base::ASCIIToUTF16("orange"),
                         base::ASCIIToUTF16("orange"), &ranges));
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));
}

// This ensures coverage for a fast path. Successful match is
// tested in ExactMatchIsOne() above.
TEST(CommanderFuzzyFinder, NeedleHaystackSameLength) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16("ranges"),
                         base::ASCIIToUTF16("orange"), &ranges));
  EXPECT_TRUE(ranges.empty());
}

// This ensures coverage for a fast path (just making sure the path has
// coverage rather than ensuring the path is taken).
TEST(CommanderFuzzyFinder, SingleCharNeedle) {
  std::vector<gfx::Range> ranges;
  FuzzyFinder finder(base::ASCIIToUTF16("o"));

  double prefix_score = finder.Find(base::ASCIIToUTF16("orange"), &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 1}}));
  double internal_score = finder.Find(base::ASCIIToUTF16("phone"), &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{2, 3}}));
  double boundary_score =
      finder.Find(base::ASCIIToUTF16("phone operator"), &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{6, 7}}));

  // Expected ordering:
  // - Prefix should rank highest.
  // - Word boundary matches that are not the prefix should rank next
  // highest, even if there's an internal match earlier in the haystack.
  // - Internal matches should rank lowest.
  EXPECT_GT(prefix_score, boundary_score);
  EXPECT_GT(boundary_score, internal_score);

  // ...and non-matches should have score = 0.
  EXPECT_EQ(0, finder.Find(base::ASCIIToUTF16("aquarium"), &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, CaseInsensitive) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(1, FuzzyFind(base::ASCIIToUTF16("orange"),
                         base::ASCIIToUTF16("Orange"), &ranges));
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));
}

TEST(CommanderFuzzyFinder, PrefixRanksHigherThanInternal) {
  std::vector<gfx::Range> ranges;
  FuzzyFinder finder(base::ASCIIToUTF16("orange"));
  double prefix_rank = finder.Find(base::ASCIIToUTF16("Orange juice"), &ranges);
  double non_prefix_rank =
      finder.Find(base::ASCIIToUTF16("William of Orange"), &ranges);

  EXPECT_GT(prefix_rank, 0);
  EXPECT_GT(non_prefix_rank, 0);
  EXPECT_LT(prefix_rank, 1);
  EXPECT_LT(non_prefix_rank, 1);
  EXPECT_GT(prefix_rank, non_prefix_rank);
}

TEST(CommanderFuzzyFinder, NeedleLongerThanHaystack) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16("orange juice"),
                         base::ASCIIToUTF16("orange"), &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, Noncontiguous) {
  std::vector<gfx::Range> ranges;
  EXPECT_GT(FuzzyFind(base::ASCIIToUTF16("tuot"),
                      base::UTF8ToUTF16("Tlön, Uqbar, Orbis Tertius"), &ranges),
            0);
  EXPECT_EQ(ranges,
            std::vector<gfx::Range>({{0, 1}, {6, 7}, {13, 14}, {19, 20}}));
}

TEST(CommanderFuzzyFinder, EmptyStringDoesNotMatch) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16(""), base::ASCIIToUTF16("orange"),
                         &ranges));
  EXPECT_TRUE(ranges.empty());
}

}  // namespace commander

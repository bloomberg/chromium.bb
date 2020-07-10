// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_match.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

using MatchPositions = TitledUrlMatch::MatchPositions;

TEST(TitledUrlMatchTest, EmptyOffsetsForEmptyMatchPositions) {
  auto offsets = TitledUrlMatch::OffsetsFromMatchPositions(MatchPositions());
  EXPECT_TRUE(offsets.empty());
}

TEST(TitledUrlMatchTest, OffsetsFromMatchPositions) {
  MatchPositions match_positions = {{1, 3}, {4, 5}, {10, 15}};
  std::vector<size_t> expected_offsets = {1, 3, 4, 5, 10, 15};
  auto offsets = TitledUrlMatch::OffsetsFromMatchPositions(match_positions);
  EXPECT_TRUE(
      std::equal(offsets.begin(), offsets.end(), expected_offsets.begin()));
}

TEST(TitledUrlMatchTest, ReplaceOffsetsInEmptyMatchPositions) {
  auto match_positions = TitledUrlMatch::ReplaceOffsetsInMatchPositions(
      MatchPositions(), std::vector<size_t>());
  EXPECT_TRUE(match_positions.empty());
}

TEST(TitledUrlMatchTest, ReplaceOffsetsInMatchPositions) {
  MatchPositions orig_match_positions = {{1, 3}, {4, 5}, {10, 15}};
  std::vector<size_t> offsets = {0, 2, 3, 4, 9, 14};
  MatchPositions expected_match_positions = {{0, 2}, {3, 4}, {9, 14}};
  auto match_positions = TitledUrlMatch::ReplaceOffsetsInMatchPositions(
      orig_match_positions, offsets);
  EXPECT_TRUE(std::equal(match_positions.begin(), match_positions.end(),
                         expected_match_positions.begin()));
}

TEST(TitledUrlMatchTest, ReplaceOffsetsRemovesItemsWithNposOffsets) {
  MatchPositions orig_match_positions = {{1, 3}, {4, 5}, {10, 15}, {17, 20}};
  std::vector<size_t> offsets = {0,
                                 base::string16::npos,
                                 base::string16::npos,
                                 4,
                                 base::string16::npos,
                                 base::string16::npos,
                                 17,
                                 20};
  MatchPositions expected_match_positions = {{17, 20}};
  auto match_positions = TitledUrlMatch::ReplaceOffsetsInMatchPositions(
      orig_match_positions, offsets);
  EXPECT_TRUE(std::equal(match_positions.begin(), match_positions.end(),
                         expected_match_positions.begin()));
}

}

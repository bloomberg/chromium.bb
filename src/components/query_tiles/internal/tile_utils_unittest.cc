// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_utils.h"

#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {
namespace {

// Tests that nothing happens when sorting an empty TileGroup.
TEST(TileUtilsTest, SortEmptyTileGroup) {
  TileGroup group;
  std::map<std::string, TileStats> tile_stats;
  tile_stats["guid-1-3"] = TileStats(group.last_updated_ts, 0.7);
  tile_stats["guid-1-4"] = TileStats(group.last_updated_ts, 0.4);

  SortTilesAndClearUnusedStats(&group.tiles, &tile_stats);
  EXPECT_EQ(tile_stats["guid-1-3"].score, 0.7);
  EXPECT_EQ(tile_stats["guid-1-4"].score, 0.4);
}

TEST(TileUtilsTest, Sort) {
  TileGroup group;
  test::ResetTestGroup(&group);

  SortTilesAndClearUnusedStats(&group.tiles, &group.tile_stats);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-3");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-1");
  EXPECT_EQ(group.tiles[2]->id, "guid-1-2");
  EXPECT_EQ(group.tiles[1]->sub_tiles[0]->id, "guid-2-2");
  EXPECT_EQ(group.tiles[1]->sub_tiles[1]->id, "guid-2-1");
  EXPECT_EQ(group.tiles[0]->sub_tiles[0]->id, "guid-1-4");
  EXPECT_EQ(group.tiles[1]->sub_tiles[1]->sub_tiles[0]->id, "guid-3-1");
}

TEST(TileUtilsTest, SortWithEmptytile_stats) {
  TileGroup group;
  test::ResetTestGroup(&group);

  std::map<std::string, TileStats> tile_stats;

  SortTilesAndClearUnusedStats(&group.tiles, &tile_stats);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-1");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-2");
  EXPECT_EQ(group.tiles[2]->id, "guid-1-3");
  EXPECT_EQ(group.tiles[0]->sub_tiles[0]->id, "guid-2-1");
  EXPECT_EQ(group.tiles[0]->sub_tiles[1]->id, "guid-2-2");
}

// If new tiles are at the front, tile ordering should be kept after
// sort.
TEST(TileUtilsTest, SortWithNewTilesAtTheFront) {
  TileGroup group;
  test::ResetTestGroup(&group);

  std::map<std::string, TileStats> tile_stats;
  tile_stats["guid-1-3"] = TileStats(group.last_updated_ts, 0.7);
  tile_stats["guid-1-4"] = TileStats(group.last_updated_ts, 0.4);
  tile_stats["guid-2-2"] = TileStats(group.last_updated_ts, 0.6);

  SortTilesAndClearUnusedStats(&group.tiles, &tile_stats);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-1");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-2");
  EXPECT_EQ(group.tiles[2]->id, "guid-1-3");
  EXPECT_EQ(group.tiles[0]->sub_tiles[0]->id, "guid-2-1");
  EXPECT_EQ(group.tiles[0]->sub_tiles[1]->id, "guid-2-2");
  // Front tiles should have the minimum score.
  EXPECT_EQ(tile_stats["guid-1-1"].score,
            TileConfig::GetMinimumScoreForNewFrontTiles());
  EXPECT_EQ(tile_stats["guid-1-2"].score,
            TileConfig::GetMinimumScoreForNewFrontTiles());
  EXPECT_EQ(tile_stats["guid-2-1"].score,
            TileConfig::GetMinimumScoreForNewFrontTiles());
}

// If new tiles are at the end, tile ordering should be kept after
// sort.
TEST(TileUtilsTest, SortWithNewTilesAtTheEnd) {
  TileGroup group;
  test::ResetTestGroup(&group);

  std::map<std::string, TileStats> tile_stats;
  tile_stats["guid-1-1"] = TileStats(group.last_updated_ts, 0.5);
  tile_stats["guid-1-2"] = TileStats(group.last_updated_ts, 0.2);
  tile_stats["guid-2-1"] = TileStats(group.last_updated_ts, 0.3);

  SortTilesAndClearUnusedStats(&group.tiles, &tile_stats);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-1");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-2");
  EXPECT_EQ(group.tiles[2]->id, "guid-1-3");
  EXPECT_EQ(group.tiles[0]->sub_tiles[0]->id, "guid-2-1");
  EXPECT_EQ(group.tiles[0]->sub_tiles[1]->id, "guid-2-2");
  EXPECT_EQ(tile_stats["guid-1-3"].score, 0);
  EXPECT_EQ(tile_stats["guid-2-2"].score, 0);
}

// Test the case that new tiles are in the middle.
TEST(TileUtilsTest, SortWithNewTilesInTheMiddle) {
  TileGroup group;
  test::ResetTestGroup(&group);

  std::map<std::string, TileStats> tile_stats;
  tile_stats["guid-1-1"] = TileStats(group.last_updated_ts, 0.5);
  tile_stats["guid-1-3"] = TileStats(group.last_updated_ts, 0.7);

  SortTilesAndClearUnusedStats(&group.tiles, &tile_stats);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-3");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-1");
  EXPECT_EQ(group.tiles[2]->id, "guid-1-2");
  EXPECT_EQ(tile_stats["guid-1-2"].score, 0.5);
  EXPECT_EQ(tile_stats["guid-1-2"].last_clicked_time, group.last_updated_ts);
}

// Test the case that stats for unused tiles are cleared.
TEST(TileUtilsTest, UnusedTilesCleared) {
  TileGroup group;
  test::ResetTestGroup(&group);
  std::string unsed_tile_id = "guid-x";

  std::map<std::string, TileStats> tile_stats;
  tile_stats["guid-1-1"] = TileStats(group.last_updated_ts, 0.5);
  tile_stats["guid-1-3"] = TileStats(group.last_updated_ts, 0.7);
  // Stats for a tile that is no longer used.
  tile_stats[unsed_tile_id] = TileStats(group.last_updated_ts, 0.1);

  SortTilesAndClearUnusedStats(&group.tiles, &tile_stats);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-3");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-1");
  EXPECT_EQ(group.tiles[2]->id, "guid-1-2");
  EXPECT_TRUE(tile_stats.find(unsed_tile_id) == tile_stats.end());
}

TEST(TileUtilsTest, CalculateTileScore) {
  base::Time now_time = base::Time::Now();
  EXPECT_EQ(CalculateTileScore(TileStats(now_time, 0.7), now_time), 0.7);
  EXPECT_EQ(CalculateTileScore(TileStats(now_time, 1.0),
                               now_time + base::TimeDelta::FromHours(18)),
            1.0);
  EXPECT_EQ(CalculateTileScore(TileStats(now_time, 1.0),
                               now_time + base::TimeDelta::FromDays(1)),
            exp(-0.099));
}

TEST(TileUtilsTest, IsTrendingTile) {
  EXPECT_TRUE(IsTrendingTile("trending_news"));
  EXPECT_FALSE(IsTrendingTile("Trending_news"));
  EXPECT_FALSE(IsTrendingTile("trendingnews"));
  EXPECT_FALSE(IsTrendingTile("news"));
}

}  // namespace

}  // namespace query_tiles

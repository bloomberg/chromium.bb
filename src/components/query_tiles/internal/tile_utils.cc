// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_utils.h"

#include <algorithm>
#include <limits>

#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "components/query_tiles/internal/tile_config.h"

namespace query_tiles {
namespace {

struct TileComparator {
  explicit TileComparator(std::map<std::string, double>* tile_score_map)
      : tile_score_map(tile_score_map) {}

  inline bool operator()(const std::unique_ptr<Tile>& a,
                         const std::unique_ptr<Tile>& b) {
    return (*tile_score_map)[a->id] > (*tile_score_map)[b->id];
  }

  std::map<std::string, double>* tile_score_map;
};

// Shuffle tiles from index starting with |GetTileShufflePosition()|,
// so that they have a chance to be displayed.
void ShuffleTiles(std::vector<std::unique_ptr<Tile>>* tiles,
                  std::map<std::string, double>* tile_score_map,
                  const TileShuffler& shuffler) {
  size_t starting_index = TileConfig::GetTileShufflePosition();
  if (tiles->size() <= starting_index + 1)
    return;

  shuffler.Shuffle(tiles, starting_index);
}

void SortTiles(std::vector<std::unique_ptr<Tile>>* tiles,
               std::map<std::string, TileStats>* tile_stats,
               std::map<std::string, double>* score_map,
               const TileShuffler& shuffler) {
  if (!tiles || tiles->empty())
    return;

  // Some tiles do not have scores, so the first step is to calculate scores
  // for them.
  double last_score = std::numeric_limits<double>::max();
  base::Time now_time = base::Time::Now();
  TileStats last_tile_stats(now_time, last_score);
  size_t new_tile_index = 0;
  // Find any tiles that don't have scores, and add new entries for them.
  for (size_t i = 0; i < tiles->size(); ++i) {
    auto iter = tile_stats->find((*tiles)[i]->id);
    // Found a new tile. Skip it for now, will add the entry when we found the
    // first
    if (iter == tile_stats->end())
      continue;

    double new_score = CalculateTileScore(iter->second, now_time);
    // If the previous tiles are new tiles, fill them with the same tile stats
    // from the neighbor that has the minimum score. Using the same tile stats
    // will allow tiles to have the same rate of decay over time.
    if (i > new_tile_index) {
      double min_score = std::min(new_score, last_score);
      TileStats new_stats =
          new_score > last_score ? last_tile_stats : iter->second;
      // For new tiles at the beginning, give them a score higher than the
      // minimum score, so that they have a chance to show if the top ranked
      // tiles have not been clicked recently.
      if (new_tile_index == 0) {
        double min_score_for_new_front_tiles =
            TileConfig::GetMinimumScoreForNewFrontTiles();
        if (min_score < min_score_for_new_front_tiles) {
          min_score = min_score_for_new_front_tiles;
          new_stats = TileStats(now_time, min_score);
        }
      }
      for (size_t j = new_tile_index; j < i; ++j) {
        tile_stats->emplace((*tiles)[j]->id, new_stats);
        score_map->emplace((*tiles)[j]->id, min_score);
      }
    }
    // Move |new_tile_index| to the next one that might not have
    // a score.
    new_tile_index = i + 1;
    last_score = new_score;
    last_tile_stats = iter->second;
    score_map->emplace((*tiles)[i]->id, last_score);
  }
  // Fill the new tiles at the end with 0 score.
  if (new_tile_index < tiles->size()) {
    TileStats new_stats(now_time, 0);
    for (size_t j = new_tile_index; j < tiles->size(); ++j) {
      tile_stats->emplace((*tiles)[j]->id, new_stats);
      score_map->emplace((*tiles)[j]->id, 0);
    }
  }
  // Sort the tiles in descending order.
  std::sort(tiles->begin(), tiles->end(), TileComparator(score_map));

  // Randomly shuffle tiles that are never clicked so they get a chance
  // to show up on the display.
  ShuffleTiles(tiles, score_map, shuffler);

  for (auto& tile : *tiles)
    SortTiles(&tile->sub_tiles, tile_stats, score_map, shuffler);
}

}  // namespace

void TileShuffler::Shuffle(std::vector<std::unique_ptr<Tile>>* tiles,
                           int start) const {
  base::RandomShuffle(tiles->begin() + start, tiles->end());
}

void SortTilesAndClearUnusedStats(std::vector<std::unique_ptr<Tile>>* tiles,
                                  std::map<std::string, TileStats>* tile_stats,
                                  const TileShuffler& shuffler) {
  if (!tiles || tiles->empty())
    return;
  std::map<std::string, double> score_map;
  SortTiles(tiles, tile_stats, &score_map, shuffler);
  auto iter = tile_stats->begin();
  while (iter != tile_stats->end()) {
    if (score_map.find(iter->first) == score_map.end()) {
      iter = tile_stats->erase(iter);
    } else {
      iter++;
    }
  }
}

double CalculateTileScore(const TileStats& tile_stats,
                          base::Time current_time) {
  if (tile_stats.last_clicked_time >= current_time)
    return tile_stats.score;
  // Reset the score if the tile has not been clicked for a long time.
  int days_passed_since_last_click =
      (current_time - tile_stats.last_clicked_time).InDaysFloored();
  if (days_passed_since_last_click >= TileConfig::GetNumDaysToResetTileScore())
    return 0;
  return tile_stats.score * exp(TileConfig::GetTileScoreDecayLambda() *
                                days_passed_since_last_click);
}

bool IsTrendingTile(const std::string& tile_id) {
  return base::StartsWith(tile_id, "trending_");
}

}  // namespace query_tiles

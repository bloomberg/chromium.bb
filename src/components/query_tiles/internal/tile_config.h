// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/query_tiles/internal/tile_types.h"
#include "url/gurl.h"

namespace query_tiles {

// Default URL string for GetQueryTiles RPC.
extern const char kDefaultGetQueryTilePath[];

// Finch parameter key for experiment tag to be passed to the server.
extern const char kExperimentTagKey[];

// Finch parameter key for base server URL to retrieve the tiles.
extern const char kBaseURLKey[];

// Finch parameter key for expire duration in seconds.
extern const char kExpireDurationKey[];

// Finch parameter key for expire duration in seconds.
extern const char kIsUnmeteredNetworkRequiredKey[];

// Finch parameter key for image prefetch mode.
extern const char kImagePrefetchModeKey[];

// Finch parameter key for the minimum interval to next schedule.
extern const char kNextScheduleMinIntervalKey[];

// Finch parameter key for random window.
extern const char kMaxRandomWindowKey[];

// Finch parameter key for one off task window.
extern const char kOneoffTaskWindowKey[];

class TileConfig {
 public:
  // Gets the URL for the Query Tiles server.
  static GURL GetQueryTilesServerUrl();

  // Gets whether running the background task requires unmeter network
  // condition.
  static bool GetIsUnMeteredNetworkRequired();

  // Gets the experiment tag to be passed to server.
  static std::string GetExperimentTag();

  // Gets the maximum duration for holding current group's info and images.
  static base::TimeDelta GetExpireDuration();

  // Gets the image prefetch mode to determine how many images will be
  // prefetched in background task.
  static ImagePrefetchMode GetImagePrefetchMode();

  // Get the interval of schedule window in ms.
  static int GetScheduleIntervalInMs();

  // Get the maxmium window for randomization in ms.
  static int GetMaxRandomWindowInMs();

  // Get the schedule window duration from start to end in one-off task params
  // in ms.
  static int GetOneoffTaskWindowInMs();

  // Get the init delay (unit:ms) argument for backoff policy.
  static int GetBackoffPolicyArgsInitDelayInMs();

  // Get the max delay (unit:ms) argument for backoff policy.
  static int GetBackoffPolicyArgsMaxDelayInMs();
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_

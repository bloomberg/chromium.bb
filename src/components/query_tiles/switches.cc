// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/switches.h"

namespace query_tiles {
namespace features {
const base::Feature kQueryTilesGeoFilter{"QueryTilesGeoFilter",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kQueryTiles{"QueryTiles",
                                base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kQueryTilesInOmnibox{"QueryTilesInOmnibox",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kQueryTilesEnableQueryEditing{
    "QueryTilesEnableQueryEditing", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsEnabledQueryTilesInOmnibox() {
  return base::FeatureList::IsEnabled(features::kQueryTilesGeoFilter) &&
         base::FeatureList::IsEnabled(features::kQueryTilesInOmnibox);
}

}  // namespace features

namespace switches {
const char kQueryTilesSingleTier[] = "query-tiles-single-tier";

const char kQueryTilesCountryCode[] = "query-tiles-country-code";

const char kQueryTilesInstantBackgroundTask[] =
    "query-tiles-instant-background-task";
}  // namespace switches
}  // namespace query_tiles

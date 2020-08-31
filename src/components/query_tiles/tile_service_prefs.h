// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TILE_SERVICE_PREFS_H_
#define COMPONENTS_QUERY_TILES_TILE_SERVICE_PREFS_H_

class PrefRegistrySimple;

namespace query_tiles {

// Key for query tiles backoff entry stored in pref service.
extern const char kBackoffEntryKey[];

// Register to prefs service.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_TILE_SERVICE_PREFS_H_

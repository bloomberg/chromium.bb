// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_INIT_AWARE_TILE_SERVICE_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_INIT_AWARE_TILE_SERVICE_H_

#include <deque>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/query_tiles/internal/tile_service_impl.h"
#include "components/query_tiles/tile_service.h"

namespace query_tiles {

// TileService that can cache API calls before the underlying |tile_service_| is
// initialized. After a successful initialization of |tile_service_|, all cached
// API calls will be invoked in sequence. If failed to initialize, cached API
// calls will be invoked with empty data.
class InitAwareTileService : public TileService {
 public:
  explicit InitAwareTileService(
      std::unique_ptr<InitializableTileService> tile_service);
  ~InitAwareTileService() override;

 private:
  // TileService implementation.
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void StartFetchForTiles(bool is_from_reduced_mode,
                          BackgroundTaskFinishedCallback callback) override;
  void CancelTask() override;

  void OnTileServiceInitialized(bool success);
  void MaybeCacheApiCall(base::OnceClosure api_call);

  // Returns whether |tile_service_| is successfully initialized.
  bool IsReady() const;

  // Returns whether |tile_service_| is failed to initialize.
  bool IsFailed() const;

  std::unique_ptr<InitializableTileService> tile_service_;
  std::deque<base::OnceClosure> cached_api_calls_;

  // The initialization result of |tile_service_|.
  base::Optional<bool> init_success_;

  base::WeakPtrFactory<InitAwareTileService> weak_ptr_factory_{this};
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_INIT_AWARE_TILE_SERVICE_H_

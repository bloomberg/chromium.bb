// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_iterator.h"
#include "components/query_tiles/internal/tile_manager.h"

namespace query_tiles {
namespace {

class TileManagerImpl : public TileManager {
 public:
  TileManagerImpl(std::unique_ptr<TileStore> store,
                  base::Clock* clock,
                  const std::string& accept_languages)
      : initialized_(false),
        store_(std::move(store)),
        clock_(clock),
        accept_languages_(accept_languages) {}

 private:
  // TileManager implementation.
  void Init(TileGroupStatusCallback callback) override {
    store_->InitAndLoad(base::BindOnce(&TileManagerImpl::OnTileStoreInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
  }

  void SaveTiles(std::unique_ptr<TileGroup> group,
                 TileGroupStatusCallback callback) override {
    if (!initialized_) {
      std::move(callback).Run(TileGroupStatus::kUninitialized);
      return;
    }

    auto group_copy = *group;
    store_->Update(group_copy.id, group_copy,
                   base::BindOnce(&TileManagerImpl::OnGroupSaved,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(group), std::move(callback)));
  }

  void GetTiles(GetTilesCallback callback) override {
    if (!tile_group_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::vector<Tile>()));
      return;
    }

    std::vector<Tile> tiles;
    for (const auto& tile : tile_group_->tiles)
      tiles.emplace_back(*tile.get());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(tiles)));
  }

  void GetTile(const std::string& tile_id, TileCallback callback) override {
    const Tile* result = nullptr;
    if (tile_group_) {
      TileIterator it(*tile_group_, TileIterator::kAllTiles);
      while (it.HasNext()) {
        const auto* tile = it.Next();
        DCHECK(tile);
        if (tile->id == tile_id) {
          result = tile;
          break;
        }
      }
    }

    auto result_tile = result ? base::make_optional(*result) : base::nullopt;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result_tile));
  }

  void SetAcceptLanguagesForTesting(
      const std::string& accept_languages) override {
    accept_languages_ = accept_languages;
  }

  void OnTileStoreInitialized(
      TileGroupStatusCallback callback,
      bool success,
      std::map<std::string, std::unique_ptr<TileGroup>> loaded_groups) {
    if (!success) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    TileGroupStatus::kFailureDbOperation));
      return;
    }

    initialized_ = true;

    PruneAndSelectGroup(std::move(callback), std::move(loaded_groups));
  }

  // Select the most recent unexpired group from |loaded_groups| with the
  // correct locale, and delete other groups.
  void PruneAndSelectGroup(
      TileGroupStatusCallback callback,
      std::map<std::string, std::unique_ptr<TileGroup>> loaded_groups) {
    TileGroupStatus status = TileGroupStatus::kSuccess;
    base::Time last_updated_time;
    std::string selected_group_id;
    for (const auto& pair : loaded_groups) {
      DCHECK(!pair.first.empty()) << "Should not have empty tile group key.";
      auto* group = pair.second.get();
      if (!group)
        continue;

      if (ValidateLocale(group) && !IsGroupExpired(group) &&
          (group->last_updated_ts > last_updated_time)) {
        last_updated_time = group->last_updated_ts;
        selected_group_id = pair.first;
      }
    }

    // Moves the selected group into in memory holder.
    if (!selected_group_id.empty()) {
      tile_group_ = std::move(loaded_groups[selected_group_id]);
      loaded_groups.erase(selected_group_id);
    } else {
      status = TileGroupStatus::kNoTiles;
    }

    // Deletes other groups.
    for (const auto& group_to_delete : loaded_groups)
      DeleteGroup(group_to_delete.first);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
  }

  // Returns true if the group is expired.
  bool IsGroupExpired(const TileGroup* group) const {
    return clock_->Now() >=
           group->last_updated_ts + TileConfig::GetExpireDuration();
  }

  // Check whether |locale_| matches with that of the |group|.
  bool ValidateLocale(const TileGroup* group) const {
    if (accept_languages_.empty() || group->locale.empty())
      return false;

    // In case the primary language matches (en-GB vs en-IN), consider
    // those are matching.
    std::string group_primary =
        group->locale.substr(0, group->locale.find("-"));
    for (auto& lang :
         base::SplitString(accept_languages_, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      if (lang.substr(0, lang.find("-")) == group_primary)
        return true;
    }

    return false;
  }

  void OnGroupSaved(std::unique_ptr<TileGroup> group,
                    TileGroupStatusCallback callback,
                    bool success) {
    if (!success) {
      std::move(callback).Run(TileGroupStatus::kFailureDbOperation);
      return;
    }

    // Only swap the in memory tile group when there is no existing tile group.
    if (!tile_group_)
      tile_group_ = std::move(group);

    std::move(callback).Run(TileGroupStatus::kSuccess);
  }

  void DeleteGroup(const std::string& key) {
    store_->Delete(key, base::BindOnce(&TileManagerImpl::OnGroupDeleted,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGroupDeleted(bool success) {
    // TODO(hesen): Record db operation metrics.
    NOTIMPLEMENTED();
  }

  // Indicates if the db is fully initialized, rejects calls if not.
  bool initialized_;

  // Storage layer of query tiles.
  std::unique_ptr<TileStore> store_;

  // The tile group in-memory holder.
  std::unique_ptr<TileGroup> tile_group_;

  // Clock object.
  base::Clock* clock_;

  // Accept languages from the PrefService. Used to check if tiles stored are of
  // the same language.
  std::string accept_languages_;

  base::WeakPtrFactory<TileManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

TileManager::TileManager() = default;

std::unique_ptr<TileManager> TileManager::Create(
    std::unique_ptr<TileStore> tile_store,
    base::Clock* clock,
    const std::string& locale) {
  return std::make_unique<TileManagerImpl>(std::move(tile_store), clock,
                                           locale);
}

}  // namespace query_tiles

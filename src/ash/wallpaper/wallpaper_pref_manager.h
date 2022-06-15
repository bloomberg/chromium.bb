// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_PREF_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_PREF_MANAGER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/account_id/account_id.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

class WallpaperControllerClient;

// Interface that provides user profiles from an account id. Abstracts the
// details of the PrefService from clients and makes testing easier.
class WallpaperProfileHelper {
 public:
  virtual ~WallpaperProfileHelper() = default;

  virtual void SetClient(WallpaperControllerClient* client) = 0;

  // Returns the syncable pref service for the user with |id| if it's available.
  // Otherwise, returns null.
  virtual PrefService* GetUserPrefServiceSyncable(const AccountId& id) = 0;

  // Returns the AccountId for the currently active account.
  virtual AccountId GetActiveAccountId() const = 0;

  // Returns true iff wallpaper sync is enabled for |id|.
  virtual bool IsWallpaperSyncEnabled(const AccountId& id) const = 0;

  // Returns true if at least one user is logged in.
  virtual bool IsActiveUserSessionStarted() const = 0;
};

// Manages wallpaper preferences and tracks the currently configured wallpaper.
class ASH_EXPORT WallpaperPrefManager
    : public base::SupportsWeakPtr<WallpaperPrefManager>,
      public ColorModeObserver,
      public SessionObserver {
 public:
  // Names of nodes with wallpaper info in |kUserWallpaperInfo| dictionary.
  static const char kNewWallpaperAssetIdNodeName[];
  static const char kNewWallpaperCollectionIdNodeName[];
  static const char kNewWallpaperDateNodeName[];
  static const char kNewWallpaperDedupKeyNodeName[];
  static const char kNewWallpaperLayoutNodeName[];
  static const char kNewWallpaperLocationNodeName[];
  static const char kNewWallpaperTypeNodeName[];
  static const char kNewWallpaperUnitIdNodeName[];
  static const char kNewWallpaperVariantListNodeName[];

  // Names of nodes for the online wallpaper variant dictionary.
  static const char kOnlineWallpaperTypeNodeName[];
  static const char kOnlineWallpaperUrlNodeName[];

  static std::unique_ptr<WallpaperPrefManager> Create(PrefService* local_state);

  // Create a PrefManager where pref service retrieval can be modified through
  // |profile_helper|.
  static std::unique_ptr<WallpaperPrefManager> CreateForTesting(
      PrefService* local_state,
      std::unique_ptr<WallpaperProfileHelper> profile_helper);

  WallpaperPrefManager(const WallpaperPrefManager&) = delete;

  ~WallpaperPrefManager() override = default;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  virtual void SetClient(WallpaperControllerClient* client) = 0;

  // Retrieve the wallpaper preference value for |account_id| and use it to
  // populate |info|. If |ephemeral| is true, data is stored in an in memory
  // map. If |ephemeral| is false, |info| is persisted to user prefs. Returns
  // true if |info| was populated successfully.
  //
  // NOTE: WallpaperPrefManager does not enforce any checks for policy
  // enforcement. Callers must check that the user is allowed to commit the pref
  // change.
  //
  // TODO(crbug.com/1329567): Remove the |ephemeral| parameter when we've safely
  // moved to SessionController for user type.
  virtual bool GetUserWallpaperInfo(const AccountId& account_id,
                                    bool ephemeral,
                                    WallpaperInfo* info) const = 0;
  virtual bool SetUserWallpaperInfo(const AccountId& account_id,
                                    bool ephemeral,
                                    const WallpaperInfo& info) = 0;

  // Remove the wallpaper entry for |account_id|.
  virtual void RemoveUserWallpaperInfo(const AccountId& account_id) = 0;

  virtual void CacheProminentColors(const AccountId& account_id,
                                    const std::vector<SkColor>& colors) = 0;

  virtual void RemoveProminentColors(const AccountId& account_id) = 0;

  virtual absl::optional<std::vector<SkColor>> GetCachedColors(
      const AccountId& account_id) const = 0;

  virtual bool SetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      const WallpaperController::DailyGooglePhotosIdCache& ids) = 0;
  virtual bool GetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      WallpaperController::DailyGooglePhotosIdCache& ids_out) const = 0;

  // Get/Set the wallpaper info in local storage.
  // TODO(crbug.com/1298586): Remove these functions from the interface when
  // this can be abstracted from callers.
  virtual bool GetLocalWallpaperInfo(const AccountId& account_id,
                                     WallpaperInfo* info) const = 0;
  virtual bool SetLocalWallpaperInfo(const AccountId& account_id,
                                     const WallpaperInfo& info) = 0;

  // Get/Set the wallpaper info from synced prefs.
  // TODO(crbug.com/1298586): Remove these functions from the interface when
  // this can be abstracted from callers.
  virtual bool GetSyncedWallpaperInfo(const AccountId& account_id,
                                      WallpaperInfo* info) const = 0;
  virtual bool SetSyncedWallpaperInfo(const AccountId& account_id,
                                      const WallpaperInfo& info) = 0;

 protected:
  WallpaperPrefManager() = default;
};

}  // namespace ash

#endif  //  ASH_WALLPAPER_WALLPAPER_PREF_MANAGER_H_

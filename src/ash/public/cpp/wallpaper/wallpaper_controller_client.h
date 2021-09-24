// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace ash {

// Used by ash to control a Chrome client of the WallpaperController.
class ASH_PUBLIC_EXPORT WallpaperControllerClient {
 public:
  // Opens the wallpaper picker window.
  virtual void OpenWallpaperPicker() = 0;

  // Closes the app side of the wallpaper preview (top header bar) if it is
  // currently open.
  virtual void MaybeClosePreviewWallpaper() = 0;

  // Sets the default wallpaper and removes the file for the previous wallpaper.
  virtual void SetDefaultWallpaper(const AccountId& account_id,
                                   bool show_wallpaper) = 0;

  // Retrieves the current collection id from the Wallpaper Picker Chrome App
  // for migration.
  virtual void MigrateCollectionIdFromChromeApp(
      const AccountId& account_id) = 0;

  // Downloads and sets a new random wallpaper from the collection of the
  // specified collection_id.
  using DailyWallpaperUrlFetchedCallback =
      base::OnceCallback<void(const absl::optional<uint64_t>& asset_id,
                              const std::string& url)>;
  virtual void FetchDailyRefreshWallpaper(
      const std::string& collection_id,
      DailyWallpaperUrlFetchedCallback callback) = 0;

  // Returns true if image was successfully saved.
  virtual bool SaveWallpaperToDriveFs(const AccountId& account_id,
                                      const base::FilePath& origin) = 0;

  virtual base::FilePath GetWallpaperPathFromDriveFs(
      const AccountId& account_id) = 0;

  virtual void GetFilesId(
      const AccountId& account_id,
      base::OnceCallback<void(const std::string&)> files_id_callback) const = 0;

  virtual bool IsWallpaperSyncEnabled(const AccountId& account_id) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_H_

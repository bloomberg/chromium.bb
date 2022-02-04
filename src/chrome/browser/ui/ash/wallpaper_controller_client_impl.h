// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_IMPL_H_

#include <memory>

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "url/gurl.h"

class AccountId;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace value_store {
class ValueStore;
}

// Handles chrome-side wallpaper control alongside the ash-side controller.
class WallpaperControllerClientImpl
    : public ash::WallpaperControllerClient,
      public file_manager::VolumeManagerObserver,
      public session_manager::SessionManagerObserver {
 public:
  WallpaperControllerClientImpl();

  WallpaperControllerClientImpl(const WallpaperControllerClientImpl&) = delete;
  WallpaperControllerClientImpl& operator=(
      const WallpaperControllerClientImpl&) = delete;

  ~WallpaperControllerClientImpl() override;

  // Initializes and connects to ash.
  void Init();

  // Tests can provide a mock interface for the ash controller.
  void InitForTesting(ash::WallpaperController* controller);

  // Sets the initial wallpaper. Should be called after the session manager has
  // been initialized.
  void SetInitialWallpaper();

  static WallpaperControllerClientImpl* Get();

  // ash::WallpaperControllerClient:
  void OpenWallpaperPicker() override;
  void MaybeClosePreviewWallpaper() override;
  void SetDefaultWallpaper(const AccountId& account_id,
                           bool show_wallpaper) override;
  void MigrateCollectionIdFromChromeApp(const AccountId& account_id) override;
  void FetchDailyRefreshWallpaper(
      const std::string& collection_id,
      DailyWallpaperUrlFetchedCallback callback) override;
  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;
  void SaveWallpaperToDriveFs(
      const AccountId& account_id,
      const base::FilePath& origin,
      base::OnceCallback<void(bool)> wallpaper_saved_callback) override;
  base::FilePath GetWallpaperPathFromDriveFs(
      const AccountId& account_id) override;
  void GetFilesId(const AccountId& account_id,
                  base::OnceCallback<void(const std::string&)>
                      files_id_callback) const override;
  bool IsWallpaperSyncEnabled(const AccountId& account_id) const override;

  // file_manager::VolumeManagerObserver:
  void OnVolumeMounted(chromeos::MountError error_code,
                       const file_manager::Volume& volume) override;

  // session_manager::SessionManagerObserver implementation.
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // Wrappers around the ash::WallpaperController interface.
  void SetCustomWallpaper(const AccountId& account_id,
                          const std::string& file_name,
                          ash::WallpaperLayout layout,
                          const gfx::ImageSkia& image,
                          bool preview_mode);
  void SetOnlineWallpaper(
      const ash::OnlineWallpaperParams& params,
      ash::WallpaperController::SetOnlineWallpaperCallback callback);
  void SetOnlineWallpaperIfExists(
      const ash::OnlineWallpaperParams& params,
      ash::WallpaperController::SetOnlineWallpaperCallback callback);
  void SetOnlineWallpaperFromData(
      const ash::OnlineWallpaperParams& params,
      const std::string& image_data,
      ash::WallpaperController::SetOnlineWallpaperCallback callback);
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path);
  void SetPolicyWallpaper(const AccountId& account_id,
                          std::unique_ptr<std::string> data);
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& file_name,
                              ash::WallpaperLayout layout,
                              const gfx::ImageSkia& image);
  void ConfirmPreviewWallpaper();
  void CancelPreviewWallpaper();
  void UpdateCustomWallpaperLayout(const AccountId& account_id,
                                   ash::WallpaperLayout layout);
  void ShowUserWallpaper(const AccountId& account_id);
  void ShowSigninWallpaper();
  void ShowAlwaysOnTopWallpaper(const base::FilePath& image_path);
  void RemoveAlwaysOnTopWallpaper();
  void RemoveUserWallpaper(const AccountId& account_id);
  void RemovePolicyWallpaper(const AccountId& account_id);
  void GetOfflineWallpaperList(
      ash::WallpaperController::GetOfflineWallpaperListCallback callback);
  void SetAnimationDuration(const base::TimeDelta& animation_duration);
  void OpenWallpaperPickerIfAllowed();
  void MinimizeInactiveWindows(const std::string& user_id_hash);
  void RestoreMinimizedWindows(const std::string& user_id_hash);
  void AddObserver(ash::WallpaperControllerObserver* observer);
  void RemoveObserver(ash::WallpaperControllerObserver* observer);
  gfx::ImageSkia GetWallpaperImage();
  const std::vector<SkColor>& GetWallpaperColors();
  bool IsWallpaperBlurred();
  bool IsActiveUserWallpaperControlledByPolicy();
  ash::WallpaperInfo GetActiveUserWallpaperInfo();
  bool ShouldShowWallpaperSetting();
  void MigrateCollectionIdFromValueStoreForTesting(
      const AccountId& account_id,
      value_store::ValueStore* storage);
  // Record Ash.Wallpaper.Source metric when a new wallpaper is set,
  // either by built-in Wallpaper app or a third party extension/app.
  void RecordWallpaperSourceUMA(const ash::WallpaperType type);

 private:
  // Initialize the controller for this client and some wallpaper directories.
  void InitController();

  // Shows the wallpaper of the first user in |UserManager::GetUsers|, or a
  // default signin wallpaper if there's no user. This ensures the wallpaper is
  // shown right after boot, regardless of when the login screen is available.
  void ShowWallpaperOnLoginScreen();

  void DeviceWallpaperImageFilePathChanged();

  // Returns true if user names should be shown on the login screen.
  bool ShouldShowUserNamesOnLogin() const;

  base::FilePath GetDeviceWallpaperImageFilePath();

  // Used as callback to |MigrateCollectionIdFromChromeApp|. Called on backend
  // task runner. Extracts the daily refresh collection id and calls
  // |SetDailyRefreshCollectionId| on main task runner.
  void OnGetWallpaperChromeAppValueStore(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      const AccountId& account_id,
      value_store::ValueStore* value_store);

  // Passes |collection_id| to wallpaper controller on main task runner.
  void SetDailyRefreshCollectionId(const AccountId& account_id,
                                   const std::string& collection_id);

  void OnDailyImageInfoFetched(DailyWallpaperUrlFetchedCallback callback,
                               bool success,
                               const backdrop::Image& image,
                               const std::string& next_resume_token);
  void OnFetchImagesForCollection(
      FetchImagesForCollectionCallback callback,
      std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher> fetcher,
      bool success,
      const std::string& collection_id,
      const std::vector<backdrop::Image>& images);

  void ObserveVolumeManagerForAccountId(const AccountId& account_id);

  // WallpaperController interface in ash.
  ash::WallpaperController* wallpaper_controller_;

  PrefService* local_state_;

  // The registrar used to watch DeviceWallpaperImageFilePath pref changes.
  PrefChangeRegistrar pref_registrar_;

  // Subscription for a callback that monitors if user names should be shown on
  // the login screen, which determines whether a user wallpaper or a default
  // wallpaper should be shown.
  base::CallbackListSubscription show_user_names_on_signin_subscription_;

  std::unique_ptr<wallpaper_handlers::BackdropSurpriseMeImageFetcher>
      surprise_me_image_fetcher_;

  base::ScopedMultiSourceObservation<file_manager::VolumeManager,
                                     file_manager::VolumeManagerObserver>
      volume_manager_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::WeakPtrFactory<WallpaperControllerClientImpl> weak_factory_{this};
  base::WeakPtrFactory<WallpaperControllerClientImpl> storage_weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_IMPL_H_

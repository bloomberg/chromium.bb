// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// Simulates WallpaperController in ash.
class TestWallpaperController : public ash::WallpaperController {
 public:
  TestWallpaperController();

  TestWallpaperController(const TestWallpaperController&) = delete;
  TestWallpaperController& operator=(const TestWallpaperController&) = delete;

  ~TestWallpaperController() override;

  // Simulates showing the wallpaper on screen by updating |current_wallpaper|
  // and notifying the observers.
  void ShowWallpaperImage(const gfx::ImageSkia& image);

  void ClearCounts();
  bool was_client_set() const { return was_client_set_; }
  int remove_user_wallpaper_count() const {
    return remove_user_wallpaper_count_;
  }
  int set_default_wallpaper_count() const {
    return set_default_wallpaper_count_;
  }
  int set_custom_wallpaper_count() const { return set_custom_wallpaper_count_; }
  int set_online_wallpaper_count() const { return set_online_wallpaper_count_; }
  int show_always_on_top_wallpaper_count() const {
    return show_always_on_top_wallpaper_count_;
  }
  int remove_always_on_top_wallpaper_count() const {
    return remove_always_on_top_wallpaper_count_;
  }
  const std::string& collection_id() const { return collection_id_; }
  const absl::optional<ash::WallpaperInfo>& wallpaper_info() const {
    return wallpaper_info_;
  }

  // ash::WallpaperController:
  void SetClient(ash::WallpaperControllerClient* client) override;
  void Init(const base::FilePath& user_data,
            const base::FilePath& wallpapers,
            const base::FilePath& custom_wallpapers,
            const base::FilePath& device_policy_wallpaper) override;
  void SetCustomWallpaper(const AccountId& account_id,
                          const base::FilePath& file_path,
                          ash::WallpaperLayout layout,
                          bool preview_mode,
                          SetCustomWallpaperCallback callback) override;
  void SetCustomWallpaper(const AccountId& account_id,
                          const std::string& file_name,
                          ash::WallpaperLayout layout,
                          const gfx::ImageSkia& image,
                          bool preview_mode) override;
  void SetOnlineWallpaper(const ash::OnlineWallpaperParams& params,
                          SetOnlineWallpaperCallback callback) override;
  void SetOnlineWallpaperIfExists(const ash::OnlineWallpaperParams& params,
                                  SetOnlineWallpaperCallback callback) override;
  void SetOnlineWallpaperFromData(const ash::OnlineWallpaperParams& params,
                                  const std::string& image_data,
                                  SetOnlineWallpaperCallback callback) override;
  void SetDefaultWallpaper(const AccountId& account_id,
                           bool show_wallpaper) override;
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path) override;
  void SetPolicyWallpaper(const AccountId& account_id,
                          const std::string& data) override;
  void SetDevicePolicyWallpaperPath(
      const base::FilePath& device_policy_wallpaper_path) override;
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& file_name,
                              ash::WallpaperLayout layout,
                              const gfx::ImageSkia& image) override;
  void ConfirmPreviewWallpaper() override;
  void CancelPreviewWallpaper() override;
  void UpdateCustomWallpaperLayout(const AccountId& account_id,
                                   ash::WallpaperLayout layout) override;
  void ShowUserWallpaper(const AccountId& account_id) override;
  void ShowSigninWallpaper() override;
  void ShowOneShotWallpaper(const gfx::ImageSkia& image) override;
  void ShowAlwaysOnTopWallpaper(const base::FilePath& image_path) override;
  void RemoveAlwaysOnTopWallpaper() override;
  void RemoveUserWallpaper(const AccountId& account_id) override;
  void RemovePolicyWallpaper(const AccountId& account_id) override;
  void GetOfflineWallpaperList(
      GetOfflineWallpaperListCallback callback) override;
  void SetAnimationDuration(base::TimeDelta animation_duration) override;
  void OpenWallpaperPickerIfAllowed() override;
  void MinimizeInactiveWindows(const std::string& user_id_hash) override;
  void RestoreMinimizedWindows(const std::string& user_id_hash) override;
  void AddObserver(ash::WallpaperControllerObserver* observer) override;
  void RemoveObserver(ash::WallpaperControllerObserver* observer) override;
  gfx::ImageSkia GetWallpaperImage() override;
  const std::vector<SkColor>& GetWallpaperColors() override;
  bool IsWallpaperBlurredForLockState() const override;
  bool IsActiveUserWallpaperControlledByPolicy() override;
  ash::WallpaperInfo GetActiveUserWallpaperInfo() override;
  bool ShouldShowWallpaperSetting() override;
  void SetDailyRefreshCollectionId(const AccountId& account_id,
                                   const std::string& collection_id) override;
  std::string GetDailyRefreshCollectionId(
      const AccountId& account_id) const override;
  void UpdateDailyRefreshWallpaper(RefreshWallpaperCallback callback) override;
  void SyncLocalAndRemotePrefs(const AccountId& account_id) override;

 private:
  bool was_client_set_ = false;
  int remove_user_wallpaper_count_ = 0;
  int set_default_wallpaper_count_ = 0;
  int set_custom_wallpaper_count_ = 0;
  int set_online_wallpaper_count_ = 0;
  int show_always_on_top_wallpaper_count_ = 0;
  int remove_always_on_top_wallpaper_count_ = 0;
  std::string collection_id_;
  absl::optional<ash::WallpaperInfo> wallpaper_info_;

  base::ObserverList<ash::WallpaperControllerObserver>::Unchecked observers_;

  gfx::ImageSkia current_wallpaper;
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_

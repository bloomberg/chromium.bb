// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_wallpaper_controller.h"

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/notreached.h"
#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

TestWallpaperController::TestWallpaperController() = default;

TestWallpaperController::~TestWallpaperController() = default;

void TestWallpaperController::ShowWallpaperImage(const gfx::ImageSkia& image) {
  current_wallpaper = image;
  for (auto& observer : observers_)
    observer.OnWallpaperChanged();
}

void TestWallpaperController::ClearCounts() {
  set_online_wallpaper_count_ = 0;
  remove_user_wallpaper_count_ = 0;
  collection_id_ = std::string();
  wallpaper_info_ = absl::nullopt;
}

void TestWallpaperController::SetClient(
    ash::WallpaperControllerClient* client) {
  was_client_set_ = true;
}

void TestWallpaperController::Init(
    const base::FilePath& user_data,
    const base::FilePath& wallpapers,
    const base::FilePath& custom_wallpapers,
    const base::FilePath& device_policy_wallpaper) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetCustomWallpaper(
    const AccountId& account_id,
    const base::FilePath& file_path,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SetCustomWallpaperCallback callback) {
  ++set_custom_wallpaper_count_;
  std::move(callback).Run(true);
}

void TestWallpaperController::SetCustomWallpaper(const AccountId& account_id,
                                                 const std::string& file_name,
                                                 ash::WallpaperLayout layout,
                                                 const gfx::ImageSkia& image,
                                                 bool preview_mode) {
  ++set_custom_wallpaper_count_;
}

void TestWallpaperController::SetOnlineWallpaper(
    const ash::OnlineWallpaperParams& params,
    SetOnlineWallpaperCallback callback) {
  ++set_online_wallpaper_count_;
  wallpaper_info_ = ash::WallpaperInfo(params);
  std::move(callback).Run(/*success=*/true);
}

void TestWallpaperController::SetOnlineWallpaperIfExists(
    const ash::OnlineWallpaperParams& params,
    SetOnlineWallpaperCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetOnlineWallpaperFromData(
    const ash::OnlineWallpaperParams& params,
    const std::string& image_data,
    SetOnlineWallpaperCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetDefaultWallpaper(const AccountId& account_id,
                                                  bool show_wallpaper) {
  ++set_default_wallpaper_count_;
}

void TestWallpaperController::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetPolicyWallpaper(const AccountId& account_id,
                                                 const std::string& data) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetDevicePolicyWallpaperPath(
    const base::FilePath& device_policy_wallpaper_path) {
  NOTIMPLEMENTED();
}

bool TestWallpaperController::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  ShowWallpaperImage(image);
  return true;
}

void TestWallpaperController::ConfirmPreviewWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::CancelPreviewWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::UpdateCustomWallpaperLayout(
    const AccountId& account_id,
    ash::WallpaperLayout layout) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowUserWallpaper(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowSigninWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowOneShotWallpaper(
    const gfx::ImageSkia& image) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowAlwaysOnTopWallpaper(
    const base::FilePath& image_path) {
  ++show_always_on_top_wallpaper_count_;
}

void TestWallpaperController::RemoveAlwaysOnTopWallpaper() {
  ++remove_always_on_top_wallpaper_count_;
}

void TestWallpaperController::RemoveUserWallpaper(const AccountId& account_id) {
  ++remove_user_wallpaper_count_;
}

void TestWallpaperController::RemovePolicyWallpaper(
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::GetOfflineWallpaperList(
    GetOfflineWallpaperListCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::OpenWallpaperPickerIfAllowed() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::AddObserver(
    ash::WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void TestWallpaperController::RemoveObserver(
    ash::WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::ImageSkia TestWallpaperController::GetWallpaperImage() {
  return current_wallpaper;
}

const std::vector<SkColor>& TestWallpaperController::GetWallpaperColors() {
  NOTIMPLEMENTED();
  static std::vector<SkColor> kColors;
  return kColors;
}

bool TestWallpaperController::IsWallpaperBlurredForLockState() const {
  NOTIMPLEMENTED();
  return false;
}

bool TestWallpaperController::IsActiveUserWallpaperControlledByPolicy() {
  NOTIMPLEMENTED();
  return false;
}

ash::WallpaperInfo TestWallpaperController::GetActiveUserWallpaperInfo() {
  return wallpaper_info_.value_or(ash::WallpaperInfo());
}

bool TestWallpaperController::ShouldShowWallpaperSetting() {
  NOTIMPLEMENTED();
  return false;
}

void TestWallpaperController::SetDailyRefreshCollectionId(
    const AccountId& account_id,
    const std::string& collection_id) {
  collection_id_ = collection_id;
}

std::string TestWallpaperController::GetDailyRefreshCollectionId(
    const AccountId& account_id) const {
  return collection_id_;
}

void TestWallpaperController::UpdateDailyRefreshWallpaper(
    RefreshWallpaperCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SyncLocalAndRemotePrefs(
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_wallpaper_image_handler.h"

#include <utility>

#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

DeviceWallpaperImageHandler::DeviceWallpaperImageHandler(
    PrefService* local_state,
    PolicyService* policy_service)
    : local_state_(local_state),
      device_wallpaper_image_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceWallpaperImage,
              this)) {}

DeviceWallpaperImageHandler::~DeviceWallpaperImageHandler() = default;

// static
void DeviceWallpaperImageHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceWallpaperImageFilePath,
                               std::string());
}

void DeviceWallpaperImageHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  local_state_->SetString(prefs::kDeviceWallpaperImageFilePath, std::string());
}

void DeviceWallpaperImageHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  local_state_->SetString(prefs::kDeviceWallpaperImageFilePath,
                          file_path.value());
}

void DeviceWallpaperImageHandler::Shutdown() {
  device_wallpaper_image_observer_.reset();
}

}  // namespace policy

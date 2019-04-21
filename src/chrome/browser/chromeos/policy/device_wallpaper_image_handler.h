// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WALLPAPER_IMAGE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WALLPAPER_IMAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_cloud_external_data_policy_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

class PolicyService;

// This class observes the device setting "DeviceWallpaperImage", and sets
// "policy.device_wallpaper_image_file_path" pref appropriately based on the
// file path with fetched wallpaper image.
class DeviceWallpaperImageHandler final
    : public DeviceCloudExternalDataPolicyObserver::Delegate {
 public:
  DeviceWallpaperImageHandler(PrefService* local_state,
                              PolicyService* policy_service);
  ~DeviceWallpaperImageHandler() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DeviceCloudExternalDataPolicyObserver::Delegate:
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;

  void Shutdown();

 private:
  PrefService* const local_state_;

  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_wallpaper_image_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceWallpaperImageHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WALLPAPER_IMAGE_HANDLER_H_

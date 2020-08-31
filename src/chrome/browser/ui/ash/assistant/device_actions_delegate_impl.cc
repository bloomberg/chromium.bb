// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

using chromeos::assistant::mojom::AppStatus;

DeviceActionsDelegateImpl::DeviceActionsDelegateImpl() = default;

DeviceActionsDelegateImpl::~DeviceActionsDelegateImpl() = default;

AppStatus DeviceActionsDelegateImpl::GetAndroidAppStatus(
    const std::string& package_name) {
  const auto* prefs =
      ArcAppListPrefs::Get(ProfileManager::GetActiveUserProfile());
  if (!prefs) {
    LOG(ERROR) << "ArcAppListPrefs is not available.";
    return AppStatus::UNKNOWN;
  }
  std::string app_id = prefs->GetAppIdByPackageName(package_name);

  return app_id.empty() ? AppStatus::UNAVAILABLE : AppStatus::AVAILABLE;
}

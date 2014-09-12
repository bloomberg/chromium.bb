// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace app_list {
namespace switches {

// If set, the app info context menu item is not available in the app list UI.
const char kDisableAppInfo[] = "disable-app-list-app-info";

// If set, Drive apps will not be shown side-by-side with Chrome apps.
const char kDisableDriveAppsInAppList[] = "disable-drive-apps-in-app-list";

// Disables syncing of the app list independent of extensions.
const char kDisableSyncAppList[] = "disable-sync-app-list";

// If set, the app list will be centered and wide instead of tall.
const char kEnableCenteredAppList[] = "enable-centered-app-list";

// If set, the experimental app list will be used. Implies
// --enable-centered-app-list.
const char kEnableExperimentalAppList[] = "enable-experimental-app-list";

// Enables syncing of the app list independent of extensions.
const char kEnableSyncAppList[] = "enable-sync-app-list";

bool IsAppListSyncEnabled() {
#if defined(TOOLKIT_VIEWS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableSyncAppList);
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableSyncAppList);
#endif
}

bool IsFolderUIEnabled() {
#if !defined(TOOLKIT_VIEWS)
  return false;  // Folder UI not implemented for Cocoa.
#endif
  // Folder UI is available only when AppList sync is enabled, and should
  // not be disabled separately.
  return IsAppListSyncEnabled();
}

bool IsVoiceSearchEnabled() {
  // Speech recognition in AppList is only for ChromeOS right now.
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool IsAppInfoEnabled() {
#if defined(TOOLKIT_VIEWS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableAppInfo);
#else
  return false;
#endif
}

bool IsExperimentalAppListEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableExperimentalAppList);
}

bool IsCenteredAppListEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableCenteredAppList) ||
         IsExperimentalAppListEnabled();
}

bool IsDriveAppsInAppListEnabled() {
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableDriveAppsInAppList);
#else
  return false;
#endif
}

}  // namespace switches
}  // namespace app_list

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace app_list {
namespace switches {

// Disables syncing of the app list independent of extensions.
const char kDisableSyncAppList[] = "disable-sync-app-list";

// If set, the voice search is disabled in app list UI.
const char kDisableVoiceSearch[] = "disable-app-list-voice-search";

// If set, the app info context menu item is available in the app list UI.
const char kEnableAppInfo[] = "enable-app-list-app-info";

// If set, the experimental app list will be used.
const char kEnableExperimentalAppList[] = "enable-experimental-app-list";

// If set, the experimental app list position will be used.
const char kEnableExperimentalAppListPosition[] =
    "enable-experimental-app-list-position";

// If set, it will always listen to the audio locally and open the app-list
// when the hotword is recognized.
const char kEnableHotwordAlwaysOn[] = "enable-app-list-hotword-always-on";

// Enables syncing of the app list independent of extensions.
const char kEnableSyncAppList[] = "enable-sync-app-list";

bool IsAppListSyncEnabled() {
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableSyncAppList);
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableSyncAppList);
#endif
}

bool IsFolderUIEnabled() {
#if defined(OS_MACOSX)
  return false;  // Folder UI not implemented for OSX
#endif
  // Folder UI is available only when AppList sync is enabled, and should
  // not be disabled separately.
  return IsAppListSyncEnabled();
}

bool IsVoiceSearchEnabled() {
  // Speech recognition in AppList is only for ChromeOS right now.
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableVoiceSearch);
#else
  return false;
#endif
}

bool IsHotwordAlwaysOnEnabled() {
#if defined(OS_CHROMEOS)
  return IsVoiceSearchEnabled() &&
      CommandLine::ForCurrentProcess()->HasSwitch(kEnableHotwordAlwaysOn);
#else
  return false;
#endif
}

bool IsAppInfoEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableAppInfo);
}

bool IsExperimentalAppListEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableExperimentalAppList);
}

bool IsExperimentalAppListPositionEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
             kEnableExperimentalAppListPosition) ||
         IsExperimentalAppListEnabled();
}

}  // namespace switches
}  // namespace app_list

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace app_list {
namespace switches {

// If set, the experimental app list will be used.
const char kEnableExperimentalAppList[] = "enable-experimental-app-list";

// If set, folder will be disabled in app list UI.
const char kDisableFolderUI[] = "disable-app-list-folder-ui";

// If set, the voice search is disabled in app list UI.
const char kDisableVoiceSearch[] = "disable-app-list-voice-search";

// If set, the app info context menu item is available in the app list UI.
const char kEnableAppInfo[] = "enable-app-list-app-info";

// Folder UI is enabled by default.
bool IsFolderUIEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableFolderUI);
}

bool IsVoiceSearchEnabled() {
  // Speech recognition in AppList is only for ChromeOS right now.
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableVoiceSearch);
#else
  return false;
#endif
}

bool IsAppInfoEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableAppInfo);
}

}  // namespcae switches
}  // namespace app_list

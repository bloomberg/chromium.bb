// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_SWITCHES_H_
#define UI_APP_LIST_APP_LIST_SWITCHES_H_

#include "build/build_config.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {
namespace switches {

// Please keep these flags sorted (but keep enable/disable pairs together).
APP_LIST_EXPORT extern const char kCustomLauncherPage[];
APP_LIST_EXPORT extern const char kDisableAppListDismissOnBlur[];
APP_LIST_EXPORT extern const char kEnableAppList[];
APP_LIST_EXPORT extern const char kEnableSyncAppList[];
APP_LIST_EXPORT extern const char kDisableSyncAppList[];
APP_LIST_EXPORT extern const char kEnableDriveSearchInChromeLauncher[];
APP_LIST_EXPORT extern const char kDisableDriveSearchInChromeLauncher[];
APP_LIST_EXPORT extern const char kResetAppListInstallState[];

bool APP_LIST_EXPORT IsAppListSyncEnabled();

bool APP_LIST_EXPORT IsFolderUIEnabled();

bool APP_LIST_EXPORT IsVoiceSearchEnabled();

// Determines whether the app list should not be dismissed on focus loss.
bool APP_LIST_EXPORT ShouldNotDismissOnBlur();

bool APP_LIST_EXPORT IsDriveAppsInAppListEnabled();

bool APP_LIST_EXPORT IsDriveSearchInChromeLauncherEnabled();

}  // namespace switches
}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_SWITCHES_H_

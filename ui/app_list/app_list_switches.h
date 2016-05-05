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
APP_LIST_EXPORT extern const char kEnableCenteredAppList[];
APP_LIST_EXPORT extern const char kEnableExperimentalAppList[];
APP_LIST_EXPORT extern const char kDisableExperimentalAppList[];
APP_LIST_EXPORT extern const char kEnableNewAppListMixer[];
APP_LIST_EXPORT extern const char kDisableNewAppListMixer[];
APP_LIST_EXPORT extern const char kEnableSyncAppList[];
APP_LIST_EXPORT extern const char kDisableSyncAppList[];
APP_LIST_EXPORT extern const char kEnableDriveSearchInChromeLauncher[];
APP_LIST_EXPORT extern const char kDisableDriveSearchInChromeLauncher[];
APP_LIST_EXPORT extern const char kResetAppListInstallState[];

bool APP_LIST_EXPORT IsAppListSyncEnabled();

bool APP_LIST_EXPORT IsFolderUIEnabled();

bool APP_LIST_EXPORT IsVoiceSearchEnabled();

bool APP_LIST_EXPORT IsExperimentalAppListEnabled();

// Determines whether either command-line switch was given for enabling the
// centered app list position. Do not use this when positioning the app list;
// instead use AppListViewDelegate::ShouldCenterWindow. It checks a superset of
// the conditions that trigger the position.
bool APP_LIST_EXPORT IsCenteredAppListEnabled();

// Determines whether the app list should not be dismissed on focus loss.
bool APP_LIST_EXPORT ShouldNotDismissOnBlur();

bool APP_LIST_EXPORT IsDriveAppsInAppListEnabled();

bool APP_LIST_EXPORT IsDriveSearchInChromeLauncherEnabled();

}  // namespace switches
}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_SWITCHES_H_

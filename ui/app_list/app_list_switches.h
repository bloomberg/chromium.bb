// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_SWITCHES_H_
#define UI_APP_LIST_APP_LIST_SWITCHES_H_

#include "ui/app_list/app_list_export.h"

namespace app_list {
namespace switches {

// Please keep these flags sorted.
APP_LIST_EXPORT extern const char kDisableSyncAppList[];
APP_LIST_EXPORT extern const char kDisableVoiceSearch[];
APP_LIST_EXPORT extern const char kEnableAppInfo[];
APP_LIST_EXPORT extern const char kEnableExperimentalAppList[];
APP_LIST_EXPORT extern const char kEnableExperimentalAppListPosition[];
APP_LIST_EXPORT extern const char kEnableHotwordAlwaysOn[];
APP_LIST_EXPORT extern const char kEnableSyncAppList[];

bool APP_LIST_EXPORT IsAppListSyncEnabled();

bool APP_LIST_EXPORT IsFolderUIEnabled();

bool APP_LIST_EXPORT IsVoiceSearchEnabled();

bool APP_LIST_EXPORT IsAppInfoEnabled();

bool APP_LIST_EXPORT IsExperimentalAppListEnabled();

bool APP_LIST_EXPORT IsExperimentalAppListPositionEnabled();

}  // namespace switches
}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_SWITCHES_H_

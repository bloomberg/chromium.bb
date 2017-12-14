// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_FEATURES_H_
#define UI_APP_LIST_APP_LIST_FEATURES_H_

#include <string>

#include "ui/app_list/app_list_export.h"

namespace base {
struct Feature;
}

namespace app_list {
namespace features {

// Please keep these features sorted.

// Enables the answer card in the app list.
APP_LIST_EXPORT extern const base::Feature kEnableAnswerCard;

// Enables background blur for the app list, lock screen, and tab switcher, also
// enables the AppsGridView mask layer. In this mode, slower devices may have
// choppier app list animations. crbug.com/765292.
APP_LIST_EXPORT extern const base::Feature kEnableBackgroundBlur;

// Enables the Play Store app search.
APP_LIST_EXPORT extern const base::Feature kEnablePlayStoreAppSearch;

bool APP_LIST_EXPORT IsAnswerCardEnabled();
bool APP_LIST_EXPORT IsBackgroundBlurEnabled();
bool APP_LIST_EXPORT IsFullscreenAppListEnabled();
bool APP_LIST_EXPORT IsPlayStoreAppSearchEnabled();
bool APP_LIST_EXPORT IsAppListFocusEnabled();
std::string APP_LIST_EXPORT AnswerServerUrl();
std::string APP_LIST_EXPORT AnswerServerQuerySuffix();

}  // namespace features
}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FEATURES_H_

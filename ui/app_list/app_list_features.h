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

// Enables the fullscreen app list.
APP_LIST_EXPORT extern const base::Feature kEnableFullscreenAppList;

bool APP_LIST_EXPORT IsAnswerCardEnabled();
bool APP_LIST_EXPORT IsFullscreenAppListEnabled();
std::string APP_LIST_EXPORT AnswerServerUrl();

}  // namespace features
}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FEATURES_H_

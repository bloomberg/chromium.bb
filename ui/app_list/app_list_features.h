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

// Enables the dark run answer card in the app list. In this mode, answer cards
// may be loaded via mock URLs and are not shown to the user.
APP_LIST_EXPORT extern const base::Feature kEnableAnswerCardDarkRun;

// Enables the fullscreen app list.
APP_LIST_EXPORT extern const base::Feature kEnableFullscreenAppList;

bool APP_LIST_EXPORT IsAnswerCardEnabled();
bool APP_LIST_EXPORT IsAnswerCardDarkRunEnabled();
bool APP_LIST_EXPORT IsFullscreenAppListEnabled();
int APP_LIST_EXPORT AnswerCardMaxWidth();
int APP_LIST_EXPORT AnswerCardMaxHeight();
std::string APP_LIST_EXPORT AnswerServerUrl();

}  // namespace features
}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FEATURES_H_

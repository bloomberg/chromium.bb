// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_

#include "base/feature_list.h"

namespace messages {

// Feature that controls whether Messages for Android infrastucture components
// are initialized. When this feature is disabled all individual message
// implementations also fallback to Infobar implementations.
extern const base::Feature kMessagesForAndroidInfrastructure;

// Feature that controls whether "save password" and "saved password
// confirmation" prompts use Messages or Infobars infrastructure.
extern const base::Feature kMessagesForAndroidPasswords;

// Feature that controls whether "popup blocked" prompts use Messages or
// Infobars infrastructure.
extern const base::Feature kMessagesForAndroidPopupBlocked;

// Feature that controls whether "reader mode" prompts use Messages or
// Infobars infrastructure.
extern const base::Feature kMessagesForAndroidReaderMode;

// Feature that controls whether "safety tip" prompts use Messages or
// Infobars infrastructure.
extern const base::Feature kMessagesForAndroidSafetyTip;

// Feature that controls whether "save card" prompts use Messages or
// Infobars infrastructure.
extern const base::Feature kMessagesForAndroidSaveCard;

// Feature that controls whether "update password" prompt uses Messages or
// Infobars infrastructure.
extern const base::Feature kMessagesForAndroidUpdatePassword;

bool IsPasswordMessagesUiEnabled();

bool IsPopupBlockedMessagesUiEnabled();

bool IsSafetyTipMessagesUiEnabled();

bool IsSaveCardMessagesUiEnabled();

bool IsUpdatePasswordMessagesUiEnabled();

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_

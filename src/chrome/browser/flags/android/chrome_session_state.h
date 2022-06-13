// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_

#include <jni.h>

namespace chrome {
namespace android {

enum CustomTabsVisibilityHistogram {
  VISIBLE_CUSTOM_TAB,
  VISIBLE_CHROME_TAB,
  kMaxValue = VISIBLE_CHROME_TAB,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class ActivityType {
  kTabbed,
  kCustomTab,
  kTrustedWebActivity,
  kWebapp,
  kWebApk,
  kMaxValue = kWebApk,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class DarkModeState {
  kUnknown,
  // Both system and browser are in dark mode.
  kDarkModeSystem,
  // Browser is in dark mode, system is not/cannot be determined.
  kDarkModeApp,
  // Both system and browser are in light mode.
  kLightModeSystem,
  // Browser is in light mode, system is not/cannot be determined.
  kLightModeApp,
  kMaxValue = kLightModeApp,
};

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue(ActivityType type);

ActivityType GetActivityType();

DarkModeState GetDarkModeState();

bool GetIsInMultiWindowModeValue();

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_

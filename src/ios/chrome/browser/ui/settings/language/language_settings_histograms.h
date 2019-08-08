// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_HISTOGRAMS_H_

// UMA histogram names. These constants are repeated in LanguagesManager.java.
const char kLanguageSettingsPageImpressionHistogram[] =
    "LanguageSettings.PageImpression";
const char kLanguageSettingsActionsHistogram[] = "LanguageSettings.Actions";

// Enum for the LanguageSettings.PageImpression histogram. These constants are
// repeated in LanguagesManager.java.
// Note: This enum is append-only. Keep in sync with "LanguageSettingsPageType"
// in src/tools/metrics/histograms/enums.xml.
enum class LanguageSettingsPages {
  PAGE_MAIN = 0,
  PAGE_ADD_LANGUAGE = 1,
  PAGE_LANGUAGE_DETAILS = 2,
  kMaxValue = PAGE_LANGUAGE_DETAILS,
};

// Enum for the LanguageSettings.Actions histogram. These constants are repeated
// in LanguagesManager.java.
// Note: This enum is append-only. Keep in sync with
// "LanguageSettingsActionType" in src/tools/metrics/histograms/enums.xml.
enum class LanguageSettingsActions {
  UNKNOWN = 0,  // Never logged.
  CLICK_ON_ADD_LANGUAGE = 1,
  LANGUAGE_ADDED = 2,
  LANGUAGE_REMOVED = 3,
  DISABLE_TRANSLATE_GLOBALLY = 4,
  ENABLE_TRANSLATE_GLOBALLY = 5,
  DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 6,
  ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 7,
  LANGUAGE_LIST_REORDERED = 8,
  kMaxValue = LANGUAGE_LIST_REORDERED,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_HISTOGRAMS_H_

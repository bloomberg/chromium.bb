// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_PREF_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_PREF_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "url/gurl.h"

class HostContentSettingsMap;

// Retrieves/records the ammount of times the user has dismissed intent picker
// UI for a given origin.
class IntentPickerAutoDisplayPref final {
 public:
  IntentPickerAutoDisplayPref(const GURL& origin,
                              HostContentSettingsMap* settings);
  ~IntentPickerAutoDisplayPref();

  void IncrementCounter();

  bool HasExceededThreshold();

 private:
  // Creates and keep track of the dictionary for this specific origin.
  IntentPickerAutoDisplayPref(const GURL& origin,
                              std::unique_ptr<base::DictionaryValue> pref_dict);

  int QueryDismissedCounter();

  void SetDismissedCounter(int new_counter);

  void Commit();

  // Origin associated to this preference.
  GURL origin_;

  int ui_dismissed_counter_;

  // Dictionary for this particular preference.
  std::unique_ptr<base::DictionaryValue> pref_dict_;

  // Content settings map used to persist the local values.
  HostContentSettingsMap* settings_map_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerAutoDisplayPref);
};

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_PREF_H_

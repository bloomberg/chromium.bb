// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_CHROME_VERSION_SERVICE_H_
#define CHROME_BROWSER_PROFILES_CHROME_VERSION_SERVICE_H_

#include <string>

#include "base/macros.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// This service manages a pref which is used to determine the version of
// Chrome by which the profile was created.
class ChromeVersionService {
 public:
  // Register the user pref we use.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Sets the version string in the pref for the current profile.
  static void SetVersion(PrefService* prefs, const std::string& version);

  // Returns the version of Chrome which created the profile.
  static std::string GetVersion(PrefService* prefs);

  // Handles setting the profile.created_by_version Pref
  static void OnProfileLoaded(PrefService* prefs, bool is_new_profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeVersionService);
};

#endif  // CHROME_BROWSER_PROFILES_CHROME_VERSION_SERVICE_H_

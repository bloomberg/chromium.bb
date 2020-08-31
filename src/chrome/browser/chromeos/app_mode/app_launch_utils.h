// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_APP_LAUNCH_UTILS_H_

#include <string>

class PrefService;
class Profile;

namespace chromeos {

// Attempts to launch the app given by |app_id| in app mode
// or exit on failure. This function will not show any launch UI
// during the launch. Use StartupAppLauncher for finer control
// over the app launch processes.
void LaunchAppOrDie(Profile* profile, const std::string& app_id);

// Removes obsolete preferences left out by previous user session;
void ResetEphemeralKioskPreferences(PrefService* prefs);
// Replace the list of preferences which are reset in tests.
void SetEphemeralKioskPreferencesListForTesting(std::vector<std::string>*);

}  // namespace chromeos

#endif // CHROME_BROWSER_CHROMEOS_APP_MODE_APP_LAUNCH_UTILS_H_
